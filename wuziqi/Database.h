#ifndef DATABASE_H
#define DATABASE_H

#include <memory>
#include <string>
#include <vector>
#include "GameLogic.h"

struct Player {
    int player_id;
    std::string username;
    std::string password_hash;
    // 注意：players 表无 salt 列，此处保留字段仅供内存中临时使用
    // std::string salt;
    std::string register_time;
    int total_games;
    int wins;
    int losses;
    int draws;
};

struct Match {
    int match_id;
    int black_player_id;
    int white_player_id;
    int winner;
    std::string start_time;
    std::string end_time;
    int total_moves;
};

struct MoveRecord {
    int move_id;
    int match_id;
    int move_number;
    int player_color;
    int pos_x;
    int pos_y;
    std::string move_time;
};

struct Session {
    std::string session_id;
    int player_id;
    std::string login_time;
    std::string last_active_time;
    std::string ip_address;
};

class Database {
public:
    Database(const std::string& host, const std::string& user, const std::string& password, const std::string& dbname);
    ~Database();
    
    bool connect();
    void disconnect();
    Player* getPlayer(int player_id);
    
    bool registerPlayer(const std::string& username, const std::string& password_hash);
    Player* loginPlayer(const std::string& username, const std::string& password_hash);
    bool updatePlayerStats(int player_id, bool is_win, bool is_draw);
    Player* getPlayerById(int player_id);
    Player* getPlayerByUsername(const std::string& username);
    
    int createMatch(int black_player_id, int white_player_id);
    bool updateMatchResult(int match_id, GameResult result);
    Player* getPlayerBySession(const std::string& session_id);
    std::string hashPassword(const std::string& password, const std::string& salt);
    std::string generateSalt();
    std::string generateSessionId();
    Match* getMatchById(int match_id);
    std::vector<Match*> getPlayerMatches(int player_id);
    
    bool addMoveRecord(int match_id, int move_number, int player_color, int pos_x, int pos_y);
    std::vector<MoveRecord*> getMatchMoves(int match_id);
    
    bool createSession(const std::string& session_id, int player_id, const std::string& ip_address);
    bool updateSessionActive(const std::string& session_id);
    Session* getSession(const std::string& session_id);
    bool deleteSession(const std::string& session_id);
    
    bool isConnected() const { return connected_; }
    
private:
    std::string host_;
    std::string user_;
    std::string password_;
    std::string dbname_;
    bool connected_;
    
    void* connection_;
};

#endif // DATABASE_H