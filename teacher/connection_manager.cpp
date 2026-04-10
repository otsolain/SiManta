#include "connection_manager.h"

#include <QDebug>
#include <QDateTime>
#include <QImage>
#include <QBuffer>

namespace LabMonitor {

ConnectionManager::ConnectionManager(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &ConnectionManager::onNewConnection);
}

ConnectionManager::~ConnectionManager()
{
    stopListening();
}

bool ConnectionManager::startListening(uint16_t port)
{
    if (m_server->isListening()) {
        m_server->close();
    }

    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "[ConnectionManager] Failed to listen on port" << port
                     << ":" << m_server->errorString();
        emit listenError(m_server->errorString());
        return false;
    }

    qInfo() << "[ConnectionManager] Listening on port" << port;
    emit listeningStarted(port);
    return true;
}

void ConnectionManager::stopListening()
{
    // Disconnect all clients
    for (auto it = m_clients.begin(); it != m_clients.end(); ) {
        auto* socket = it.key();
        if (it.value().timeoutTimer) {
            it.value().timeoutTimer->stop();
            delete it.value().timeoutTimer;
        }
        socket->disconnectFromHost();
        it = m_clients.erase(it);
    }

    if (m_server->isListening()) {
        m_server->close();
    }
}

bool ConnectionManager::isListening() const
{
    return m_server->isListening();
}

QList<StudentInfo> ConnectionManager::connectedStudents() const
{
    QList<StudentInfo> list;
    for (const auto& client : m_clients) {
        if (client.helloReceived) {
            list.append(client.info);
        }
    }
    return list;
}

void ConnectionManager::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();

        if (m_clients.size() >= MAX_CONNECTIONS) {
            qWarning() << "[ConnectionManager] Max connections reached, rejecting:"
                        << socket->peerAddress().toString();
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        qInfo() << "[ConnectionManager] New connection from"
                 << socket->peerAddress().toString() << ":" << socket->peerPort();

        ClientState state;
        state.socket = socket;
        state.info.ipAddress = socket->peerAddress().toString();

        // Timeout timer
        state.timeoutTimer = new QTimer(this);
        state.timeoutTimer->setSingleShot(true);
        state.timeoutTimer->setInterval(TIMEOUT_MS);
        connect(state.timeoutTimer, &QTimer::timeout,
                this, &ConnectionManager::onClientTimeout);
        state.timeoutTimer->setProperty("socket", QVariant::fromValue<void*>(socket));
        state.timeoutTimer->start();

        m_clients[socket] = state;

        connect(socket, &QTcpSocket::readyRead,
                this, &ConnectionManager::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &ConnectionManager::onClientDisconnected);

        // Send ACK
        socket->write(createPacket(MsgType::ACK));
    }
}

void ConnectionManager::onClientReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_clients.contains(socket)) return;

    m_clients[socket].readBuffer.append(socket->readAll());

    // Reset timeout
    if (m_clients[socket].timeoutTimer) {
        m_clients[socket].timeoutTimer->start();
    }

    processClientData(socket);
}

void ConnectionManager::processClientData(QTcpSocket* socket)
{
    auto& client = m_clients[socket];

    while (client.readBuffer.size() >= HEADER_SIZE) {
        PacketHeader header;
        if (!parseHeader(client.readBuffer.left(HEADER_SIZE), header)) {
            qWarning() << "[ConnectionManager] Invalid packet from"
                        << socket->peerAddress().toString() << ", clearing buffer";
            client.readBuffer.clear();
            return;
        }

        int totalSize = HEADER_SIZE + static_cast<int>(header.payloadLength);
        if (client.readBuffer.size() < totalSize) {
            // Need more data — wait
            return;
        }

        // Extract payload
        QByteArray payload = client.readBuffer.mid(HEADER_SIZE, static_cast<int>(header.payloadLength));
        client.readBuffer.remove(0, totalSize);

        MsgType type = static_cast<MsgType>(header.msgType & 0xFF);

        switch (type) {
        case MsgType::HELLO:
            handleHello(socket, payload);
            break;
        case MsgType::FRAME:
            handleFrame(socket, payload);
            break;
        case MsgType::PING:
            // Respond with PONG
            socket->write(createPacket(MsgType::PONG));
            break;
        case MsgType::PONG:
            // Keepalive response — already reset timeout above
            break;
        case MsgType::CHAT_MSG: {
            ChatData chat;
            if (parseChatPayload(payload, chat) && client.helloReceived) {
                emit chatReceived(client.info.id, chat.sender, chat.message);
            }
            break;
        }
        case MsgType::HELP_REQUEST: {
            HelpData help;
            if (parseHelpPayload(payload, help) && client.helloReceived) {
                emit helpRequestReceived(client.info.id, help.studentName, help.message);
            }
            break;
        }
        case MsgType::APP_STATUS: {
            if (client.helloReceived) {
                QJsonDocument doc = QJsonDocument::fromJson(payload);
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    client.info.activeApp = obj.value("app").toString();
                    client.info.activeAppClass = obj.value("class").toString();
                    emit appStatusReceived(client.info.id, client.info.activeApp, client.info.activeAppClass);
                }
            }
            break;
        }
        default:
            qWarning() << "[ConnectionManager] Unknown msg type:" << header.msgType;
            break;
        }
    }
}

void ConnectionManager::handleHello(QTcpSocket* socket, const QByteArray& payload)
{
    auto& client = m_clients[socket];

    if (!parseHelloPayload(payload, client.info)) {
        qWarning() << "[ConnectionManager] Failed to parse HELLO from"
                    << socket->peerAddress().toString();
        return;
    }

    client.info.ipAddress = socket->peerAddress().toString();
    client.info.id = client.info.hostname + "@" + client.info.ipAddress;
    client.info.online = true;
    client.helloReceived = true;

    qInfo() << "[ConnectionManager] HELLO from" << client.info.hostname
             << "(" << client.info.username << "@" << client.info.ipAddress << ")"
             << "screen:" << client.info.screenRes;

    // ACK
    socket->write(createPacket(MsgType::ACK));

    emit studentConnected(client.info);
}

void ConnectionManager::handleFrame(QTcpSocket* socket, const QByteArray& payload)
{
    auto& client = m_clients[socket];

    if (!client.helloReceived) {
        qWarning() << "[ConnectionManager] FRAME before HELLO from"
                    << socket->peerAddress().toString();
        return;
    }

    // Decode JPEG
    QImage image;
    if (!image.loadFromData(payload, "JPEG")) {
        qWarning() << "[ConnectionManager] Failed to decode JPEG frame from"
                    << client.info.hostname;
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    client.lastFrame = pixmap;
    client.info.lastFrameTime = QDateTime::currentMSecsSinceEpoch();

    emit frameReceived(client.info.id, pixmap);
}

void ConnectionManager::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    removeClient(socket);
}

void ConnectionManager::onClientTimeout()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;

    QTcpSocket* socket = static_cast<QTcpSocket*>(timer->property("socket").value<void*>());
    if (!socket || !m_clients.contains(socket)) return;

    qWarning() << "[ConnectionManager] Timeout for"
                << m_clients[socket].info.hostname
                << "(" << socket->peerAddress().toString() << ")";

    socket->disconnectFromHost();
    // removeClient will be called by onClientDisconnected
}

void ConnectionManager::removeClient(QTcpSocket* socket)
{
    if (!m_clients.contains(socket)) return;

    auto& client = m_clients[socket];
    QString id = client.info.id;

    qInfo() << "[ConnectionManager] Client disconnected:"
             << client.info.hostname << "(" << client.info.ipAddress << ")";

    if (client.timeoutTimer) {
        client.timeoutTimer->stop();
        delete client.timeoutTimer;
    }

    m_clients.remove(socket);
    socket->deleteLater();

    if (!id.isEmpty()) {
        emit studentDisconnected(id);
    }
}

QString ConnectionManager::clientId(QTcpSocket* socket) const
{
    if (m_clients.contains(socket)) {
        return m_clients[socket].info.id;
    }
    return {};
}

void ConnectionManager::sendMessage(const QStringList& studentIds, const QString& title,
                                    const QString& body, const QString& sender)
{
    QByteArray payload = createMessagePayload(title, body, sender);
    QByteArray packet = createPacket(MsgType::MESSAGE, payload);

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        if (studentIds.contains(it.value().info.id)) {
            it.key()->write(packet);
            it.key()->flush();
            qInfo() << "[ConnectionManager] Sent MESSAGE to" << it.value().info.hostname;
        }
    }
}

void ConnectionManager::sendMessageToAll(const QString& title, const QString& body,
                                         const QString& sender)
{
    QByteArray payload = createMessagePayload(title, body, sender);
    QByteArray packet = createPacket(MsgType::MESSAGE, payload);

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        it.key()->write(packet);
        it.key()->flush();
        qInfo() << "[ConnectionManager] Sent MESSAGE to" << it.value().info.hostname;
    }
}

// ── Lock/Unlock helpers ───────────────────────────────────────

void ConnectionManager::sendLockScreen(const QStringList& studentIds)
{
    QByteArray packet = createPacket(MsgType::LOCK_SCREEN);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        if (studentIds.contains(it.value().info.id)) {
            it.key()->write(packet);
            it.key()->flush();
            qInfo() << "[ConnectionManager] Sent LOCK to" << it.value().info.hostname;
        }
    }
}

void ConnectionManager::sendUnlockScreen(const QStringList& studentIds)
{
    QByteArray packet = createPacket(MsgType::UNLOCK_SCREEN);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        if (studentIds.contains(it.value().info.id)) {
            it.key()->write(packet);
            it.key()->flush();
            qInfo() << "[ConnectionManager] Sent UNLOCK to" << it.value().info.hostname;
        }
    }
}

void ConnectionManager::sendLockAll()
{
    QByteArray packet = createPacket(MsgType::LOCK_SCREEN);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        it.key()->write(packet);
        it.key()->flush();
    }
    qInfo() << "[ConnectionManager] Sent LOCK to ALL students";
}

void ConnectionManager::sendUnlockAll()
{
    QByteArray packet = createPacket(MsgType::UNLOCK_SCREEN);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        it.key()->write(packet);
        it.key()->flush();
    }
    qInfo() << "[ConnectionManager] Sent UNLOCK to ALL students";
}

// ── URL helpers ───────────────────────────────────────────────

void ConnectionManager::sendUrl(const QStringList& studentIds, const QString& url)
{
    QByteArray payload = createUrlPayload(url);
    QByteArray packet = createPacket(MsgType::SEND_URL, payload);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        if (studentIds.contains(it.value().info.id)) {
            it.key()->write(packet);
            it.key()->flush();
            qInfo() << "[ConnectionManager] Sent URL to" << it.value().info.hostname;
        }
    }
}

void ConnectionManager::sendUrlToAll(const QString& url)
{
    QByteArray payload = createUrlPayload(url);
    QByteArray packet = createPacket(MsgType::SEND_URL, payload);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        it.key()->write(packet);
        it.key()->flush();
    }
    qInfo() << "[ConnectionManager] Sent URL to ALL students";
}

// ── Chat helper ──────────────────────────────────────────────

void ConnectionManager::sendChatTo(const QString& studentId, const QString& sender,
                                   const QString& message)
{
    QByteArray payload = createChatPayload(sender, message);
    QByteArray packet = createPacket(MsgType::CHAT_MSG, payload);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        if (it.value().info.id == studentId) {
            it.key()->write(packet);
            it.key()->flush();
            qInfo() << "[ConnectionManager] Sent chat to" << it.value().info.hostname;
            break;
        }
    }
}

} // namespace LabMonitor
