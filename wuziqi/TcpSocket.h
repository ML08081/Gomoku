#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET SocketHandle;
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
typedef int SocketHandle;
#define INVALID_SOCKET_VALUE -1
#define SOCKET_ERROR_VALUE -1
#endif

class TcpSocket {
public:
    TcpSocket();
    TcpSocket(SocketHandle handle);
    ~TcpSocket();
    
    bool create();
    bool bind(const std::string& ip, int port);
    bool listen(int backlog = 10);
    TcpSocket* accept(std::string& client_ip, int& client_port);
    bool connect(const std::string& ip, int port);
    
    ssize_t send(const std::string& data);
    ssize_t recv(std::string& buffer, size_t max_size = 4096);
    
    void close();
    bool isValid() const;
    SocketHandle getHandle() const;
    
    bool setNonBlocking(bool enable);
    bool setReuseAddr(bool enable);
    
    static bool initialize();
    static void cleanup();
    
private:
    SocketHandle handle_;
};

#endif // TCPSOCKET_H