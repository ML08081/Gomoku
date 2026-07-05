#include "TcpSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
static bool winsock_initialized = false;
#else
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

TcpSocket::TcpSocket() : handle_(INVALID_SOCKET_VALUE) {}

TcpSocket::TcpSocket(SocketHandle handle) : handle_(handle) {}

TcpSocket::~TcpSocket() {
    close();
}

bool TcpSocket::create() {
    handle_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return handle_ != INVALID_SOCKET_VALUE;
}

bool TcpSocket::bind(const std::string& ip, int port) {
    if (!isValid()) return false;
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (ip.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    }
    
    return ::bind(handle_, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR_VALUE;
}

bool TcpSocket::listen(int backlog) {
    if (!isValid()) return false;
    return ::listen(handle_, backlog) != SOCKET_ERROR_VALUE;
}

TcpSocket* TcpSocket::accept(std::string& client_ip, int& client_port) {
    if (!isValid()) return nullptr;
    
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    SocketHandle client_handle = ::accept(handle_, (sockaddr*)&client_addr, &addr_len);
    
    if (client_handle == INVALID_SOCKET_VALUE) return nullptr;
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    client_ip = ip_str;
    client_port = ntohs(client_addr.sin_port);
    
    return new TcpSocket(client_handle);
}

bool TcpSocket::connect(const std::string& ip, int port) {
    if (!isValid()) return false;
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    
    return ::connect(handle_, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR_VALUE;
}

ssize_t TcpSocket::send(const std::string& data) {
    if (!isValid()) return -1;
    return ::send(handle_, data.c_str(), data.size(), 0);
}

ssize_t TcpSocket::recv(std::string& buffer, size_t max_size) {
    if (!isValid()) return -1;
    
    std::vector<char> temp_buffer(max_size);
    ssize_t bytes_read = ::recv(handle_, temp_buffer.data(), max_size, 0);
    
    if (bytes_read > 0) {
        buffer.assign(temp_buffer.data(), bytes_read);
    }
    
    return bytes_read;
}

void TcpSocket::close() {
    if (handle_ != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        closesocket(handle_);
#else
        ::close(handle_);
#endif
        handle_ = INVALID_SOCKET_VALUE;
    }
}

bool TcpSocket::isValid() const {
    return handle_ != INVALID_SOCKET_VALUE;
}

SocketHandle TcpSocket::getHandle() const {
    return handle_;
}

bool TcpSocket::setNonBlocking(bool enable) {
    if (!isValid()) return false;
    
#ifdef _WIN32
    u_long mode = enable ? 1 : 0;
    return ioctlsocket(handle_, FIONBIO, &mode) != SOCKET_ERROR_VALUE;
#else
    int flags = fcntl(handle_, F_GETFL, 0);
    if (flags == -1) return false;
    flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(handle_, F_SETFL, flags) != -1;
#endif
}

bool TcpSocket::setReuseAddr(bool enable) {
    if (!isValid()) return false;
    
    int optval = enable ? 1 : 0;
    return setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) != SOCKET_ERROR_VALUE;
}

bool TcpSocket::initialize() {
#ifdef _WIN32
    if (winsock_initialized) return true;
    
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) return false;
    
    winsock_initialized = true;
    return true;
#else
    return true;
#endif
}

void TcpSocket::cleanup() {
#ifdef _WIN32
    if (winsock_initialized) {
        WSACleanup();
        winsock_initialized = false;
    }
#endif
}