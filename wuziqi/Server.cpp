/**
 * Server.cpp
 *
 * 五子棋游戏服务端核心实现
 *
 * 架构说明：
 *   - acceptThread  : 专门监听并接受新客户端连接，将新 fd 注册到 epoll
 *   - epollThread   : 用 epoll_wait 持续监听所有客户端 fd 的可读事件，
 *                     有数据到达时向 TaskQueue 提交读取任务（非阻塞）
 *   - workerThread  : 多个工作线程从 TaskQueue 取任务，读取 socket 数据、
 *                     拼接粘包缓冲区、调用 processMessage 处理业务逻辑
 *   - matchThread   : 定期检查匹配队列，凑成对后建立游戏房间
 *   - heartbeatThread: 定期检查超时客户端并断开
 *
 * 锁规则（防止死锁）：
 *   - clients_mutex_  : 只在读写 clients_ / session_to_fd_ 时持有，持有期间
 *                       不调用任何可能再次加锁的函数（包括 sendToClient）
 *   - rooms_mutex_    : 只在读写 rooms_ 时持有
 *   - queue_mutex_    : 只在读写 waiting_queue_ 时持有
 *   - 三把锁的获取顺序固定为：clients_mutex_ → rooms_mutex_ → queue_mutex_
 *     如需同时持有，必须按此顺序获取，防止死锁
 *
 * 发送策略：
 *   - sendToClientUnlocked : 调用方已持有 clients_mutex_ 时使用
 *   - sendToClient         : 内部自行加锁，在无锁上下文中使用
 */

#include "Server.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <random>
#include <stdexcept>

#ifndef _WIN32
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

// =========================================================
// 构造 / 析构
// =========================================================

Server::Server(int port, Database* db)
    : port_(port)
    , db_(db)
    , running_(false)
#ifndef _WIN32
    , epoll_fd_(-1)
#endif
{}

Server::~Server() {
    stop();
}

// =========================================================
// start / stop
// =========================================================

bool Server::start() {
    // 初始化 socket 系统（Windows 需要 WSAStartup，Linux 无操作）
    if (!TcpSocket::initialize()) {
        LOG_ERROR("Socket 系统初始化失败");
        return false;
    }

    // 创建监听 socket
    if (!server_socket_.create()) {
        LOG_ERROR("创建服务器 Socket 失败");
        return false;
    }
    server_socket_.setReuseAddr(true);
    server_socket_.setNonBlocking(true);

    if (!server_socket_.bind("", port_)) {
        LOG_ERROR("绑定端口失败: " + std::to_string(port_));
        return false;
    }
    if (!server_socket_.listen(128)) {
        LOG_ERROR("监听失败: " + std::to_string(port_));
        return false;
    }

#ifndef _WIN32
    // 创建 epoll 实例
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        LOG_ERROR("epoll_create1 失败");
        return false;
    }
#endif

    running_ = true;

    // 启动各线程
    accept_thread_    = std::thread(&Server::acceptThread, this);
    epoll_thread_     = std::thread(&Server::epollThread, this);
    match_thread_     = std::thread(&Server::matchThread, this);
    heartbeat_thread_ = std::thread(&Server::heartbeatThread, this);

    for (int i = 0; i < WORKER_THREADS; ++i) {
        worker_threads_.emplace_back(&Server::workerThread, this, i);
    }

    LOG_INFO("服务器启动成功，监听端口: " + std::to_string(port_));
    return true;
}

void Server::stop() {
    if (!running_) return;
    running_ = false;

    // 停止任务队列，唤醒所有工作线程
    task_queue_.stop();

    // 等待所有线程结束
    if (accept_thread_.joinable())    accept_thread_.join();
    if (epoll_thread_.joinable())     epoll_thread_.join();
    if (match_thread_.joinable())     match_thread_.join();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    for (auto& t : worker_threads_) {
        if (t.joinable()) t.join();
    }

#ifndef _WIN32
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
#endif

    server_socket_.close();

    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& pair : clients_) {
            if (pair.second.socket) {
                pair.second.socket->close();
            }
        }
        clients_.clear();
        session_to_fd_.clear();
    }

    LOG_INFO("服务器已完全停止");
    TcpSocket::cleanup();
}

// =========================================================
// epoll 辅助函数
// =========================================================

bool Server::epollAdd(int fd) {
#ifndef _WIN32
    struct epoll_event ev;
    ev.events  = EPOLLIN | EPOLLET; // 边缘触发，可读事件
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LOG_ERROR("epoll_ctl ADD 失败 fd=" + std::to_string(fd));
        return false;
    }
    return true;
#else
    (void)fd;
    return true;
#endif
}

bool Server::epollDel(int fd) {
#ifndef _WIN32
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        // 允许失败（fd 可能已关闭）
        return false;
    }
    return true;
#else
    (void)fd;
    return true;
#endif
}

// =========================================================
// acceptThread：接受新连接
// =========================================================

void Server::acceptThread() {
    LOG_INFO("acceptThread 启动");
    while (running_) {
        std::string client_ip;
        int client_port = 0;
        TcpSocket* client_socket = server_socket_.accept(client_ip, client_port);

        if (client_socket) {
            // 设置非阻塞，以便 epoll 边缘触发正确工作
            client_socket->setNonBlocking(true);
            int fd = client_socket->getHandle();

            // 注册到 epoll（先注册再加入 map，防止事件到达时找不到 session）
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                ClientSession session;
                session.fd           = fd;
                session.socket       = std::unique_ptr<TcpSocket>(client_socket);
                session.ip_address   = client_ip;
                session.port         = client_port;
                session.status       = STATUS_CONNECTED;
                session.session_id   = "";
                session.player_id    = -1;
                session.username     = "";
                session.room_id      = -1;
                session.color        = EMPTY;
                session.receive_buffer = "";
                session.last_active  = std::chrono::steady_clock::now();
                session.active       = true;
                clients_[fd]         = std::move(session);
            }

            // 将新 fd 加入 epoll 监听
            if (!epollAdd(fd)) {
                LOG_ERROR("epoll 注册失败，移除客户端 fd=" + std::to_string(fd));
                removeClient(fd, false);
            } else {
                LOG_INFO("新客户端连接: " + client_ip + ":" + std::to_string(client_port)
                         + " fd=" + std::to_string(fd));
            }
        }

        // 非阻塞 accept 在无连接时会返回 nullptr，短暂休眠避免忙等
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    LOG_INFO("acceptThread 退出");
}

// =========================================================
// epollThread：监听所有 fd 的可读事件（Linux/麒麟专用）
// =========================================================

void Server::epollThread() {
    LOG_INFO("epollThread 启动（epoll 驱动）");

#ifndef _WIN32
    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (running_) {
        // epoll_wait 超时 200ms，以便定期检查 running_ 标志
        int n = epoll_wait(epoll_fd_, events, EPOLL_MAX_EVENTS, 200);
        if (n < 0) {
            if (errno == EINTR) continue; // 被信号中断，正常
            LOG_ERROR("epoll_wait 错误: " + std::string(strerror(errno)));
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                // 连接异常或断开
                LOG_INFO("epoll 检测到连接断开 fd=" + std::to_string(fd));
                removeClient(fd);
                continue;
            }

            if (events[i].events & EPOLLIN) {
                // 有数据可读，提交到工作线程队列
                // 用 lambda 捕获 fd，在工作线程中读取数据
                task_queue_.push([this, fd]() {
                    dispatchClientData(fd);
                });
            }
        }
    }
#else
    // Windows 回退方案：轮询所有客户端（不需要 epoll）
    while (running_) {
        std::vector<int> fds;
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (auto& pair : clients_) {
                if (pair.second.active) {
                    fds.push_back(pair.first);
                }
            }
        }
        for (int fd : fds) {
            task_queue_.push([this, fd]() {
                dispatchClientData(fd);
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
#endif

    LOG_INFO("epollThread 退出");
}

// =========================================================
// workerThread：从任务队列取任务执行
// =========================================================

void Server::workerThread(int thread_id) {
    LOG_INFO("workerThread[" + std::to_string(thread_id) + "] 启动");
    std::function<void()> task;
    while (task_queue_.pop(task)) {
        try {
            task();
        } catch (const std::exception& e) {
            LOG_ERROR("workerThread[" + std::to_string(thread_id) + "] 任务异常: "
                      + std::string(e.what()));
        }
    }
    LOG_INFO("workerThread[" + std::to_string(thread_id) + "] 退出");
}

// =========================================================
// dispatchClientData：读取 socket 数据，处理粘包，提交消息
// 注意：此函数可能被多个工作线程并发调用，通过 clients_mutex_ 保护共享状态
// =========================================================

void Server::dispatchClientData(int fd) {
    // ---- 第一步：读取数据，追加到缓冲区 ----
    std::string new_data;
    ssize_t bytes_read = 0;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it == clients_.end()) return;

        ClientSession& session = it->second;
        if (!session.socket || !session.socket->isValid()) {
            removeClientUnlocked(fd);
            return;
        }

        // 循环读取直到 EAGAIN（边缘触发必须读完）
        std::string buf;
#ifndef _WIN32
        while (true) {
            ssize_t n = session.socket->recv(buf, 4096);
            if (n > 0) {
                session.receive_buffer += buf;
                session.last_active = std::chrono::steady_clock::now();
                bytes_read += n;
                buf.clear();
            } else if (n == 0) {
                // 对端关闭连接
                LOG_INFO("客户端断开: " + session.ip_address + ":" + std::to_string(session.port));
                removeClientUnlocked(fd);
                return;
            } else {
                // EAGAIN / EWOULDBLOCK：数据读完
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                // 其他错误
                LOG_ERROR("recv 错误 fd=" + std::to_string(fd)
                          + " errno=" + std::to_string(errno));
                removeClientUnlocked(fd);
                return;
            }
        }
#else
        ssize_t n = session.socket->recv(buf, 4096);
        if (n > 0) {
            session.receive_buffer += buf;
            session.last_active = std::chrono::steady_clock::now();
            bytes_read = n;
        } else if (n == 0) {
            removeClientUnlocked(fd);
            return;
        } else {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                removeClientUnlocked(fd);
            }
            return;
        }
#endif
        // 如果本次没读到任何数据，直接返回
        if (bytes_read == 0) return;

        // ---- 第二步：从缓冲区提取完整消息行（以 \n 为分隔符）----
        // 将完整消息收集起来，在锁释放后处理（避免在锁内执行业务逻辑）
        new_data = session.receive_buffer;
        session.receive_buffer.clear();
    }

    // ---- 第三步：在锁外解析并处理消息（防止死锁）----
    std::string remaining;
    size_t pos = 0;
    while (pos < new_data.size()) {
        size_t newline = new_data.find('\n', pos);
        if (newline == std::string::npos) {
            // 不完整的消息，保留到缓冲区
            remaining = new_data.substr(pos);
            break;
        }
        std::string line = new_data.substr(pos, newline - pos);
        // 去除 \r（Windows 客户端可能发送 \r\n）
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            processMessage(fd, line);
        }
        pos = newline + 1;
    }

    // 将剩余不完整数据放回缓冲区
    if (!remaining.empty()) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it != clients_.end()) {
            it->second.receive_buffer = remaining + it->second.receive_buffer;
        }
    }
}

// =========================================================
// processMessage：解析命令并路由
// =========================================================

void Server::processMessage(int fd, const std::string& message) {
    std::vector<std::string> args = splitMessage(message);
    if (args.empty()) return;

    const std::string& command = args[0];
    LOG_DEBUG("收到命令 fd=" + std::to_string(fd) + " cmd=" + message);

    if      (command == "LOGIN")      handleLogin(fd, args);
    else if (command == "REGISTER")   handleRegister(fd, args);
    else if (command == "MATCH")      handleMatch(fd, args);
    else if (command == "MOVE")       handleMove(fd, args);
    else if (command == "SURRENDER")  handleSurrender(fd, args);
    else if (command == "QUIT")       handleQuit(fd, args);
    else if (command == "STATS")      handleStats(fd, args);
    else if (command == "REPLAY")     handleReplay(fd, args);
    else if (command == "HEARTBEAT")  handleHeartbeat(fd, args);
    else {
        sendToClient(fd, "ERROR Unknown command: " + command);
        LOG_WARN("未知命令 fd=" + std::to_string(fd) + " cmd=" + command);
    }
}

// =========================================================
// sendToClient（有锁版本）
// =========================================================

void Server::sendToClient(int fd, const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    sendToClientUnlocked(fd, message);
}

// =========================================================
// sendToClientUnlocked（无锁版本，调用方已持有 clients_mutex_）
// =========================================================

void Server::sendToClientUnlocked(int fd, const std::string& message) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) return;
    if (it->second.socket && it->second.socket->isValid()) {
        std::string data = message + "\n";
        ssize_t n = it->second.socket->send(data);
        if (n < 0) {
            LOG_WARN("sendToClientUnlocked 发送失败 fd=" + std::to_string(fd));
        }
    }
}

// =========================================================
// removeClient（有锁版本）
// =========================================================

void Server::removeClient(int fd, bool notify_opponent) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    removeClientUnlocked(fd, notify_opponent);
}

// =========================================================
// removeClientUnlocked（无锁版本，调用方已持有 clients_mutex_）
// =========================================================

void Server::removeClientUnlocked(int fd, bool notify_opponent) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) return;

    ClientSession& session = it->second;

    // 从 epoll 移除
    epollDel(fd);

    // 删除数据库会话记录
    if (!session.session_id.empty()) {
        db_->deleteSession(session.session_id);
        session_to_fd_.erase(session.session_id);
    }

    // 从匹配队列移除
    if (session.status == STATUS_WAITING) {
        std::lock_guard<std::mutex> q_lock(queue_mutex_);
        std::queue<int> new_q;
        while (!waiting_queue_.empty()) {
            int f = waiting_queue_.front();
            waiting_queue_.pop();
            if (f != fd) new_q.push(f);
        }
        waiting_queue_ = new_q;
    }

    // 通知对手
    if (session.room_id != -1 && notify_opponent) {
        int room_id = session.room_id;
        PlayerColor color = session.color;
        // 在锁外通知（先记录后处理）
        // 使用临时变量避免持有两把锁
        std::lock_guard<std::mutex> r_lock(rooms_mutex_);
        auto room_it = rooms_.find(room_id);
        if (room_it != rooms_.end() && room_it->second.active) {
            room_it->second.active = false;
            // 找到对手 fd 并通知
            const std::string& opp_session =
                (color == BLACK) ? room_it->second.white_session_id
                                 : room_it->second.black_session_id;
            auto opp_it = session_to_fd_.find(opp_session);
            if (opp_it != session_to_fd_.end()) {
                int opp_fd = opp_it->second;
                sendToClientUnlocked(opp_fd, "OPPONENT_DISCONNECTED");
                // 更新对手状态
                auto opp_session_it = clients_.find(opp_fd);
                if (opp_session_it != clients_.end()) {
                    opp_session_it->second.status  = STATUS_LOGGED_IN;
                    opp_session_it->second.room_id = -1;
                    opp_session_it->second.color   = EMPTY;
                }
            }
            // 更新数据库比赛结果
            GameResult result = (color == BLACK) ? WHITE_WIN : BLACK_WIN;
            db_->updateMatchResult(room_it->second.match_id, result);
        }
    }

    // 关闭 socket
    if (session.socket) {
        session.socket->close();
    }

    LOG_INFO("客户端已移除: " + session.ip_address + ":" + std::to_string(session.port)
             + " fd=" + std::to_string(fd));
    clients_.erase(it);
}

// =========================================================
// handleRegister：注册新用户
// =========================================================

bool Server::handleRegister(int fd, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        sendToClient(fd, "ERROR Invalid arguments for REGISTER");
        return false;
    }

    const std::string& username = args[1];
    const std::string& password = args[2];

    // 用户名长度校验
    if (username.size() < 2 || username.size() > 20) {
        sendToClient(fd, "ERROR Username length must be 2-20 characters");
        return false;
    }
    // 密码长度校验
    if (password.size() < 4) {
        sendToClient(fd, "ERROR Password must be at least 4 characters");
        return false;
    }

    // 检查用户名是否已存在（数据库操作，在锁外执行）
    Player* existing = db_->getPlayerByUsername(username);
    if (existing) {
        delete existing;
        sendToClient(fd, "ERROR Username already exists");
        LOG_WARN("注册失败 - 用户名已存在: " + username);
        return false;
    }

    // 对密码做 SHA-256 哈希后再存库（固定盐=""，与登录一致）
    std::string hashed_pw = db_->hashPassword(password, "");
    if (db_->registerPlayer(username, hashed_pw)) {
        sendToClient(fd, "REGISTER_OK");
        LOG_INFO("新玩家注册成功: " + username);
        return true;
    } else {
        sendToClient(fd, "ERROR Register failed, please try again");
        LOG_ERROR("注册失败（数据库错误）: " + username);
        return false;
    }
}

// =========================================================
// handleLogin：用户登录
// =========================================================

bool Server::handleLogin(int fd, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        sendToClient(fd, "ERROR Invalid arguments for LOGIN");
        return false;
    }

    const std::string& username = args[1];
    const std::string& password = args[2];

    // 检查是否已登录
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it != clients_.end() && it->second.status >= STATUS_LOGGED_IN) {
            sendToClientUnlocked(fd, "ERROR Already logged in");
            return false;
        }
    }

    // 对密码做 SHA-256 哈希，与注册时使用相同规则（固定盐=""）
    std::string hashed_pw = db_->hashPassword(password, "");

    // 执行登录验证（数据库操作，在锁外执行）
    Player* player = db_->loginPlayer(username, hashed_pw);

    if (!player) {
        sendToClient(fd, "ERROR Login failed, wrong username or password");
        LOG_WARN("登录失败: " + username);
        return false;
    }

    // 生成会话 ID 并保存
    std::string session_id = generateSessionId();
    std::string ip;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it == clients_.end()) {
            delete player;
            return false;
        }
        ip = it->second.ip_address;
    }

    // 创建数据库会话（在锁外）
    db_->createSession(session_id, player->player_id, ip);

    // 更新客户端会话状态（加锁）
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it == clients_.end()) {
            db_->deleteSession(session_id);
            delete player;
            return false;
        }
        ClientSession& session = it->second;
        session.status     = STATUS_LOGGED_IN;
        session.session_id = session_id;
        session.player_id  = player->player_id;
        session.username   = player->username;
        session_to_fd_[session_id] = fd;

        // 发送登录成功响应（持有锁，使用无锁版本发送）
        std::string response = "LOGIN_OK " + session_id
            + " " + std::to_string(player->total_games)
            + " " + std::to_string(player->wins)
            + " " + std::to_string(player->losses)
            + " " + std::to_string(player->draws)
            + " " + player->username;
        sendToClientUnlocked(fd, response);
    }

    LOG_INFO("玩家登录成功: " + username + " session=" + session_id);
    delete player;
    return true;
}

// =========================================================
// handleMatch：请求匹配
// =========================================================

bool Server::handleMatch(int fd, const std::vector<std::string>& args) {
    (void)args;

    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = clients_.find(fd);
    if (it == clients_.end()) return false;

    ClientSession& session = it->second;

    if (session.status < STATUS_LOGGED_IN) {
        sendToClientUnlocked(fd, "ERROR Not logged in");
        return false;
    }
    if (session.status == STATUS_WAITING) {
        sendToClientUnlocked(fd, "ERROR Already in queue");
        return false;
    }
    if (session.status == STATUS_PLAYING) {
        sendToClientUnlocked(fd, "ERROR Already in game");
        return false;
    }

    session.status = STATUS_WAITING;
    {
        std::lock_guard<std::mutex> q_lock(queue_mutex_);
        waiting_queue_.push(fd);
    }

    sendToClientUnlocked(fd, "MATCH_QUEUE");
    LOG_INFO("玩家 " + session.username + " 进入匹配队列 fd=" + std::to_string(fd));
    return true;
}

// =========================================================
// handleMove：落子
// =========================================================

bool Server::handleMove(int fd, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        sendToClient(fd, "ERROR Invalid arguments for MOVE");
        return false;
    }

    int x, y;
    try {
        x = std::stoi(args[1]);
        y = std::stoi(args[2]);
    } catch (...) {
        sendToClient(fd, "ERROR Invalid coordinates");
        return false;
    }

    // 获取当前玩家状态
    int room_id = -1;
    PlayerColor my_color = EMPTY;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it == clients_.end()) return false;
        if (it->second.status != STATUS_PLAYING) {
            sendToClientUnlocked(fd, "ERROR Not in game");
            return false;
        }
        room_id  = it->second.room_id;
        my_color = it->second.color;
    }

    // 处理落子逻辑
    std::lock_guard<std::mutex> r_lock(rooms_mutex_);
    auto room_it = rooms_.find(room_id);
    if (room_it == rooms_.end()) {
        sendToClient(fd, "ERROR Room not found");
        return false;
    }

    Room& room = room_it->second;
    if (!room.active) {
        sendToClient(fd, "ERROR Room is no longer active");
        return false;
    }
    if (room.current_turn != my_color) {
        sendToClient(fd, "ERROR Not your turn");
        return false;
    }
    if (!room.game.makeMove(x, y, my_color)) {
        sendToClient(fd, "ERROR Invalid move");
        return false;
    }

    // 记录落子到数据库
    db_->addMoveRecord(room.match_id, room.game.getMoveCount(), my_color, x, y);

    // 广播落子消息（带上落子方颜色：1=黑, 2=白，避免客户端猜测）
    std::string move_msg = "MOVE " + std::to_string(x) + " " + std::to_string(y) + " "
                         + std::to_string(static_cast<int>(my_color));
    notifyRoomPlayers(room_id, move_msg);

    // 检查胜负
    GameResult result = room.game.checkWin(x, y);
    if (result != NONE) {
        endRoom(room_id, result);
    } else {
        // 切换回合
        room.current_turn = (my_color == BLACK) ? WHITE : BLACK;
        std::string turn_msg = "TURN " + std::to_string(static_cast<int>(room.current_turn));
        notifyRoomPlayers(room_id, turn_msg);
    }
    return true;
}

// =========================================================
// handleSurrender：认输
// =========================================================

bool Server::handleSurrender(int fd, const std::vector<std::string>& args) {
    (void)args;

    int room_id = -1;
    PlayerColor my_color = EMPTY;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it == clients_.end()) return false;
        if (it->second.status != STATUS_PLAYING) {
            sendToClientUnlocked(fd, "ERROR Not in game");
            return false;
        }
        room_id  = it->second.room_id;
        my_color = it->second.color;

        LOG_INFO("玩家 " + it->second.username + " 认输");
    }

    GameResult result = (my_color == BLACK) ? WHITE_WIN : BLACK_WIN;
    std::lock_guard<std::mutex> r_lock(rooms_mutex_);
    endRoom(room_id, result);
    return true;
}

// =========================================================
// handleQuit：退出
// =========================================================

bool Server::handleQuit(int fd, const std::vector<std::string>& args) {
    (void)args;
    LOG_INFO("客户端主动退出 fd=" + std::to_string(fd));
    removeClient(fd);
    return true;
}

// =========================================================
// handleStats：查询战绩
// =========================================================

bool Server::handleStats(int fd, const std::vector<std::string>& args) {
    (void)args;

    int player_id = -1;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it == clients_.end()) return false;
        if (it->second.status < STATUS_LOGGED_IN) {
            sendToClientUnlocked(fd, "ERROR Not logged in");
            return false;
        }
        player_id = it->second.player_id;
    }

    Player* player = db_->getPlayerById(player_id);
    if (player) {
        std::string resp = "STATS_RESULT "
            + std::to_string(player->total_games) + " "
            + std::to_string(player->wins)        + " "
            + std::to_string(player->losses)      + " "
            + std::to_string(player->draws);
        sendToClient(fd, resp);
        delete player;
        return true;
    }

    sendToClient(fd, "ERROR Failed to get stats");
    return false;
}

// =========================================================
// handleReplay：查看对局回放
// =========================================================

bool Server::handleReplay(int fd, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        sendToClient(fd, "ERROR Invalid arguments for REPLAY");
        return false;
    }

    int match_id;
    try {
        match_id = std::stoi(args[1]);
    } catch (...) {
        sendToClient(fd, "ERROR Invalid match_id");
        return false;
    }

    std::vector<MoveRecord*> moves = db_->getMatchMoves(match_id);
    if (moves.empty()) {
        sendToClient(fd, "ERROR No replay data found");
        return false;
    }

    sendToClient(fd, "REPLAY_START " + std::to_string(match_id));
    for (MoveRecord* mv : moves) {
        sendToClient(fd, "REPLAY_MOVE "
            + std::to_string(mv->move_number) + " "
            + std::to_string(mv->player_color) + " "
            + std::to_string(mv->pos_x) + " "
            + std::to_string(mv->pos_y));
        delete mv;
    }
    sendToClient(fd, "REPLAY_END");
    return true;
}

// =========================================================
// handleHeartbeat：客户端心跳
// =========================================================

bool Server::handleHeartbeat(int fd, const std::vector<std::string>& args) {
    (void)args;
    // 更新活跃时间（无需数据库操作）
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(fd);
        if (it != clients_.end()) {
            it->second.last_active = std::chrono::steady_clock::now();
        }
    }
    sendToClient(fd, "HEARTBEAT_OK");
    return true;
}

// =========================================================
// matchThread：匹配线程
// =========================================================

void Server::matchThread() {
    LOG_INFO("matchThread 启动");
    while (running_) {
        {
            std::lock_guard<std::mutex> q_lock(queue_mutex_);
            while (waiting_queue_.size() >= 2) {
                int p1 = waiting_queue_.front(); waiting_queue_.pop();
                int p2 = waiting_queue_.front(); waiting_queue_.pop();
                // createRoom 在 matchThread 中调用，需要 clients_mutex_
                // 但不在 queue_mutex_ 持有时获取（可能死锁）
                // 因此先 pop 再 unlock，然后在 createRoom 里加锁
                // 这里 queue_mutex_ 的 lock_guard 仍在作用域内，
                // createRoom 不加 queue_mutex_，不会死锁
                createRoom(p1, p2);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_INFO("matchThread 退出");
}

// =========================================================
// heartbeatThread：超时检测
// =========================================================

void Server::heartbeatThread() {
    LOG_INFO("heartbeatThread 启动");
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));

        auto now = std::chrono::steady_clock::now();
        std::vector<int> timeout_fds;

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (auto& pair : clients_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - pair.second.last_active).count();
                if (elapsed > MAX_IDLE_TIME) {
                    timeout_fds.push_back(pair.first);
                }
            }
        }

        for (int fd : timeout_fds) {
            LOG_INFO("客户端超时断开 fd=" + std::to_string(fd));
            removeClient(fd);
        }
    }
    LOG_INFO("heartbeatThread 退出");
}

// =========================================================
// createRoom：创建游戏房间
// =========================================================

void Server::createRoom(int p1_fd, int p2_fd) {
    std::string p1_session, p2_session, p1_user, p2_user;
    int p1_id = -1, p2_id = -1;

    // 验证两个玩家都还在线且处于等待状态
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it1 = clients_.find(p1_fd);
        auto it2 = clients_.find(p2_fd);

        // 若其中一人已离线，将另一人放回队列
        if (it1 == clients_.end() || it1->second.status != STATUS_WAITING) {
            if (it2 != clients_.end() && it2->second.status == STATUS_WAITING) {
                std::lock_guard<std::mutex> q_lock(queue_mutex_);
                waiting_queue_.push(p2_fd);
            }
            return;
        }
        if (it2 == clients_.end() || it2->second.status != STATUS_WAITING) {
            if (it1 != clients_.end() && it1->second.status == STATUS_WAITING) {
                std::lock_guard<std::mutex> q_lock(queue_mutex_);
                waiting_queue_.push(p1_fd);
            }
            return;
        }

        p1_session = it1->second.session_id;
        p2_session = it2->second.session_id;
        p1_user    = it1->second.username;
        p2_user    = it2->second.username;
        p1_id      = it1->second.player_id;
        p2_id      = it2->second.player_id;
    }

    // 创建数据库比赛记录（在锁外）
    int match_id = db_->createMatch(p1_id, p2_id);
    if (match_id < 0) {
        LOG_ERROR("createMatch 失败");
        return;
    }

    // 生成房间 ID
    int room_id = static_cast<int>(
        std::hash<std::string>()(p1_session + p2_session) & 0x7FFFFFFF);

    // 更新客户端状态并建立房间
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it1 = clients_.find(p1_fd);
        auto it2 = clients_.find(p2_fd);
        if (it1 == clients_.end() || it2 == clients_.end()) return;

        it1->second.status  = STATUS_PLAYING;
        it1->second.room_id = room_id;
        it1->second.color   = BLACK;

        it2->second.status  = STATUS_PLAYING;
        it2->second.room_id = room_id;
        it2->second.color   = WHITE;

        {
            std::lock_guard<std::mutex> r_lock(rooms_mutex_);
            Room room;
            room.room_id          = room_id;
            room.black_player_id  = p1_id;
            room.white_player_id  = p2_id;
            room.black_session_id = p1_session;
            room.white_session_id = p2_session;
            room.match_id         = match_id;
            room.game             = GameLogic();
            room.active           = true;
            room.current_turn     = BLACK;
            rooms_[room_id]       = std::move(room);
        }

        // 发送匹配成功通知（持有锁，用无锁版本）
        sendToClientUnlocked(p1_fd,
            "MATCH_START " + std::to_string(match_id) + " BLACK " + p2_user);
        sendToClientUnlocked(p2_fd,
            "MATCH_START " + std::to_string(match_id) + " WHITE " + p1_user);
    }

    LOG_INFO("房间创建: room=" + std::to_string(room_id)
             + " match=" + std::to_string(match_id)
             + " 黑=" + p1_user + " 白=" + p2_user);
}

// =========================================================
// endRoom：结束游戏房间
// =========================================================

void Server::endRoom(int room_id, GameResult result) {
    // 注意：调用此函数时必须已持有 rooms_mutex_
    // （由 handleMove、handleSurrender 等在持有 r_lock 时调用）
    auto room_it = rooms_.find(room_id);
    if (room_it == rooms_.end()) return;

    Room& room = room_it->second;
    if (!room.active) return;
    room.active = false;

    // 更新数据库
    db_->updateMatchResult(room.match_id, result);

    std::string winner_str;
    if (result == BLACK_WIN) {
        db_->updatePlayerStats(room.black_player_id, true,  false);
        db_->updatePlayerStats(room.white_player_id, false, false);
        winner_str = "BLACK";
    } else if (result == WHITE_WIN) {
        db_->updatePlayerStats(room.white_player_id, true,  false);
        db_->updatePlayerStats(room.black_player_id, false, false);
        winner_str = "WHITE";
    } else {
        db_->updatePlayerStats(room.black_player_id, false, true);
        db_->updatePlayerStats(room.white_player_id, false, true);
        winner_str = "DRAW";
    }

    // 通知双方游戏结束
    notifyRoomPlayers(room_id, "GAME_OVER " + winner_str);

    // 更新双方客户端状态
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto update_player = [&](const std::string& session_id) {
            auto sit = session_to_fd_.find(session_id);
            if (sit == session_to_fd_.end()) return;
            auto cit = clients_.find(sit->second);
            if (cit == clients_.end()) return;
            cit->second.status  = STATUS_LOGGED_IN;
            cit->second.room_id = -1;
            cit->second.color   = EMPTY;
        };
        update_player(room.black_session_id);
        update_player(room.white_session_id);
    }

    LOG_INFO("房间结束: room=" + std::to_string(room_id) + " 胜方=" + winner_str);
    rooms_.erase(room_it);
}

// =========================================================
// notifyRoomPlayers：广播消息给房间双方
// =========================================================

void Server::notifyRoomPlayers(int room_id, const std::string& message) {
    // 调用时可能已持有 rooms_mutex_（来自 endRoom/handleMove），也可能没有持有
    // 此处不再加 rooms_mutex_，由调用方负责
    auto room_it = rooms_.find(room_id);
    if (room_it == rooms_.end()) return;

    const Room& room = room_it->second;
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto send_to = [&](const std::string& session_id) {
        auto sit = session_to_fd_.find(session_id);
        if (sit == session_to_fd_.end()) return;
        sendToClientUnlocked(sit->second, message);
    };

    send_to(room.black_session_id);
    send_to(room.white_session_id);
}

// =========================================================
// 工具函数
// =========================================================

std::vector<std::string> Server::splitMessage(const std::string& message) {
    std::vector<std::string> args;
    std::istringstream ss(message);
    std::string token;
    while (ss >> token) {
        args.push_back(token);
    }
    return args;
}

std::string Server::generateSessionId() {
    static const char chars[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);

    std::string id;
    id.reserve(32);
    for (int i = 0; i < 32; ++i) {
        id += chars[dis(gen)];
    }
    return id;
}
