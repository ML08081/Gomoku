/**
 * networkmanager.cpp
 *
 * 客户端网络通信实现
 *
 * 关键改进：
 *   1. 粘包处理：使用 receiveBuffer_ 累积接收数据，
 *      每次 onReadyRead 时读取所有可用数据追加到缓冲区，
 *      再从缓冲区按 "\n" 切割完整消息，防止消息被截断。
 *   2. 自动重连：断开后可配置自动重试连接（最多 MAX_RECONNECT 次）。
 *   3. 心跳包：每 25 秒发送 HEARTBEAT 指令，防止服务端以超时断开连接。
 *   4. 发送前检查连接状态，避免在未连接时调用崩溃。
 */

#include "networkmanager.h"
#include <QDebug>
#include <QApplication>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))
    , reconnectTimer_(new QTimer(this))
    , heartbeatTimer_(new QTimer(this))
    , lastPort_(0)
    , connectedToServer_(false)
    , autoReconnect_(false)
    , heartbeatEnabled_(true)
    , reconnectAttempts_(0)
{
    // ---- socket 信号绑定 ----
    connect(socket_, &QTcpSocket::connected,
            this,    &NetworkManager::onConnected);
    connect(socket_, &QTcpSocket::disconnected,
            this,    &NetworkManager::onDisconnected);
    connect(socket_, &QTcpSocket::readyRead,
            this,    &NetworkManager::onReadyRead);
    connect(socket_, &QTcpSocket::stateChanged,
            this,    &NetworkManager::onStateChanged);

    // Qt 5.15+: 使用 errorOccurred 信号 + QOverload 精确选择信号重载
    connect(socket_, QOverload<QAbstractSocket::SocketError>::of(
                &QAbstractSocket::errorOccurred),
            this, &NetworkManager::onError);

    // ---- 重连定时器（单次触发）----
    reconnectTimer_->setSingleShot(true);
    connect(reconnectTimer_, &QTimer::timeout,
            this, &NetworkManager::onReconnectTimer);

    // ---- 心跳定时器（周期触发）----
    heartbeatTimer_->setInterval(HEARTBEAT_INTERVAL);
    connect(heartbeatTimer_, &QTimer::timeout,
            this, &NetworkManager::onHeartbeatTimer);
}

NetworkManager::~NetworkManager()
{
    heartbeatTimer_->stop();
    reconnectTimer_->stop();
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
        socket_->waitForDisconnected(500);
    }
}

// =========================================================
// 连接管理
// =========================================================

void NetworkManager::connectToServer(const QString &host, quint16 port)
{
    lastHost_ = host;
    lastPort_ = port;
    reconnectAttempts_ = 0;

    qDebug() << "[网络] 正在连接到:" << host << ":" << port;

    // 如果当前已连接，先断开
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort(); // 立即断开，不等待
    }

    receiveBuffer_.clear();
    socket_->connectToHost(host, port);
}

void NetworkManager::disconnectFromServer()
{
    autoReconnect_ = false; // 主动断开时关闭自动重连
    reconnectTimer_->stop();
    heartbeatTimer_->stop();
    connectedToServer_ = false;

    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
    }
}

bool NetworkManager::isConnected() const
{
    return connectedToServer_
        && socket_->state() == QAbstractSocket::ConnectedState;
}

void NetworkManager::setAutoReconnect(bool enabled)
{
    autoReconnect_ = enabled;
}

void NetworkManager::setHeartbeatEnabled(bool enabled)
{
    heartbeatEnabled_ = enabled;
    if (!enabled) {
        heartbeatTimer_->stop();
    }
}

// =========================================================
// 业务指令发送
// =========================================================

void NetworkManager::sendRegister(const QString &username, const QString &password)
{
    if (!isConnected()) {
        emit errorOccurred(QString::fromUtf8("未连接到服务器，请先点击「连接服务器」"));
        return;
    }
    sendMessage(QString("REGISTER %1 %2").arg(username).arg(password));
}

void NetworkManager::sendLogin(const QString &username, const QString &password)
{
    if (!isConnected()) {
        emit errorOccurred(QString::fromUtf8("未连接到服务器，请先点击「连接服务器」"));
        return;
    }
    sendMessage(QString("LOGIN %1 %2").arg(username).arg(password));
}

void NetworkManager::sendMatch()
{
    if (!isConnected()) {
        emit errorOccurred(QString::fromUtf8("未连接到服务器"));
        return;
    }
    sendMessage("MATCH");
}

void NetworkManager::sendMove(int x, int y)
{
    if (!isConnected()) return;
    sendMessage(QString("MOVE %1 %2").arg(x).arg(y));
}

void NetworkManager::sendSurrender()
{
    if (!isConnected()) return;
    sendMessage("SURRENDER");
}

void NetworkManager::sendStats()
{
    if (!isConnected()) return;
    sendMessage("STATS");
}

void NetworkManager::sendQuit()
{
    if (!isConnected()) return;
    sendMessage("QUIT");
}

// =========================================================
// 内部发送（统一入口，追加 \n）
// =========================================================

void NetworkManager::sendMessage(const QString &message)
{
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "[网络] 发送失败（未连接）:" << message;
        return;
    }

    QByteArray data = message.toUtf8() + "\n";
    qint64 written = socket_->write(data);
    if (written < 0) {
        qDebug() << "[网络] write 错误:" << socket_->errorString();
    } else {
        socket_->flush();
        qDebug() << "[网络] 发送:" << message;
    }
}

// =========================================================
// 接收数据处理（核心：粘包处理）
// =========================================================

void NetworkManager::onReadyRead()
{
    // 将所有可用数据追加到缓冲区
    QByteArray raw = socket_->readAll();
    receiveBuffer_ += QString::fromUtf8(raw);

    // 从缓冲区按 \n 切割完整消息
    processBuffer();
}

void NetworkManager::processBuffer()
{
    while (true) {
        int idx = receiveBuffer_.indexOf('\n');
        if (idx < 0) break; // 没有完整消息，等待更多数据

        QString line = receiveBuffer_.left(idx).trimmed();
        receiveBuffer_.remove(0, idx + 1); // 移除已处理的部分（包含 \n）

        if (!line.isEmpty()) {
            qDebug() << "[网络] 收到:" << line;
            emit messageReceived(line);
        }
    }
}

// =========================================================
// socket 事件处理
// =========================================================

void NetworkManager::onConnected()
{
    connectedToServer_ = true;
    reconnectAttempts_ = 0;
    receiveBuffer_.clear();

    qDebug() << "[网络] 连接成功!";

    // 启动心跳
    if (heartbeatEnabled_) {
        heartbeatTimer_->start();
    }

    emit connected();
}

void NetworkManager::onDisconnected()
{
    connectedToServer_ = false;
    heartbeatTimer_->stop();
    qDebug() << "[网络] 连接断开";

    // 自动重连逻辑
    if (autoReconnect_ && reconnectAttempts_ < MAX_RECONNECT
        && !lastHost_.isEmpty() && lastPort_ > 0)
    {
        reconnectAttempts_++;
        qDebug() << "[网络] 将在" << RECONNECT_INTERVAL / 1000
                 << "秒后尝试重连（第" << reconnectAttempts_ << "次）";
        reconnectTimer_->start(RECONNECT_INTERVAL);
        emit reconnecting();
    } else {
        emit disconnected();
    }
}

void NetworkManager::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString errorMsg = socket_->errorString();
    qDebug() << "[网络] Socket 错误:" << errorMsg;

    // ConnectError 时 onDisconnected 不一定会触发，这里也尝试重连
    heartbeatTimer_->stop();
    connectedToServer_ = false;

    emit errorOccurred(errorMsg);
}

void NetworkManager::onStateChanged(QAbstractSocket::SocketState state)
{
    switch (state) {
    case QAbstractSocket::UnconnectedState:
        qDebug() << "[网络] 状态: 未连接";
        connectedToServer_ = false;
        break;
    case QAbstractSocket::HostLookupState:
        qDebug() << "[网络] 状态: 正在查找主机";
        break;
    case QAbstractSocket::ConnectingState:
        qDebug() << "[网络] 状态: 正在连接";
        break;
    case QAbstractSocket::ConnectedState:
        qDebug() << "[网络] 状态: 已连接";
        connectedToServer_ = true;
        break;
    case QAbstractSocket::ClosingState:
        qDebug() << "[网络] 状态: 正在关闭";
        break;
    default:
        break;
    }
}

void NetworkManager::onReconnectTimer()
{
    if (!lastHost_.isEmpty() && lastPort_ > 0) {
        qDebug() << "[网络] 正在重新连接...";
        receiveBuffer_.clear();
        socket_->connectToHost(lastHost_, lastPort_);
    }
}

void NetworkManager::onHeartbeatTimer()
{
    if (isConnected()) {
        sendMessage("HEARTBEAT");
    }
}
