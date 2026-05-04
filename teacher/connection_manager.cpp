#include "connection_manager.h"

#include <QDebug>
#include <QDateTime>
#include <QImage>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QNetworkInterface>
#include <QPainter>
#include <QPointer>

namespace LabMonitor {
static constexpr qint64 MAX_BUFFER_SIZE = 50 * 1024 * 1024; // 50 MB (matched to protocol)

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

    // Increase server socket buffer for high-throughput scenarios
    m_server->setMaxPendingConnections(MAX_CONNECTIONS);

    qInfo() << "[ConnectionManager] Listening on port" << port;
    emit listeningStarted(port);

    // Start UDP beacon for auto-discovery
    startBeacon(port);

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

    stopBeacon();
}

// ── Beacon packet: broadcast teacher presence for auto-discovery ──
void ConnectionManager::sendBeaconPacket()
{
    if (!m_beaconSocket) return;

    // Build beacon datagram: [MAGIC 4B][TCP_PORT 2B][HOSTNAME utf8]
    QByteArray datagram;
    QDataStream stream(&datagram, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << DISCOVERY_MAGIC;
    stream << m_beaconTcpPort;

    QString hostname = getLocalHostname();
    datagram.append(hostname.toUtf8());

    // Broadcast to 255.255.255.255 AND to all subnet broadcast addresses
    m_beaconSocket->writeDatagram(datagram, QHostAddress::Broadcast, DISCOVERY_PORT);

    // Also send to all local subnet broadcast addresses for better hotspot compatibility
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            iface.flags().testFlag(QNetworkInterface::IsRunning) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            const auto entries = iface.addressEntries();
            for (const auto& entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QHostAddress broadcast = entry.broadcast();
                    if (!broadcast.isNull()) {
                        m_beaconSocket->writeDatagram(datagram, broadcast, DISCOVERY_PORT);
                    }
                }
            }
        }
    }
}

void ConnectionManager::startBeacon(uint16_t tcpPort)
{
    m_beaconTcpPort = tcpPort;

    if (!m_beaconSocket) {
        m_beaconSocket = new QUdpSocket(this);
        // Enable broadcast on the socket
        m_beaconSocket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 0);
    }

    if (!m_beaconTimer) {
        m_beaconTimer = new QTimer(this);
        m_beaconTimer->setInterval(DISCOVERY_INTERVAL);
        connect(m_beaconTimer, &QTimer::timeout,
                this, &ConnectionManager::sendBeaconPacket);
    }

    m_beaconTimer->start();

    // ── FIX: Send first beacon IMMEDIATELY (don't wait for timer interval) ──
    sendBeaconPacket();

    qInfo() << "[ConnectionManager] UDP beacon started on port" << DISCOVERY_PORT
             << "(advertising TCP port" << tcpPort << ")";
}

void ConnectionManager::stopBeacon()
{
    if (m_beaconTimer) {
        m_beaconTimer->stop();
    }
    if (m_beaconSocket) {
        m_beaconSocket->close();
    }
    qInfo() << "[ConnectionManager] UDP beacon stopped";
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
                        << socket->peerAddress().toString().remove("::ffff:");
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        qInfo() << "[ConnectionManager] New connection from"
                 << socket->peerAddress().toString().remove("::ffff:") << ":" << socket->peerPort();

        ClientState state;
        state.socket = socket;
        state.info.ipAddress = socket->peerAddress().toString().remove("::ffff:");
        state.timeoutTimer = new QTimer(this);
        state.timeoutTimer->setSingleShot(true);
        state.timeoutTimer->setInterval(TIMEOUT_MS);
        connect(state.timeoutTimer, &QTimer::timeout,
                this, &ConnectionManager::onClientTimeout);
        state.timeoutTimer->setProperty("socket", QVariant::fromValue<void*>(socket));
        state.timeoutTimer->start();

        m_clients[socket] = state;

        // ── Socket tuning for reliable, low-latency connections ──
        socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

        // Increase socket receive buffer for large frame data (8 MB)
        socket->setReadBufferSize(8 * 1024 * 1024);

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
                    << socket->peerAddress().toString().remove("::ffff:") << ", disconnecting to resync";
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
                        << socket->peerAddress().toString().remove("::ffff:") << ", disconnecting to resync";
            socket->disconnectFromHost();
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
        case MsgType::FRAME_DELTA:
            handleDeltaFrame(socket, payload);
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
        case MsgType::DIR_LIST_RESPONSE: {
            if (client.helloReceived) {
                QJsonDocument doc = QJsonDocument::fromJson(payload);
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    QString path = obj.value("path").toString();
                    QJsonArray entries = obj.value("entries").toArray();
                    emit dirListReceived(client.info.id, path, entries);
                }
            }
            break;
        }
        case MsgType::FILE_RETRIEVE_START: {
            if (client.helloReceived) {
                QJsonDocument doc = QJsonDocument::fromJson(payload);
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    QString fileName = obj.value("fileName").toString();
                    qint64 fileSize = obj.value("fileSize").toVariant().toLongLong();
                    emit fileRetrieveStarted(client.info.id, fileName, fileSize);
                }
            }
            break;
        }
        case MsgType::FILE_RETRIEVE_CHUNK: {
            if (client.helloReceived) {
                emit fileRetrieveChunk(client.info.id, payload);
            }
            break;
        }
        case MsgType::FILE_RETRIEVE_END: {
            if (client.helloReceived) {
                QString fileName = parseFileEndPayload(payload);
                emit fileRetrieveCompleted(client.info.id, fileName);
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
                    << socket->peerAddress().toString().remove("::ffff:");
        return;
    }

    client.info.ipAddress = socket->peerAddress().toString().remove("::ffff:");
    client.info.id = client.info.hostname + "@" + client.info.ipAddress;
    client.info.online = true;
    client.helloReceived = true;

    // ── FIX: Detect duplicate student ID (same student reconnecting) ──
    // If another socket already has this ID, remove the old one first
    QList<QTcpSocket*> toRemove;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.key() != socket && it.value().helloReceived && it.value().info.id == client.info.id) {
            toRemove.append(it.key());
        }
    }
    for (auto* oldSocket : toRemove) {
        qInfo() << "[ConnectionManager] Replacing duplicate connection for" << client.info.id;
        removeClient(oldSocket);
    }

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
                    << socket->peerAddress().toString().remove("::ffff:");
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

void ConnectionManager::handleDeltaFrame(QTcpSocket* socket, const QByteArray& payload)
{
    auto& client = m_clients[socket];

    if (!client.helloReceived) {
        qWarning() << "[ConnectionManager] FRAME_DELTA before HELLO from"
                    << socket->peerAddress().toString().remove("::ffff:");
        return;
    }

    // Need a previous frame to apply delta onto
    if (client.lastFrame.isNull()) {
        qWarning() << "[ConnectionManager] FRAME_DELTA but no previous frame for"
                    << client.info.hostname << "— requesting full frame";
        return;
    }

    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint16_t tileCount;
    stream >> tileCount;

    if (tileCount == 0 || tileCount > 10000) return;

    // Paint changed tiles onto previous frame
    QPixmap frame = client.lastFrame;
    QPainter painter(&frame);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    for (int i = 0; i < tileCount && stream.status() == QDataStream::Ok; i++) {
        uint16_t x, y, w, h;
        uint32_t jpegLen;
        stream >> x >> y >> w >> h >> jpegLen;

        if (jpegLen == 0 || jpegLen > 10 * 1024 * 1024) break;

        QByteArray jpegData(static_cast<int>(jpegLen), Qt::Uninitialized);
        if (stream.readRawData(jpegData.data(), static_cast<int>(jpegLen)) != static_cast<int>(jpegLen)) {
            break;
        }

        QImage tileImg;
        if (tileImg.loadFromData(jpegData, "JPEG")) {
            painter.drawImage(x, y, tileImg);
        }
    }
    painter.end();

    client.lastFrame = frame;
    client.info.lastFrameTime = QDateTime::currentMSecsSinceEpoch();

    emit frameReceived(client.info.id, frame);
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

    // ── FIX: Send a PING before disconnecting (grace period) ──
    // Give the student one last chance to respond
    if (socket->state() == QAbstractSocket::ConnectedState) {
        qWarning() << "[ConnectionManager] Timeout for"
                    << m_clients[socket].info.hostname
                    << "(" << socket->peerAddress().toString().remove("::ffff:") << ")"
                    << "— sending final PING before disconnect";
        socket->write(createPacket(MsgType::PING));
        socket->flush();

        // Give 10 more seconds for the PONG to come back
        QPointer<QTcpSocket> safeSocket(socket);
        QTimer::singleShot(10000, this, [this, safeSocket]() {
            if (safeSocket && m_clients.contains(safeSocket.data())) {
                qWarning() << "[ConnectionManager] Final timeout — disconnecting"
                            << m_clients[safeSocket.data()].info.hostname;
                safeSocket->disconnectFromHost();
            }
        });
    } else {
        socket->disconnectFromHost();
    }
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
    }
    qInfo() << "[ConnectionManager] Sent LOCK to ALL students";
}

void ConnectionManager::sendUnlockAll()
{
    QByteArray packet = createPacket(MsgType::UNLOCK_SCREEN);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        it.key()->write(packet);
    }
    qInfo() << "[ConnectionManager] Sent UNLOCK to ALL students";
}

void ConnectionManager::sendCommand(const QStringList& studentIds, const QString& cmd)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << cmd;

    QByteArray packet = createPacket(MsgType::CMD, payload);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        if (studentIds.contains(it.value().info.id)) {
            it.key()->write(packet);
        }
    }
}

void ConnectionManager::sendCommandToAll(const QString& cmd)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << cmd;

    QByteArray packet = createPacket(MsgType::CMD, payload);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        it.key()->write(packet);
    }
}

void ConnectionManager::broadcastUpdateSpeed(int ms)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<qint32>(ms);

    QByteArray packet = createPacket(MsgType::SET_UPDATE_SPEED, payload);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        it.key()->write(packet);
    }
    qInfo() << "[ConnectionManager] Broadcast UPDATE_SPEED:" << ms << "ms";
}

void ConnectionManager::sendUrl(const QStringList& studentIds, const QString& url)
{
    QByteArray payload = createUrlPayload(url);
    QByteArray packet = createPacket(MsgType::SEND_URL, payload);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value().helloReceived) continue;
        if (studentIds.contains(it.value().info.id)) {
            it.key()->write(packet);
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
            qInfo() << "[ConnectionManager] Sent chat to" << it.value().info.hostname;
            break;
        }
    }
}

void ConnectionManager::sendFile(const QStringList& studentIds, const QString& filePath, bool isFolder, const QString& destPath)
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
    QByteArray startPayload = createFileStartPayload(fileName, fileSize, isFolder, destPath);
    QByteArray startPacket = createPacket(MsgType::TRANSFER_START, startPayload);
    for (auto* s : targets) {
        s->write(startPacket);
    }
    QByteArray fileData = file.readAll();
    file.close();

    // ── Increased chunk size for faster transfers (1 MB) ──
    constexpr int CHUNK_SIZE = 1024 * 1024; // 1 MB chunks (was 512 KB)
    int totalChunks = (fileData.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;

    qInfo() << "[ConnectionManager] Starting file transfer:" << fileName
             << "size:" << fileSize << "chunks:" << totalChunks
             << "dest:" << (destPath.isEmpty() ? "default" : destPath);
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
            }
        }

        int percent = static_cast<int>(((idx + 1) * 100) / totalChunks);
        emit fileTransferProgress(*sharedFileName, percent);

        // ── Adaptive delay: wait longer if sockets have large write buffers ──
        int delay = 2;
        for (auto* s : *sharedTargets) {
            if (s && s->bytesToWrite() > 4 * 1024 * 1024) {
                delay = 50; // Back off if write buffer is getting full
                break;
            }
        }
        QTimer::singleShot(delay, this, [this, sharedSendNextChunk, idx]() {
            (*sharedSendNextChunk)(idx + 1);
        });
    };
    (*sharedSendNextChunk)(0);
}

void ConnectionManager::sendFileToAll(const QString& filePath, bool isFolder, const QString& destPath)
{
    QStringList allIds;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().helloReceived) {
            allIds.append(it.value().info.id);
        }
    }
    sendFile(allIds, filePath, isFolder, destPath);
}

void ConnectionManager::sendDirListRequest(const QString& studentId, const QString& path)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().helloReceived && it.value().info.id == studentId) {
            QByteArray payload = createDirListRequest(path);
            it.key()->write(createPacket(MsgType::DIR_LIST_REQUEST, payload));
            return;
        }
    }
}

void ConnectionManager::sendMkdirRequest(const QString& studentId, const QString& path)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().helloReceived && it.value().info.id == studentId) {
            QByteArray payload = createMkdirRequest(path);
            it.key()->write(createPacket(MsgType::MKDIR_REQUEST, payload));
            return;
        }
    }
}

void ConnectionManager::sendFileRetrieveRequest(const QString& studentId, const QString& filePath)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().helloReceived && it.value().info.id == studentId) {
            QByteArray payload = createFileRetrieveRequest(filePath);
            it.key()->write(createPacket(MsgType::FILE_RETRIEVE_REQUEST, payload));
            return;
        }
    }
}

void ConnectionManager::sendQualityHigh(const QString& studentId)
{
    QByteArray packet = createPacket(MsgType::QUALITY_HIGH);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().helloReceived && it.value().info.id == studentId) {
            it.key()->write(packet);
            qInfo() << "[ConnectionManager] Sent QUALITY_HIGH to" << it.value().info.hostname;
            return;
        }
    }
}

void ConnectionManager::sendQualityNormal(const QString& studentId)
{
    QByteArray packet = createPacket(MsgType::QUALITY_NORMAL);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().helloReceived && it.value().info.id == studentId) {
            it.key()->write(packet);
            qInfo() << "[ConnectionManager] Sent QUALITY_NORMAL to" << it.value().info.hostname;
            return;
        }
    }
}

void ConnectionManager::disconnectStudent(const QString& studentId)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().helloReceived && it.value().info.id == studentId) {
            qInfo() << "[ConnectionManager] Teacher kicking student:" << it.value().info.hostname;
            QTcpSocket* socket = it.key();
            // Send KICK so student stops auto-reconnecting
            socket->write(createPacket(MsgType::KICK));
            socket->flush();
            // Disconnect after a brief delay to ensure KICK is sent
            QPointer<QTcpSocket> safeSocket(socket);
            QTimer::singleShot(200, this, [this, safeSocket]() {
                if (safeSocket && m_clients.contains(safeSocket.data())) {
                    safeSocket->disconnectFromHost();
                }
            });
            return;
        }
    }
}

} // namespace LabMonitor
