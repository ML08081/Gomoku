#include <iostream>
#include <string>
#include <signal.h>
#include <chrono>
#include <thread>

#include "Server.h"
#include "Database.h"
#include "Logger.h"

Server* g_server = nullptr;

void signalHandler(int signum) {
    LOG_INFO("收到关闭信号 " + std::to_string(signum) + "，正在关闭服务器...");
    if (g_server) {
        g_server->stop();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    std::string db_host = "tcp://127.0.0.1:3306";
    std::string db_user = "root";
    std::string db_password = "q12121212";
    std::string db_name = "gomoku_game";
    int port = 8888;

    if (argc >= 2) {
        port = std::stoi(argv[1]);
    }

    if (argc >= 3) {
        db_host = argv[2];
    }

    if (argc >= 4) {
        db_user = argv[3];
    }

    if (argc >= 5) {
        db_password = argv[4];
    }

    if (argc >= 6) {
        db_name = argv[5];
    }

    Logger::getInstance().setLogLevel(LOG_INFO);
    Logger::getInstance().setLogFile("gomoku_server.log");

    std::cout << "==========================================" << std::endl;
    std::cout << "        五子棋游戏服务器 v1.0" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "配置信息:" << std::endl;
    std::cout << "  监听端口: " << port << std::endl;
    std::cout << "  数据库主机: " << db_host << std::endl;
    std::cout << "  数据库名称: " << db_name << std::endl;
    std::cout << "==========================================" << std::endl;

    LOG_INFO("正在启动五子棋游戏服务器 v1.0");
    LOG_INFO("配置: 端口=" + std::to_string(port) + " 数据库=" + db_name);

    Database db(db_host, db_user, db_password, db_name);

    if (!db.connect()) {
        LOG_ERROR("数据库连接失败");
        std::cerr << "❌ 数据库连接失败！" << std::endl;
        return 1;
    }
    LOG_INFO("数据库连接成功");
    std::cout << "✅ 数据库连接成功！" << std::endl;

    Server server(port, &db);

    if (!server.start()) {
        LOG_ERROR("服务器启动失败");
        std::cerr << "❌ 服务器启动失败！" << std::endl;
        return 1;
    }

    g_server = &server;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    LOG_INFO("服务器启动成功，正在监听端口 " + std::to_string(port));
    std::cout << "✅ 服务器启动成功！" << std::endl;
    std::cout << "🚀 正在监听端口 " << port << "..." << std::endl;
    std::cout << "按 Ctrl+C 停止服务器" << std::endl;
    std::cout << "==========================================" << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}