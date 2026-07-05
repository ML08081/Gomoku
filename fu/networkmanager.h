#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QHostAddress>

/**
 * NetworkManager - 网络通信管理类
 *
 * 功能：
 *   - 连接 / 断开服务器
 *   - 发送登录、注册、匹配、落子等指令
 *   - 使用 "\n" 作为消息分隔符处理粘包
 *   - 连接断开后自动重连（可配置）
 *   - 定时发送心跳包，保持连接活跃
 */
class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    // ---- 连接管理 ----
    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    // ---- 业务指令 ----
    void sendRegister(const QString &username, const QString &password);
    void sendLogin(const QString &username, const QString &password);
    void sendMatch();
    void sendMove(int x, int y);
    void sendSurrender();
    void sendStats();
    void sendQuit();

    // ---- 配置 ----
    void setAutoReconnect(bool enabled);    // 是否开启自动重连（默认关闭）
    void setHeartbeatEnabled(bool enabled); // 是否开启心跳（默认开启）

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void messageReceived(const QString &message);
    void reconnecting();    // 正在尝试重连

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError socketError);
    void onReadyRead();
    void onStateChanged(QAbstractSocket::SocketState state);
    void onReconnectTimer();
    void onHeartbeatTimer();

private:
    void sendMessage(const QString &message);
    void processBuffer();    // 处理接收缓冲区（解决粘包）

    QTcpSocket *socket_;
    QTimer     *reconnectTimer_;  // 重连定时器
    QTimer     *heartbeatTimer_;  // 心跳定时器

    QString    receiveBuffer_;    // 接收缓冲区（处理粘包）
    QString    lastHost_;
    quint16    lastPort_;
    bool       connectedToServer_;
    bool       autoReconnect_;
    bool       heartbeatEnabled_;
    int        reconnectAttempts_; // 已尝试重连次数
    static const int MAX_RECONNECT = 5;     // 最大重连次数
    static const int RECONNECT_INTERVAL = 3000; // 重连间隔（ms）
    static const int HEARTBEAT_INTERVAL = 25000; // 心跳间隔（ms，服务端超时120s）
};

#endif // NETWORKMANAGER_H
