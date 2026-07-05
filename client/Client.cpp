#ifdef _WIN32
#include <windows.h>

static void PrintChineseClient(const wchar_t* str) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleW(hConsole, str, wcslen(str), &written, NULL);
    }
}

static void PrintChineseLineClient(const wchar_t* str) {
    PrintChineseClient(str);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        const wchar_t newline[] = L"\n";
        DWORD written;
        WriteConsoleW(hConsole, newline, 1, &written, NULL);
    }
}
#endif

#include "Client.h"
#include <iostream>

Client::Client() : running_(false), connected_(false) {}

Client::~Client() {
    stopListening();
    disconnect();
}

bool Client::connectToServer(const std::string& ip, int port) {
    if (!TcpSocket::initialize()) {
        return false;
    }
    
    socket_ = std::make_unique<TcpSocket>();
    if (!socket_->create()) {
        return false;
    }
    
    if (!socket_->connect(ip, port)) {
        socket_->close();
        return false;
    }
    
    connected_ = true;
#ifdef _WIN32
    PrintChineseLineClient(L"[客户端] 连接服务器成功！");
#else
    std::cout << "[客户端] 连接服务器成功！" << std::endl;
#endif
    return true;
}

void Client::disconnect() {
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
    connected_ = false;
#ifdef _WIN32
    PrintChineseLineClient(L"[客户端] 已断开连接");
#else
    std::cout << "[客户端] 已断开连接" << std::endl;
#endif
}

bool Client::sendMessage(const std::string& message) {
    if (!socket_ || !socket_->isValid()) {
        return false;
    }
    
    ssize_t bytes_sent = socket_->send(message + "\n");
    return bytes_sent > 0;
}

std::string Client::receiveMessage() {
    if (!socket_ || !socket_->isValid()) {
        return "";
    }
    
    std::string buffer;
    socket_->recv(buffer, 4096);
    return buffer;
}

void Client::startListening() {
    running_ = true;
    listen_thread_ = std::thread(&Client::listenThread, this);
}

void Client::stopListening() {
    running_ = false;
    if (listen_thread_.joinable()) {
        listen_thread_.join();
    }
}

bool Client::isConnected() const {
    return connected_ && socket_ && socket_->isValid();
}

void Client::listenThread() {
    while (running_) {
        if (!isConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        std::string buffer;
        ssize_t bytes_read = socket_->recv(buffer, 4096);
        
        if (bytes_read > 0) {
#ifdef _WIN32
            PrintChineseClient(L"\n[服务器消息] ");
#else
            std::cout << "\n[服务器消息] ";
#endif
            std::cout << buffer;
#ifdef _WIN32
            PrintChineseClient(L"> ");
#else
            std::cout << "> ";
#endif
            std::cout.flush();
        } else if (bytes_read == 0) {
#ifdef _WIN32
            PrintChineseLineClient(L"\n[客户端] 服务器断开连接");
#else
            std::cout << "\n[客户端] 服务器断开连接" << std::endl;
#endif
            connected_ = false;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
