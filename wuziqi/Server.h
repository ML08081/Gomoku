#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <functional>
#include <condition_variable>

// Linux epoll 头文件（麒麟系统兼容）
#ifndef _WIN32
#include <sys/epoll.h>
#endif

#include "TcpSocket.h"
#include "Database.h"
#include "GameLogic.h"
#include "Logger.h"

// =========================================================
// 客户端状态枚举
// =========================================================
enum ClientStatus {
    STATUS_DISCONNECTED = 0,  // 已断开
    STATUS_CONNECTED    = 1,  // 已连接但未登录
    STATUS_LOGGED_IN    = 2,  // 已登录
    STATUS_WAITING      = 3,  // 等待匹配
    STATUS_PLAYING      = 4   // 游戏中
};

// =========================================================
// 客户端会话信息
// =========================================================
struct ClientSession {
    int fd;                                             // socket 文件描述符
    std::unique_ptr<TcpSocket> socket;                  // TCP socket 对象
    std::string ip_address;                             // 客户端 IP
    int port;                                           // 客户端端口
    ClientStatus status;                                // 当前状态
    std::string session_id;                             // 登录后的会话 ID
    int player_id;                                      // 玩家 ID（-1 表示未登录）
    std::string username;                               // 玩家用户名
    int room_id;                                        // 所在房间 ID（-1 表示无）
    PlayerColor color;                                  // 棋子颜色
    std::string receive_buffer;                         // 接收缓冲区（处理粘包）
    std::chrono::steady_clock::time_point last_active;  // 最后活跃时间
    bool active;                                        // 是否有效
};

// =========================================================
// 游戏房间信息
// =========================================================
struct Room {
    int room_id;
    int black_player_id;
    int white_player_id;
    std::string black_session_id;
    std::string white_session_id;
    int match_id;           // 数据库中的比赛 ID
    GameLogic game;
    bool active;
    PlayerColor current_turn;
};

// =========================================================
// 线程池任务队列（用于处理客户端消息）
// =========================================================
class TaskQueue {
public:
    TaskQueue() : stopped_(false) {}

    // 提交任务（fd + 消息内容）
    void push(std::function<void()> task) {
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
        cond_.notify_one();
    }

    // 取出任务（阻塞等待）
    bool pop(std::function<void()>& task) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] {
            return !tasks_.empty() || stopped_;
        });
        if (stopped_ && tasks_.empty()) return false;
        task = std::move(tasks_.front());
        tasks_.pop();
        return true;
    }

    void stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        stopped_ = true;
        cond_.notify_all();
    }

private:
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stopped_;
};

// =========================================================
// Server 主类
// =========================================================
class Server {
public:
    Server(int port, Database* db);
    ~Server();

    bool start();
    void stop();

private:
    // ---- 线程入口 ----
    void acceptThread();        // 专门接受新连接
    void epollThread();         // epoll 驱动：监听所有已连接 fd 的可读事件
    void workerThread(int id);  // 工作线程：处理消息任务
    void matchThread();         // 匹配线程
    void heartbeatThread();     // 心跳超时检测线程

    // ---- 消息处理 ----
    void dispatchClientData(int fd);                      // 读取数据并分发任务
    void processMessage(int fd, const std::string& msg);  // 解析并路由命令

    // ---- 命令处理器（在 clients_mutex_ 外执行，不持有锁）----
    bool handleLogin(int fd, const std::vector<std::string>& args);
    bool handleRegister(int fd, const std::vector<std::string>& args);
    bool handleMatch(int fd, const std::vector<std::string>& args);
    bool handleMove(int fd, const std::vector<std::string>& args);
    bool handleSurrender(int fd, const std::vector<std::string>& args);
    bool handleQuit(int fd, const std::vector<std::string>& args);
    bool handleStats(int fd, const std::vector<std::string>& args);
    bool handleReplay(int fd, const std::vector<std::string>& args);
    bool handleHeartbeat(int fd, const std::vector<std::string>& args);

    // ---- 发送辅助（两个版本：无锁/有锁）----
    // 注意：调用方需自行保证线程安全
    void sendToClientUnlocked(int fd, const std::string& message);  // 调用前已持有 clients_mutex_
    void sendToClient(int fd, const std::string& message);          // 内部加锁

    // ---- 连接管理 ----
    void removeClient(int fd, bool notify_opponent = true);         // 内部加锁
    void removeClientUnlocked(int fd, bool notify_opponent = true); // 调用前已持有 clients_mutex_

    // ---- 游戏房间管理 ----
    void matchPlayers();
    void createRoom(int player1_fd, int player2_fd);
    void endRoom(int room_id, GameResult result);
    void notifyRoomPlayers(int room_id, const std::string& message);

    // ---- 工具 ----
    std::vector<std::string> splitMessage(const std::string& message);
    std::string generateSessionId();

    // ---- 添加/移除 epoll 监听 ----
    bool epollAdd(int fd);
    bool epollDel(int fd);

    // ---- 成员变量 ----
    int port_;
    TcpSocket server_socket_;
    Database* db_;
    std::atomic<bool> running_;

    // epoll fd（仅 Linux/麒麟系统使用）
#ifndef _WIN32
    int epoll_fd_;
    static const int EPOLL_MAX_EVENTS = 256;
#endif

    // 客户端映射表
    std::unordered_map<int, ClientSession> clients_;
    std::unordered_map<std::string, int> session_to_fd_;  // session_id -> fd
    std::mutex clients_mutex_;

    // 游戏房间表
    std::unordered_map<int, Room> rooms_;
    std::mutex rooms_mutex_;

    // 匹配队列
    std::queue<int> waiting_queue_;
    std::mutex queue_mutex_;

    // 工作线程任务队列
    TaskQueue task_queue_;

    // 线程集合
    std::thread accept_thread_;
    std::thread epoll_thread_;
    std::thread match_thread_;
    std::thread heartbeat_thread_;
    std::vector<std::thread> worker_threads_;

    // 配置常量
    static const int WORKER_THREADS   = 4;    // 工作线程数量
    static const int HEARTBEAT_INTERVAL = 30; // 心跳检测间隔（秒）
    static const int MAX_IDLE_TIME    = 120;  // 最大空闲时间（秒）
};

#endif // SERVER_H
