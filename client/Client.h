#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>

#include "../wuziqi/TcpSocket.h"

class Client {
public:
    Client();
    ~Client();
    
    bool connectToServer(const std::string& ip, int port);
    void disconnect();
    
    bool sendMessage(const std::string& message);
    std::string receiveMessage();
    
    void startListening();
    void stopListening();
    
    bool isConnected() const;
    
private:
    void listenThread();
    
    std::unique_ptr<TcpSocket> socket_;
    std::thread listen_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
};

#endif // CLIENT_H