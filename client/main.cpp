#ifdef _WIN32
#include <windows.h>

void printChinese(const wchar_t* text) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleW(hConsole, text, wcslen(text), &written, NULL);
    }
}

void printChineseLine(const wchar_t* text) {
    printChinese(text);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleW(hConsole, L"\n", 1, &written, NULL);
    }
}
#endif

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "Client.h"

std::string generateSalt() {
    std::string salt;
    const char chars[] = "0123456789abcdef";
    for (int i = 0; i < 16; ++i) {
        salt += chars[rand() % 16];
    }
    return salt;
}

void printHelp() {
#ifdef _WIN32
    printChineseLine(L"\n可用命令:");
    printChineseLine(L"  REGISTER <用户名> <密码>     - 注册新账号");
    printChineseLine(L"  LOGIN <用户名> <密码>        - 登录账号");
    printChineseLine(L"  MATCH                       - 请求匹配玩家");
    printChineseLine(L"  MOVE <x> <y>                 - 落子");
    printChineseLine(L"  SURRENDER                   - 认输");
    printChineseLine(L"  STATS                       - 查询战绩");
    printChineseLine(L"  REPLAY <match_id>            - 回放对局");
    printChineseLine(L"  QUIT                        - 退出");
    printChineseLine(L"  HELP                        - 显示帮助");
#else
    std::cout << "\n可用命令:" << std::endl;
    std::cout << "  REGISTER <用户名> <密码>     - 注册新账号" << std::endl;
    std::cout << "  LOGIN <用户名> <密码>        - 登录账号" << std::endl;
    std::cout << "  MATCH                       - 请求匹配玩家" << std::endl;
    std::cout << "  MOVE <x> <y>                 - 落子" << std::endl;
    std::cout << "  SURRENDER                   - 认输" << std::endl;
    std::cout << "  STATS                       - 查询战绩" << std::endl;
    std::cout << "  REPLAY <match_id>            - 回放对局" << std::endl;
    std::cout << "  QUIT                        - 退出" << std::endl;
    std::cout << "  HELP                        - 显示帮助" << std::endl;
#endif
}

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int server_port = 8888;

    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = std::stoi(argv[2]);
    }

#ifdef _WIN32
    printChineseLine(L"==========================================");
    printChineseLine(L"        五子棋客户端 v1.0");
    printChineseLine(L"==========================================");
    printChinese(L"服务器地址: ");
    std::cout << server_ip << ":" << server_port << std::endl;
    printChineseLine(L"==========================================");
#else
    std::cout << "==========================================" << std::endl;
    std::cout << "        五子棋客户端 v1.0" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "服务器地址: " << server_ip << ":" << server_port << std::endl;
    std::cout << "==========================================" << std::endl;
#endif

    Client client;
    
    if (!client.connectToServer(server_ip, server_port)) {
#ifdef _WIN32
        printChineseLine(L"连接服务器失败！");
#else
        std::cerr << "连接服务器失败！" << std::endl;
#endif
        return 1;
    }

    client.startListening();
    printHelp();

    std::string input;
    while (true) {
#ifdef _WIN32
        printChinese(L"> ");
#else
        std::cout << "> ";
#endif
        std::cout.flush();
        std::getline(std::cin, input);

        if (input.empty()) continue;

        std::string command = input.substr(0, input.find(' '));
        
        if (command == "QUIT" || command == "quit") {
            client.sendMessage("QUIT");
            break;
        }
        
        if (command == "HELP" || command == "help") {
            printHelp();
            continue;
        }
        
        if (command == "REGISTER" || command == "register") {
            size_t pos = input.find(' ');
            if (pos != std::string::npos) {
                std::string rest = input.substr(pos + 1);
                size_t pos2 = rest.find(' ');
                if (pos2 != std::string::npos) {
                    std::string username = rest.substr(0, pos2);
                    std::string password = rest.substr(pos2 + 1);
                    std::string salt = generateSalt();
                    std::string msg = "REGISTER " + username + " " + password + " " + salt;
                    client.sendMessage(msg);
                    continue;
                }
            }
#ifdef _WIN32
            printChineseLine(L"用法: REGISTER <用户名> <密码>");
#else
            std::cout << "用法: REGISTER <用户名> <密码>" << std::endl;
#endif
            continue;
        }
        
        if (command == "LOGIN" || command == "login") {
            size_t pos = input.find(' ');
            if (pos != std::string::npos) {
                std::string rest = input.substr(pos + 1);
                size_t pos2 = rest.find(' ');
                if (pos2 != std::string::npos) {
                    std::string username = rest.substr(0, pos2);
                    std::string password = rest.substr(pos2 + 1);
                    std::string salt = generateSalt();
                    std::string msg = "LOGIN " + username + " " + password + " " + salt;
                    client.sendMessage(msg);
                    continue;
                }
            }
#ifdef _WIN32
            printChineseLine(L"用法: LOGIN <用户名> <密码>");
#else
            std::cout << "用法: LOGIN <用户名> <密码>" << std::endl;
#endif
            continue;
        }
        
        client.sendMessage(input);
    }

    client.stopListening();
    client.disconnect();
    
#ifdef _WIN32
    printChineseLine(L"客户端已退出");
#else
    std::cout << "客户端已退出" << std::endl;
#endif
    return 0;
}
