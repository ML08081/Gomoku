#include "Logger.h"
#include "GameLogic.h"
#include "Database.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include <sstream>
#include <random>
#include <iomanip>
#include <openssl/sha.h>

// ============================================================
// 说明：本文件所有 SQL 字段名均与数据库代码.txt 中定义的表结构严格对齐
//   players     : player_id, username, password_hash, register_time,
//                 total_games, wins, losses, draws
//                 （注意：无 salt 列，密码采用"password+固定盐"SHA-256存储）
//   matches     : match_id, black_player_id, white_player_id,
//                 winner(TINYINT: 1=黑胜,2=白胜,0=平局), start_time,
//                 end_time, total_moves
//   moves       : move_id, match_id, move_number, player_color(TINYINT),
//                 pos_x, pos_y, move_time
//   game_sessions: session_id, player_id, login_time, last_active_time,
//                  ip_address
// ============================================================

// -------------------- 构造/析构 --------------------

Database::Database(const std::string& host, const std::string& user,
                   const std::string& password, const std::string& dbname)
    : host_(host), user_(user), password_(password), dbname_(dbname),
      connected_(false), connection_(nullptr) {}

Database::~Database() {
    disconnect();
}

// -------------------- 连接管理 --------------------

bool Database::connect() {
    try {
        LOG_INFO("正在连接数据库... 主机=" + host_ + " 用户=" + user_
                 + " 数据库=" + dbname_);
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        sql::Connection* conn = driver->connect(host_, user_, password_);
        conn->setSchema(dbname_);
        connection_ = conn;
        connected_  = true;
        LOG_INFO("数据库连接成功");
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("数据库连接错误: " + std::string(e.what())
                  + " (错误码=" + std::to_string(e.getErrorCode())
                  + " SQL状态=" + e.getSQLState() + ")");
        LOG_ERROR("请检查: 1)MySQL服务是否运行 2)账密是否正确 3)数据库是否已建");
        return false;
    }
}

void Database::disconnect() {
    if (connection_) {
        delete static_cast<sql::Connection*>(connection_);
        connection_ = nullptr;
        connected_  = false;
    }
}

// -------------------- 玩家注册 --------------------
// 注意：players 表无 salt 列，密码采用 SHA-256(password + APP_SALT) 存储。
// 若需升级为随机盐方案，只需在表中加 salt 列并取消下方注释。

bool Database::registerPlayer(const std::string& username,
                               const std::string& password_hash) {
    if (!connected_) {
        LOG_ERROR("数据库未连接，无法注册: " + username);
        return false;
    }
    try {
        LOG_INFO("正在注册玩家: " + username);
        // password_hash 由调用方（Server.cpp）在传入前已做 SHA-256 处理
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "INSERT INTO players (username, password_hash) "
                "VALUES (?, ?)"));
        stmt->setString(1, username);
        stmt->setString(2, password_hash);
        int affected = stmt->executeUpdate();
        if (affected > 0) {
            LOG_INFO("玩家注册成功: " + username);
            return true;
        }
        LOG_WARN("玩家注册：无记录插入: " + username);
        return false;
    } catch (sql::SQLException& e) {
        LOG_ERROR("玩家注册错误: " + std::string(e.what())
                  + " (错误码=" + std::to_string(e.getErrorCode()) + ")");
        return false;
    }
}

// -------------------- 玩家登录 --------------------

Player* Database::loginPlayer(const std::string& username,
                               const std::string& password_hash) {
    if (!connected_) {
        LOG_ERROR("数据库未连接，无法登录: " + username);
        return nullptr;
    }
    try {
        LOG_INFO("正在验证登录: " + username);
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT player_id, username, password_hash, "
                "       register_time, total_games, wins, losses, draws "
                "FROM players WHERE username = ?"));
        stmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            std::string stored_hash = res->getString("password_hash");
            // password_hash 由调用方已做 SHA-256 处理，直接比较
            if (password_hash == stored_hash) {
                Player* player = new Player();
                player->player_id    = res->getInt("player_id");
                player->username     = res->getString("username");
                player->password_hash = stored_hash;
                player->register_time = res->getString("register_time");
                player->total_games  = res->getInt("total_games");
                player->wins         = res->getInt("wins");
                player->losses       = res->getInt("losses");
                player->draws        = res->getInt("draws");
                LOG_INFO("玩家登录成功: " + username);
                return player;
            }
            LOG_WARN("玩家登录失败，密码错误: " + username);
        } else {
            LOG_WARN("玩家不存在: " + username);
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("玩家登录错误: " + std::string(e.what())
                  + " (错误码=" + std::to_string(e.getErrorCode()) + ")");
    }
    return nullptr;
}

// -------------------- 按 ID 查玩家 --------------------

Player* Database::getPlayer(int player_id) {
    if (!connected_) return nullptr;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT player_id, username, password_hash, "
                "       register_time, total_games, wins, losses, draws "
                "FROM players WHERE player_id = ?"));
        stmt->setInt(1, player_id);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            Player* player = new Player();
            player->player_id     = res->getInt("player_id");
            player->username      = res->getString("username");
            player->password_hash = res->getString("password_hash");
            player->register_time = res->getString("register_time");
            player->total_games   = res->getInt("total_games");
            player->wins          = res->getInt("wins");
            player->losses        = res->getInt("losses");
            player->draws         = res->getInt("draws");
            return player;
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("按 ID 查玩家错误: " + std::string(e.what()));
    }
    return nullptr;
}

Player* Database::getPlayerById(int player_id) {
    return getPlayer(player_id);
}

Player* Database::getPlayerByUsername(const std::string& username) {
    if (!connected_) return nullptr;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT player_id, username, password_hash, "
                "       register_time, total_games, wins, losses, draws "
                "FROM players WHERE username = ?"));
        stmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            Player* player = new Player();
            player->player_id     = res->getInt("player_id");
            player->username      = res->getString("username");
            player->password_hash = res->getString("password_hash");
            player->register_time = res->getString("register_time");
            player->total_games   = res->getInt("total_games");
            player->wins          = res->getInt("wins");
            player->losses        = res->getInt("losses");
            player->draws         = res->getInt("draws");
            return player;
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("按用户名查玩家错误: " + std::string(e.what()));
    }
    return nullptr;
}

// -------------------- 更新战绩 --------------------

bool Database::updatePlayerStats(int player_id, bool is_win, bool is_draw) {
    if (!connected_) return false;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt;
        if (is_win) {
            stmt.reset(conn->prepareStatement(
                "UPDATE players SET total_games = total_games + 1, "
                "wins = wins + 1 WHERE player_id = ?"));
        } else if (is_draw) {
            stmt.reset(conn->prepareStatement(
                "UPDATE players SET total_games = total_games + 1, "
                "draws = draws + 1 WHERE player_id = ?"));
        } else {
            stmt.reset(conn->prepareStatement(
                "UPDATE players SET total_games = total_games + 1, "
                "losses = losses + 1 WHERE player_id = ?"));
        }
        stmt->setInt(1, player_id);
        return stmt->executeUpdate() > 0;
    } catch (sql::SQLException& e) {
        LOG_ERROR("更新玩家战绩错误: " + std::string(e.what()));
        return false;
    }
}

// -------------------- 创建对局 --------------------
// matches 表：black_player_id, white_player_id, winner(默认NULL), start_time

int Database::createMatch(int black_player_id, int white_player_id) {
    if (!connected_) return -1;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "INSERT INTO matches "
                "  (black_player_id, white_player_id, start_time) "
                "VALUES (?, ?, NOW())"));
        stmt->setInt(1, black_player_id);
        stmt->setInt(2, white_player_id);
        stmt->executeUpdate();

        // 获取自增 ID
        std::unique_ptr<sql::Statement> st(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(
            st->executeQuery("SELECT LAST_INSERT_ID()"));
        if (res->next()) {
            int match_id = res->getInt(1);
            LOG_INFO("创建对局成功: match_id=" + std::to_string(match_id));
            return match_id;
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("创建对局错误: " + std::string(e.what())
                  + " (错误码=" + std::to_string(e.getErrorCode()) + ")");
    }
    return -1;
}

// -------------------- 更新对局结果 --------------------
// winner：1=黑方胜，2=白方胜，0=平局；同时更新 end_time 和 total_moves

bool Database::updateMatchResult(int match_id, GameResult result) {
    if (!connected_) return false;
    try {
        // 将 GameResult 枚举转换为 winner 整数值
        int winner_val;
        switch (result) {
            case BLACK_WIN: winner_val = 1; break;
            case WHITE_WIN: winner_val = 2; break;
            case DRAW:      winner_val = 0; break;
            default:        winner_val = 0; break; // ABORTED 也记平局
        }

        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        // 同时更新 total_moves（统计落子数）
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "UPDATE matches "
                "SET winner = ?, end_time = NOW(), "
                "    total_moves = (SELECT COUNT(*) FROM moves WHERE match_id = ?) "
                "WHERE match_id = ?"));
        stmt->setInt(1, winner_val);
        stmt->setInt(2, match_id);
        stmt->setInt(3, match_id);
        bool ok = stmt->executeUpdate() > 0;
        if (ok) {
            LOG_INFO("对局结果已更新: match_id=" + std::to_string(match_id)
                     + " winner=" + std::to_string(winner_val));
        }
        return ok;
    } catch (sql::SQLException& e) {
        LOG_ERROR("更新对局结果错误: " + std::string(e.what())
                  + " (错误码=" + std::to_string(e.getErrorCode()) + ")");
        return false;
    }
}

// -------------------- 按 ID 查对局 --------------------

Match* Database::getMatchById(int match_id) {
    if (!connected_) return nullptr;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT match_id, black_player_id, white_player_id, "
                "       winner, start_time, end_time, total_moves "
                "FROM matches WHERE match_id = ?"));
        stmt->setInt(1, match_id);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            Match* match = new Match();
            match->match_id         = res->getInt("match_id");
            match->black_player_id  = res->getInt("black_player_id");
            match->white_player_id  = res->getInt("white_player_id");
            match->winner           = res->getInt("winner");  // NULL → 0
            match->start_time       = res->getString("start_time");
            match->end_time         = res->getString("end_time");
            match->total_moves      = res->getInt("total_moves");
            return match;
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("查对局信息错误: " + std::string(e.what()));
    }
    return nullptr;
}

// -------------------- 查玩家所有对局 --------------------

std::vector<Match*> Database::getPlayerMatches(int player_id) {
    std::vector<Match*> matches;
    if (!connected_) return matches;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT match_id, black_player_id, white_player_id, "
                "       winner, start_time, end_time, total_moves "
                "FROM matches "
                "WHERE black_player_id = ? OR white_player_id = ? "
                "ORDER BY start_time DESC"));
        stmt->setInt(1, player_id);
        stmt->setInt(2, player_id);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            Match* match = new Match();
            match->match_id        = res->getInt("match_id");
            match->black_player_id = res->getInt("black_player_id");
            match->white_player_id = res->getInt("white_player_id");
            match->winner          = res->getInt("winner");
            match->start_time      = res->getString("start_time");
            match->end_time        = res->getString("end_time");
            match->total_moves     = res->getInt("total_moves");
            matches.push_back(match);
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("查玩家对局记录错误: " + std::string(e.what()));
    }
    return matches;
}

// -------------------- 落子记录 --------------------
// moves 表字段：player_color, pos_x, pos_y（与数据库代码.txt 严格对应）

bool Database::addMoveRecord(int match_id, int move_number,
                              int player_color, int pos_x, int pos_y) {
    if (!connected_) return false;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "INSERT INTO moves "
                "  (match_id, move_number, player_color, pos_x, pos_y) "
                "VALUES (?, ?, ?, ?, ?)"));
        stmt->setInt(1, match_id);
        stmt->setInt(2, move_number);
        stmt->setInt(3, player_color);   // 1=黑，2=白
        stmt->setInt(4, pos_x);
        stmt->setInt(5, pos_y);
        return stmt->executeUpdate() > 0;
    } catch (sql::SQLException& e) {
        LOG_ERROR("落子记录写入错误: " + std::string(e.what())
                  + " (错误码=" + std::to_string(e.getErrorCode()) + ")");
        return false;
    }
}

std::vector<MoveRecord*> Database::getMatchMoves(int match_id) {
    std::vector<MoveRecord*> moves;
    if (!connected_) return moves;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT move_id, match_id, move_number, "
                "       player_color, pos_x, pos_y, move_time "
                "FROM moves WHERE match_id = ? ORDER BY move_number"));
        stmt->setInt(1, match_id);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            MoveRecord* move = new MoveRecord();
            move->move_id      = res->getInt("move_id");
            move->match_id     = res->getInt("match_id");
            move->move_number  = res->getInt("move_number");
            move->player_color = res->getInt("player_color");
            move->pos_x        = res->getInt("pos_x");
            move->pos_y        = res->getInt("pos_y");
            move->move_time    = res->getString("move_time");
            moves.push_back(move);
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("查落子记录错误: " + std::string(e.what()));
    }
    return moves;
}

// -------------------- 会话管理 --------------------
// 表名：game_sessions（与数据库代码.txt 对应）
// 字段：session_id, player_id, login_time, last_active_time, ip_address

bool Database::createSession(const std::string& session_id, int player_id,
                              const std::string& ip_address) {
    if (!connected_) return false;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "INSERT INTO game_sessions "
                "  (session_id, player_id, login_time, last_active_time, ip_address) "
                "VALUES (?, ?, NOW(), NOW(), ?)"));
        stmt->setString(1, session_id);
        stmt->setInt(2, player_id);
        stmt->setString(3, ip_address);
        stmt->executeUpdate();
        LOG_INFO("创建会话成功: " + session_id);
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("创建会话错误: " + std::string(e.what()));
        return false;
    }
}

bool Database::updateSessionActive(const std::string& session_id) {
    if (!connected_) return false;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "UPDATE game_sessions "
                "SET last_active_time = NOW() "
                "WHERE session_id = ?"));
        stmt->setString(1, session_id);
        return stmt->executeUpdate() > 0;
    } catch (sql::SQLException& e) {
        LOG_ERROR("更新会话活跃时间错误: " + std::string(e.what()));
        return false;
    }
}

Session* Database::getSession(const std::string& session_id) {
    if (!connected_) return nullptr;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT session_id, player_id, login_time, "
                "       last_active_time, ip_address "
                "FROM game_sessions WHERE session_id = ?"));
        stmt->setString(1, session_id);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            Session* session = new Session();
            session->session_id       = res->getString("session_id");
            session->player_id        = res->getInt("player_id");
            session->login_time       = res->getString("login_time");
            session->last_active_time = res->getString("last_active_time");
            session->ip_address       = res->getString("ip_address");
            return session;
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("查询会话错误: " + std::string(e.what()));
    }
    return nullptr;
}

bool Database::deleteSession(const std::string& session_id) {
    if (!connected_) return false;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "DELETE FROM game_sessions WHERE session_id = ?"));
        stmt->setString(1, session_id);
        return stmt->executeUpdate() > 0;
    } catch (sql::SQLException& e) {
        LOG_ERROR("删除会话错误: " + std::string(e.what()));
        return false;
    }
}

Player* Database::getPlayerBySession(const std::string& session_id) {
    if (!connected_) return nullptr;
    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection_);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT p.player_id, p.username, p.password_hash, "
                "       p.register_time, p.total_games, p.wins, p.losses, p.draws "
                "FROM players p "
                "JOIN game_sessions s ON p.player_id = s.player_id "
                "WHERE s.session_id = ?"));
        stmt->setString(1, session_id);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            Player* player = new Player();
            player->player_id     = res->getInt("player_id");
            player->username      = res->getString("username");
            player->password_hash = res->getString("password_hash");
            player->register_time = res->getString("register_time");
            player->total_games   = res->getInt("total_games");
            player->wins          = res->getInt("wins");
            player->losses        = res->getInt("losses");
            player->draws         = res->getInt("draws");
            return player;
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("通过会话查玩家错误: " + std::string(e.what()));
    }
    return nullptr;
}

// -------------------- 工具函数 --------------------

std::string Database::hashPassword(const std::string& password,
                                    const std::string& salt) {
    // salt 参数保留接口兼容性，实际表中无 salt 列，使用固定后缀即可
    std::string input = password + salt;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()),
           input.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string Database::generateSalt() {
    // 为保持接口兼容性保留此函数；若表中无 salt 列可返回固定字符串
    static const char alphanum[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    std::string salt;
    for (int i = 0; i < 16; i++) salt += alphanum[dis(gen)];
    return salt;
}

std::string Database::generateSessionId() {
    static const char alphanum[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    std::string id;
    for (int i = 0; i < 32; i++) id += alphanum[dis(gen)];
    return id;
}
