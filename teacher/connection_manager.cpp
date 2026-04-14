#include "connection_manager.h"

#include <QDebug>
#include <QDateTime>
#include <QImage>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>

namespace LabMonitor {
static constexpr qint64 MAX_BUFFER_SIZE = 20 * 1024 * 1024;

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
        socket->write(createPacket(MsgType::ACK));
    }
}

void ConnectionManager::onClientReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_clients.contains(socket)) return;

    m_clients[socket].readBuffer.append(socket->readAll());
    if (m_clients[socket].readBuffer.size() > MAX_BUFFER_SIZE) {
        qWarning() << "[ConnectionManager] Buffer exceeded limit for"
                    << socket->peerAddress().toString() << ", disconnecting";
        socket->disconnectFromHost();
        return;
    }
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
        qint64 totalSize = static_cast<qint64>(HEADER_SIZE) + static_cast<qint64>(header.payloadLength);
        if (client.readBuffer.size() < totalSize) {
            return;
        }
        QByteArray payload = client.readBuffer.mid(HEADER_SIZE, static_cast<int>(header.payloadLength));
        client.readBuffer.remove(0, static_cast<int>(totalSize));

        MsgType type = static_cast<MsgType>(header.msgType & 0xFF);

        switch (type) {
        case MsgType::HELLO:
            handleHello(socket, payload);
            break;
        case MsgType::FRAME:
            handleFrame(socket, payload);
            break;
        case MsgType::PING:
            socket->write(createPacket(MsgType::PONG));
            break;
        case MsgType::PONG:
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
                    QPixmap appIcon;
                    QString iconB64 = obj.value("icon").toString();
                    if (!iconB64.isEmpty()) {
                        QByteArray iconData = QByteArray::fromBase64(iconB64.toUtf8());
                        appIcon.loadFromData(iconData, "PNG");
                    }

                    double cpuUsage = obj.value("cpuUsage").toDouble(-1.0);
                    double ramUsage = obj.value("ramUsage").toDouble(-1.0);

                    emit appStatusReceived(client.info.id, client.info.activeApp,
                                           client.info.activeAppClass, appIcon,
                                           cpuUsage, ramUsage);
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

void ConnectionManager::sendFile(const QStringList& studentIds, const QString& filePath, bool isFolder)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ConnectionManager] Cannot open file:" << filePath;
        return;
    }

    QString fileName = QFileInfo(filePath).fileName();
    qint64 fileSize = file.size();
    QList<QTcpSocket*> targets;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        if (studentIds.contains(it.value().info.id)) {
            targets.append(it.key());
        }
    }

    if (targets.isEmpty()) {
        qWarning() << "[ConnectionManager] No matching students for file transfer";
        return;
    }
    QByteArray startPayload = createFileStartPayload(fileName, fileSize, isFolder);
    QByteArray startPacket = createPacket(MsgType::TRANSFER_START, startPayload);
    for (auto* s : targets) {
        s->write(startPacket);
        s->flush();
    }
    QByteArray fileData = file.readAll();
    file.close();

    constexpr int CHUNK_SIZE = 512 * 1024;
    int totalChunks = (fileData.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;
    int chunkIndex = 0;

    qInfo() << "[ConnectionManager] Starting file transfer:" << fileName
             << "size:" << fileSize << "chunks:" << totalChunks;
    auto sharedData = QSharedPointer<QByteArray>::create(fileData);
    auto sharedTargets = QSharedPointer<QList<QTcpSocket*>>::create(targets);
    auto sharedFileName = QSharedPointer<QString>::create(fileName);
    auto sharedIsFolder = QSharedPointer<bool>::create(isFolder);
    auto sharedSendNextChunk = std::make_shared<std::function<void(int)>>();
    *sharedSendNextChunk = [this, sharedData, sharedTargets, sharedFileName, sharedIsFolder,
                            totalChunks, CHUNK_SIZE, sharedSendNextChunk](int idx) {
        if (idx >= totalChunks) {
            QByteArray endPayload = createFileEndPayload(*sharedFileName);
            QByteArray endPacket = createPacket(MsgType::TRANSFER_END, endPayload);
            for (auto* s : *sharedTargets) {
                if (s && s->state() == QAbstractSocket::ConnectedState) {
                    s->write(endPacket);
                    s->flush();
                }
            }
            qInfo() << "[ConnectionManager] File transfer complete:" << *sharedFileName;
            emit fileTransferComplete(*sharedFileName);
            return;
        }

        int offset = idx * CHUNK_SIZE;
        int len = qMin(CHUNK_SIZE, static_cast<int>(sharedData->size()) - offset);
        QByteArray chunk = sharedData->mid(offset, len);
        QByteArray chunkPacket = createPacket(MsgType::TRANSFER_CHUNK, chunk);

        for (auto* s : *sharedTargets) {
            if (s && s->state() == QAbstractSocket::ConnectedState) {
                s->write(chunkPacket);
                s->flush();
            }
        }

        int percent = static_cast<int>(((idx + 1) * 100) / totalChunks);
        emit fileTransferProgress(*sharedFileName, percent);
        QTimer::singleShot(5, this, [this, sharedSendNextChunk, idx]() {
            (*sharedSendNextChunk)(idx + 1);
        });
    };
    (*sharedSendNextChunk)(0);
}

void ConnectionManager::sendFileToAll(const QString& filePath, bool isFolder)
{
    QStringList allIds;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().helloReceived) {
            allIds.append(it.value().info.id);
        }
    }
    sendFile(allIds, filePath, isFolder);
}

}
