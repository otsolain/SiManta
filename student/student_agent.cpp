#include "student_agent.h"

#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QBuffer>
#include <QImage>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QNetworkDatagram>

#include <windows.h>
#include <psapi.h>
#include <shellapi.h>

namespace LabMonitor {
static constexpr qint64 MAX_BUFFER_SIZE = 50 * 1024 * 1024; // 50 MB (matched to protocol)

StudentAgent::StudentAgent(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_capturer(new ScreenCapturer(this))
    , m_captureTimer(new QTimer(this))
    , m_pingTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected,
            this, &StudentAgent::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &StudentAgent::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &StudentAgent::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &StudentAgent::onReadyRead);
    m_captureTimer->setTimerType(Qt::PreciseTimer);
    connect(m_captureTimer, &QTimer::timeout,
            this, &StudentAgent::captureAndSend);
    m_pingTimer->setInterval(PING_INTERVAL_MS);
    connect(m_pingTimer, &QTimer::timeout,
            this, &StudentAgent::sendPing);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &StudentAgent::attemptReconnect);
    connect(m_capturer, &ScreenCapturer::captureError,
            this, [this](const QString& err) {
        qWarning() << "[StudentAgent] Capture error:" << err;
    });
    m_lastPongTimer.start();
}

StudentAgent::~StudentAgent()
{
    stop();
}

void StudentAgent::setCaptureQuality(int quality)
{
    m_capturer->setQuality(quality);
    m_baseQuality = quality;
    m_adaptiveQuality = quality;
}

void StudentAgent::setCaptureScale(double scale)
{
    m_capturer->setScale(scale);
}

void StudentAgent::start()
{
    if (m_running) return;

    qInfo() << "[StudentAgent] Starting...";
    qInfo() << "[StudentAgent] Teacher:" << m_teacherHost << ":" << m_teacherPort;
    qInfo() << "[StudentAgent] Capture interval:" << m_captureInterval << "ms";
    qInfo() << "[StudentAgent] JPEG quality:" << m_capturer->quality();
    qInfo() << "[StudentAgent] Scale:" << m_capturer->scale();

    m_running = true;
    m_captureTimer->setInterval(m_captureInterval);
    resetReconnectBackoff();

    if (m_autoDiscovery) {
        startDiscovery();
    } else {
        connectToTeacher();
    }
}

void StudentAgent::stop()
{
    if (!m_running) return;

    qInfo() << "[StudentAgent] Stopping...";
    m_running = false;

    m_captureTimer->stop();
    m_pingTimer->stop();
    m_reconnectTimer->stop();
    stopDiscovery();

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }

    m_connected = false;
}

void StudentAgent::connectToTeacher()
{
    if (!m_running) return;

    qInfo() << "[StudentAgent] Connecting to" << m_teacherHost << ":" << m_teacherPort;
    m_socket->connectToHost(m_teacherHost, m_teacherPort);
}

void StudentAgent::onConnected()
{
    qInfo() << "[StudentAgent] Connected to teacher!";
    m_connected = true;
    m_readBuffer.clear();
    resetReconnectBackoff();

    // ── Socket tuning for reliable, high-throughput connection ──
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setReadBufferSize(8 * 1024 * 1024); // 8 MB read buffer

    // Reset adaptive quality state
    m_adaptiveQuality = m_baseQuality;
    m_consecutiveSkips = 0;
    m_missedPongs = 0;
    m_waitingForPong = false;
    m_appStatusCounter = 0;

    sendHello();
    m_captureTimer->start();
    m_pingTimer->start();

    // Stop discovery once connected — we found the teacher
    stopDiscovery();

    emit connected();
}

void StudentAgent::onDisconnected()
{
    qInfo() << "[StudentAgent] Disconnected from teacher";
    m_connected = false;

    m_captureTimer->stop();
    m_pingTimer->stop();

    emit disconnected();
    if (m_running && !m_reconnectTimer->isActive()) {
        if (m_autoDiscovery) {
            // Restart discovery to find teacher again (may have changed IP)
            qInfo() << "[StudentAgent] Restarting discovery after disconnect";
            startDiscovery();
        } else {
            qInfo() << "[StudentAgent] Reconnecting in" << m_reconnectDelay << "ms";
            m_reconnectTimer->start(m_reconnectDelay);
            m_reconnectDelay = qMin(m_reconnectDelay * 2, m_maxReconnectDelay);
        }
    }
}

void StudentAgent::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    if (m_connected) {
        qWarning() << "[StudentAgent] Socket error:" << m_socket->errorString();
    }
    if (!m_connected && m_running && !m_reconnectTimer->isActive()) {
        if (m_autoDiscovery) {
            qInfo() << "[StudentAgent] Connection failed, restarting discovery";
            startDiscovery();
        } else {
            qInfo() << "[StudentAgent] Connection failed, retrying in" << m_reconnectDelay << "ms";
            m_reconnectTimer->start(m_reconnectDelay);
            m_reconnectDelay = qMin(m_reconnectDelay * 2, m_maxReconnectDelay);
        }
    }
}

void StudentAgent::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());
    if (m_readBuffer.size() > MAX_BUFFER_SIZE) {
        qWarning() << "[StudentAgent] Buffer exceeded limit, clearing";
        m_readBuffer.clear();
        return;
    }

    processIncomingData();
}

void StudentAgent::processIncomingData()
{
    while (m_readBuffer.size() >= HEADER_SIZE) {
        PacketHeader header;
        if (!parseHeader(m_readBuffer.left(HEADER_SIZE), header)) {
            qWarning() << "[StudentAgent] Invalid packet header, clearing buffer";
            m_readBuffer.clear();
            return;
        }
        qint64 totalSize = static_cast<qint64>(HEADER_SIZE) + static_cast<qint64>(header.payloadLength);
        if (m_readBuffer.size() < totalSize) {
            return;
        }
        QByteArray payload = m_readBuffer.mid(HEADER_SIZE, static_cast<int>(header.payloadLength));
        m_readBuffer.remove(0, static_cast<int>(totalSize));

        MsgType type = static_cast<MsgType>(header.msgType & 0xFF);

        switch (type) {
        case MsgType::ACK:
            break;
        case MsgType::PING:
            if (m_socket->state() == QAbstractSocket::ConnectedState) {
                m_socket->write(createPacket(MsgType::PONG));
            }
            break;
        case MsgType::PONG:
            // ── Connection health confirmed ──
            m_waitingForPong = false;
            m_missedPongs = 0;
            // Restore quality if it was reduced due to missed PONGs
            if (m_adaptiveQuality < m_baseQuality && m_consecutiveSkips == 0) {
                m_adaptiveQuality = m_baseQuality;
                m_capturer->setQuality(m_adaptiveQuality);
            }
            break;
        case MsgType::CMD: {
            QString cmd = parseCmdPayload(payload);
            if (!cmd.isEmpty()) {
                qInfo() << "[StudentAgent] Executing CMD:" << cmd;
                emit cmdReceived(cmd);
            }
            break;
        }
        case MsgType::MESSAGE: {
            MessageData msg;
            if (parseMessagePayload(payload, msg)) {
                qInfo() << "[StudentAgent] Received MESSAGE:" << msg.title;
                emit messageReceived(msg.title, msg.body, msg.sender);
            } else {
                qWarning() << "[StudentAgent] Failed to parse MESSAGE payload";
            }
            break;
        }
        case MsgType::LOCK_SCREEN:
            qInfo() << "[StudentAgent] Screen LOCKED by teacher";
            emit lockScreenRequested();
            break;
        case MsgType::UNLOCK_SCREEN:
            qInfo() << "[StudentAgent] Screen UNLOCKED by teacher";
            emit unlockScreenRequested();
            break;
        case MsgType::SEND_URL: {
            QString url = parseUrlPayload(payload);
            if (!url.isEmpty()) {
                qInfo() << "[StudentAgent] Opening URL:" << url;
                emit urlReceived(url);
            }
            break;
        }
        case MsgType::CHAT_MSG: {
            ChatData chat;
            if (parseChatPayload(payload, chat)) {
                qInfo() << "[StudentAgent] Chat from" << chat.sender << ":" << chat.message;
                emit chatReceived(chat.sender, chat.message);
            }
            break;
        }
        case MsgType::TRANSFER_START:
            handleFileStart(payload);
            break;
        case MsgType::TRANSFER_CHUNK:
            handleFileChunk(payload);
            break;
        case MsgType::TRANSFER_END:
            handleFileEnd(payload);
            break;
        case MsgType::DIR_LIST_REQUEST:
            handleDirListRequest(payload);
            break;
        case MsgType::MKDIR_REQUEST:
            handleMkdirRequest(payload);
            break;
        case MsgType::FILE_RETRIEVE_REQUEST:
            handleFileRetrieveRequest(payload);
            break;
        case MsgType::SET_UPDATE_SPEED: {
            if (payload.size() >= sizeof(qint32)) {
                QDataStream stream(payload);
                stream.setVersion(QDataStream::Qt_6_0);
                qint32 ms;
                stream >> ms;
                m_captureInterval = ms;
                m_captureTimer->setInterval(m_captureInterval);
                qInfo() << "[StudentAgent] Capture interval set to:" << ms << "ms";
            }
            break;
        }
        case MsgType::QUALITY_HIGH:
            // ── Veyon-like: teacher opened fullscreen → switch to high-res fast mode ──
            qInfo() << "[StudentAgent] Switching to HIGH quality mode (fullscreen view)";
            m_capturer->setQuality(95);       // Near-lossless
            m_capturer->setScale(1.0);        // Full resolution
            m_captureTimer->setInterval(200); // ~5 FPS for smooth live view
            m_capturer->resetDelta();         // Force full frame on next capture
            break;
        case MsgType::QUALITY_NORMAL:
            // ── Teacher closed fullscreen → restore normal thumbnail mode ──
            qInfo() << "[StudentAgent] Switching to NORMAL quality mode (thumbnail view)";
            m_capturer->setQuality(m_adaptiveQuality);
            m_capturer->setScale(0.5);                   // Half resolution for thumbnails
            m_captureTimer->setInterval(m_captureInterval); // Original interval
            break;
        case MsgType::KICK:
            // Teacher forcefully disconnected us — stop reconnecting
            qInfo() << "[StudentAgent] KICKED by teacher — stopping reconnect";
            m_running = false;  // prevents onDisconnected() from auto-reconnecting
            m_captureTimer->stop();
            m_pingTimer->stop();
            m_reconnectTimer->stop();
            emit kicked();
            break;
        default:
            qWarning() << "[StudentAgent] Unknown message type:" << header.msgType;
            break;
        }
    }
}

void StudentAgent::sendHello()
{
    if (!m_connected) return;

    QByteArray payload = createHelloPayload(
        getLocalHostname(),
        getLocalUsername(),
        getOsString(),
        getScreenResolution()
    );

    QByteArray packet = createPacket(MsgType::HELLO, payload);
    m_socket->write(packet);

    qInfo() << "[StudentAgent] Sent HELLO (" << payload.size() << "bytes payload)";
}

void StudentAgent::captureAndSend()
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // ── Adaptive backpressure: skip frame if write buffer is congested ──
    qint64 pendingBytes = m_socket->bytesToWrite();
    if (pendingBytes > 10 * 1024 * 1024) {
        // Buffer critically full (>10 MB) — skip frame entirely
        m_consecutiveSkips++;
        if (m_consecutiveSkips % 5 == 0) {
            qInfo() << "[StudentAgent] Write buffer congested (" << pendingBytes / 1024 / 1024
                     << "MB), skipped" << m_consecutiveSkips << "frames";
        }
        // Reduce quality if we keep skipping
        if (m_consecutiveSkips > 3 && m_adaptiveQuality > 30) {
            m_adaptiveQuality = qMax(30, m_adaptiveQuality - 10);
            m_capturer->setQuality(m_adaptiveQuality);
            qInfo() << "[StudentAgent] Reduced quality to" << m_adaptiveQuality << "(backpressure)";
        }
        return;
    }

    // Gradually restore quality when buffer is healthy
    if (m_consecutiveSkips > 0 && pendingBytes < 2 * 1024 * 1024) {
        m_consecutiveSkips = 0;
        if (m_adaptiveQuality < m_baseQuality) {
            m_adaptiveQuality = qMin(m_baseQuality, m_adaptiveQuality + 5);
            m_capturer->setQuality(m_adaptiveQuality);
            qInfo() << "[StudentAgent] Restored quality to" << m_adaptiveQuality;
        }
    }

    // ── Veyon-like delta frame capture ──
    DeltaResult delta = m_capturer->captureDelta();

    // Nothing changed on screen → skip frame entirely (huge bandwidth saving!)
    if (delta.isEmpty) {
        // Still count for app status throttle
        m_appStatusCounter++;
        if (m_appStatusCounter >= 3) {
            m_appStatusCounter = 0;
            sendAppStatus();
        }
        return;
    }

    qint64 written = 0;
    if (delta.isFullFrame) {
        // Full frame (first frame, resolution change, or >60% tiles changed)
        QByteArray packet = createPacket(MsgType::FRAME, delta.fullJpeg);
        written = m_socket->write(packet);
    } else {
        // Delta frame — only send changed tiles
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << static_cast<uint16_t>(delta.tiles.size());
        for (const auto& tile : delta.tiles) {
            stream << tile.x << tile.y << tile.w << tile.h;
            stream << static_cast<uint32_t>(tile.jpegData.size());
            stream.writeRawData(tile.jpegData.constData(), tile.jpegData.size());
        }
        QByteArray packet = createPacket(MsgType::FRAME_DELTA, payload);
        written = m_socket->write(packet);
    }

    if (written > 0) {
        emit frameSent(static_cast<int>(written));
    }

    // ── Throttle app status to every 3rd frame to reduce overhead ──
    m_appStatusCounter++;
    if (m_appStatusCounter >= 3) {
        m_appStatusCounter = 0;
        sendAppStatus();
    }
}

void StudentAgent::sendAppStatus()
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) return;

    QString appName;
    QString appClass;
    QString exeFullPath;
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        wchar_t title[256];
        GetWindowTextW(hwnd, title, 256);
        appName = QString::fromWCharArray(title);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                wchar_t exePath[MAX_PATH];
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
                    exeFullPath = QString::fromWCharArray(exePath);
                    appClass = QFileInfo(exeFullPath).baseName(); // e.g. "firefox", "chrome"
                }
                CloseHandle(hProc);
            }
        }
    }

    if (appName.isEmpty() && appClass.isEmpty()) return;
    if (appName.length() > 80) {
        appName = appName.left(77) + "...";
    }

    QJsonObject obj;
    obj["app"]      = appName;
    obj["class"]    = appClass;
    obj["cpuUsage"] = getCpuUsage();
    obj["ramUsage"] = getRamUsage();
    static QString s_lastExePath;
    static QString s_cachedIconB64;

    if (!exeFullPath.isEmpty() && exeFullPath != s_lastExePath) {
        s_cachedIconB64.clear();
        s_lastExePath = exeFullPath;
        HICON hIconSmall = nullptr;
        ExtractIconExW(reinterpret_cast<const wchar_t*>(exeFullPath.utf16()), 0, nullptr, &hIconSmall, 1);
        if (hIconSmall) {
            ICONINFO iconInfo = {};
            if (GetIconInfo(hIconSmall, &iconInfo)) {
                BITMAP bm = {};
                GetObject(iconInfo.hbmColor, sizeof(bm), &bm);
                int w = bm.bmWidth;
                int h = bm.bmHeight;

                if (w > 0 && h > 0 && w <= 256 && h <= 256) {
                    HDC hDC = GetDC(nullptr);
                    HDC hMemDC = CreateCompatibleDC(hDC);

                    BITMAPINFOHEADER bi = {};
                    bi.biSize = sizeof(BITMAPINFOHEADER);
                    bi.biWidth = w;
                    bi.biHeight = -h; // top-down
                    bi.biPlanes = 1;
                    bi.biBitCount = 32;
                    bi.biCompression = BI_RGB;

                    QImage img(w, h, QImage::Format_ARGB32);
                    HGDIOBJ hOld = SelectObject(hMemDC, iconInfo.hbmColor);
                    GetDIBits(hMemDC, iconInfo.hbmColor, 0, h, img.bits(),
                              reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
                    SelectObject(hMemDC, hOld);

                    DeleteDC(hMemDC);
                    ReleaseDC(nullptr, hDC);
                    QByteArray pngData;
                    QBuffer pngBuf(&pngData);
                    pngBuf.open(QIODevice::WriteOnly);
                    img.save(&pngBuf, "PNG");
                    s_cachedIconB64 = QString::fromLatin1(pngData.toBase64());
                }

                if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
                if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
            }
            DestroyIcon(hIconSmall);
        }
    }

    if (!s_cachedIconB64.isEmpty()) {
        obj["icon"] = s_cachedIconB64;
    }

    QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray packet = createPacket(MsgType::APP_STATUS, payload);
    m_socket->write(packet);
}

void StudentAgent::sendPing()
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // ── PONG health monitoring ──
    if (m_waitingForPong) {
        m_missedPongs++;
        if (m_missedPongs >= 5) {
            qWarning() << "[StudentAgent] Missed" << m_missedPongs
                        << "PONGs — connection may be dead";
            // Don't disconnect — let the TCP timeout handle it
            // But reduce quality to minimize bandwidth
            if (m_adaptiveQuality > 30) {
                m_adaptiveQuality = 30;
                m_capturer->setQuality(m_adaptiveQuality);
            }
        }
    }

    m_waitingForPong = true;
    m_lastPongTimer.restart();
    m_socket->write(createPacket(MsgType::PING));
}

void StudentAgent::attemptReconnect()
{
    if (!m_running) return;

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    connectToTeacher();
}

void StudentAgent::resetReconnectBackoff()
{
    m_reconnectDelay = 1000;
}

void StudentAgent::startDiscovery()
{
    if (!m_discoverySocket) {
        m_discoverySocket = new QUdpSocket(this);
        connect(m_discoverySocket, &QUdpSocket::readyRead,
                this, &StudentAgent::onDiscoveryReadyRead);
    }

    if (m_discoverySocket->state() != QAbstractSocket::BoundState) {
        // Bind to DISCOVERY_PORT to receive teacher beacons
        if (m_discoverySocket->bind(QHostAddress::AnyIPv4, DISCOVERY_PORT,
                                      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            qInfo() << "[StudentAgent] Discovery listening on UDP port" << DISCOVERY_PORT;
        } else {
            qWarning() << "[StudentAgent] Failed to bind discovery socket:"
                        << m_discoverySocket->errorString();
            // Fallback: try direct TCP connection
            connectToTeacher();
        }
    }
}

void StudentAgent::stopDiscovery()
{
    if (m_discoverySocket) {
        m_discoverySocket->close();
        qInfo() << "[StudentAgent] Discovery stopped";
    }
}

void StudentAgent::onDiscoveryReadyRead()
{
    while (m_discoverySocket && m_discoverySocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_discoverySocket->receiveDatagram();
        QByteArray data = datagram.data();

        // Minimum size: 4 (magic) + 2 (port) = 6 bytes
        if (data.size() < 6) continue;

        QDataStream stream(data);
        stream.setByteOrder(QDataStream::LittleEndian);

        uint32_t magic;
        uint16_t tcpPort;
        stream >> magic >> tcpPort;

        if (magic != DISCOVERY_MAGIC) continue;

        // Extract teacher hostname from remaining bytes
        QByteArray hostnameBytes = data.mid(6);
        QString teacherHostname = QString::fromUtf8(hostnameBytes);

        // Get teacher IP from datagram sender
        QHostAddress senderAddr = datagram.senderAddress();
        QString teacherIp = senderAddr.toString();

        // Normalize IPv4-mapped IPv6 addresses (e.g., "::ffff:192.168.1.5")
        if (senderAddr.protocol() == QAbstractSocket::IPv6Protocol) {
            bool ok = false;
            quint32 ipv4 = senderAddr.toIPv4Address(&ok);
            if (ok) {
                teacherIp = QHostAddress(ipv4).toString();
            }
        }

        // Skip our own beacons (ignore if sender is localhost)
        if (teacherIp == "127.0.0.1" || teacherIp == "0.0.0.0") continue;

        qInfo() << "[StudentAgent] Discovered teacher:" << teacherHostname
                 << "at" << teacherIp << ":" << tcpPort;

        // Update teacher host/port and connect
        m_teacherHost = teacherIp;
        m_teacherPort = tcpPort;
        stopDiscovery();
        resetReconnectBackoff();

        emit teacherDiscovered(teacherIp, tcpPort, teacherHostname);

        // Connect via TCP now that we know the teacher IP
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
        connectToTeacher();
        return; // Only process the first valid beacon
    }
}

void StudentAgent::sendChat(const QString& message)
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray payload = createChatPayload(getLocalHostname(), message);
    QByteArray packet = createPacket(MsgType::CHAT_MSG, payload);
    m_socket->write(packet);
    qInfo() << "[StudentAgent] Sent chat message";
}

void StudentAgent::sendHelpRequest(const QString& message)
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray payload = createHelpPayload(getLocalHostname(), message);
    QByteArray packet = createPacket(MsgType::HELP_REQUEST, payload);
    m_socket->write(packet);
    qInfo() << "[StudentAgent] Sent help request";
}

void StudentAgent::handleFileStart(const QByteArray& payload)
{
    FileStartData fsd;
    if (!parseFileStartPayload(payload, fsd)) {
        qWarning() << "[StudentAgent] Failed to parse FILE_START";
        return;
    }

    // Use destPath if provided, otherwise default to Downloads/Simanta
    QString targetDir;
    if (!fsd.destPath.isEmpty()) {
        targetDir = fsd.destPath;
    } else {
        QString downloadsDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        targetDir = downloadsDir + "/Simanta";
    }
    QDir().mkpath(targetDir);

    QString filePath = targetDir + "/" + fsd.fileName;
    if (m_incomingFile) {
        m_incomingFile->close();
        delete m_incomingFile;
        m_incomingFile = nullptr;
    }

    m_incomingFile = new QFile(filePath, this);
    if (!m_incomingFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[StudentAgent] Cannot open file for writing:" << filePath;
        delete m_incomingFile;
        m_incomingFile = nullptr;
        return;
    }

    m_incomingFileName = fsd.fileName;
    m_incomingFileSize = fsd.fileSize;
    m_incomingReceived = 0;
    m_incomingIsFolder = fsd.isFolder;

    qInfo() << "[StudentAgent] Receiving file:" << fsd.fileName
             << "->" << targetDir
             << "size:" << fsd.fileSize << "isFolder:" << fsd.isFolder;

    emit fileReceiveStarted(fsd.fileName, fsd.fileSize, fsd.isFolder);
}

void StudentAgent::handleFileChunk(const QByteArray& payload)
{
    if (!m_incomingFile || !m_incomingFile->isOpen()) {
        qWarning() << "[StudentAgent] FILE_CHUNK but no active transfer";
        return;
    }

    m_incomingFile->write(payload);
    m_incomingReceived += payload.size();
    if (m_incomingFileSize > 0) {
        int percent = static_cast<int>((m_incomingReceived * 100) / m_incomingFileSize);
        emit fileReceiveProgress(m_incomingFileName, qBound(0, percent, 100));
    }
}

void StudentAgent::handleFileEnd(const QByteArray& payload)
{
    Q_UNUSED(payload)

    if (!m_incomingFile || !m_incomingFile->isOpen()) {
        qWarning() << "[StudentAgent] FILE_END but no active transfer";
        return;
    }

    QString filePath = m_incomingFile->fileName();
    m_incomingFile->close();
    delete m_incomingFile;
    m_incomingFile = nullptr;

    qInfo() << "[StudentAgent] File received:" << filePath
             << "(" << m_incomingReceived << "bytes)";

    QString savePath = filePath;
    bool wasFolder = m_incomingIsFolder;
    if (m_incomingIsFolder && filePath.endsWith(".zip", Qt::CaseInsensitive)) {
        QString extractDir = filePath;
        extractDir.chop(4); // remove .zip
        QDir().mkpath(extractDir);
        QProcess* proc = new QProcess(this);
        QString fName = m_incomingFileName;
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, filePath, extractDir, fName](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                QFile::remove(filePath); // delete temp zip
                qInfo() << "[StudentAgent] Folder extracted to:" << extractDir;
            } else {
                qWarning() << "[StudentAgent] Extract failed, exit code:" << exitCode;
            }
            proc->deleteLater();
            emit fileReceiveCompleted(fName, extractDir, true);
        });

        proc->start("powershell", QStringList()
            << "-NoProfile" << "-Command"
            << QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
               .arg(filePath, extractDir));
    } else {
        emit fileReceiveCompleted(m_incomingFileName, savePath, false);
    }
}

double StudentAgent::getCpuUsage()
{
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return -1.0;
    }

    auto toU64 = [](const FILETIME& ft) -> quint64 {
        return (static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    };

    quint64 idle   = toU64(idleTime);
    quint64 kernel = toU64(kernelTime);
    quint64 user   = toU64(userTime);

    if (!m_cpuInitialized) {
        m_lastCpuIdle   = idle;
        m_lastCpuKernel = kernel;
        m_lastCpuUser   = user;
        m_cpuInitialized = true;
        return 0.0;
    }

    quint64 dIdle   = idle   - m_lastCpuIdle;
    quint64 dKernel = kernel - m_lastCpuKernel;
    quint64 dUser   = user   - m_lastCpuUser;

    m_lastCpuIdle   = idle;
    m_lastCpuKernel = kernel;
    m_lastCpuUser   = user;

    quint64 total = dKernel + dUser;
    if (total == 0) return 0.0;

    double usage = (1.0 - (static_cast<double>(dIdle) / static_cast<double>(total))) * 100.0;
    return qBound(0.0, usage, 100.0);
}

double StudentAgent::getRamUsage()
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (!GlobalMemoryStatusEx(&memInfo)) {
        return -1.0;
    }
    return static_cast<double>(memInfo.dwMemoryLoad); // percentage 0-100
}

void StudentAgent::handleDirListRequest(const QByteArray& payload)
{
    QString path = parseDirListRequest(payload);
    if (path.isEmpty()) {
        // Default to user's home directory
        path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }

    qInfo() << "[StudentAgent] DIR_LIST_REQUEST for:" << path;

    QDir dir(path);
    QJsonArray entries;

    if (dir.exists()) {
        // Add parent directory entry
        if (path != "/" && path.length() > 3) {  // Not root (e.g. C:\)
            QJsonObject parentEntry;
            parentEntry["name"] = "..";
            parentEntry["isDir"] = true;
            parentEntry["size"] = 0;
            entries.append(parentEntry);
        }

        // Add directories first, then files
        auto dirList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const auto& fi : dirList) {
            QJsonObject entry;
            entry["name"] = fi.fileName();
            entry["isDir"] = true;
            entry["size"] = 0;
            entries.append(entry);
        }

        auto fileList = dir.entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
        for (const auto& fi : fileList) {
            QJsonObject entry;
            entry["name"] = fi.fileName();
            entry["isDir"] = false;
            entry["size"] = fi.size();
            entries.append(entry);
        }
    }

    QByteArray response = createDirListResponse(path, entries);
    m_socket->write(createPacket(MsgType::DIR_LIST_RESPONSE, response));
    m_socket->flush();
}

void StudentAgent::handleMkdirRequest(const QByteArray& payload)
{
    QString path = parseMkdirRequest(payload);
    if (path.isEmpty()) return;

    qInfo() << "[StudentAgent] MKDIR_REQUEST:" << path;

    bool ok = QDir().mkpath(path);
    qInfo() << "[StudentAgent] mkdir result:" << (ok ? "success" : "failed");
}

void StudentAgent::handleFileRetrieveRequest(const QByteArray& payload)
{
    QString filePath = parseFileRetrieveRequest(payload);
    if (filePath.isEmpty()) return;

    qInfo() << "[StudentAgent] FILE_RETRIEVE_REQUEST:" << filePath;

    // Clean up any previous retrieve in progress
    if (m_retrieveFile) {
        m_retrieveFile->close();
        delete m_retrieveFile;
        m_retrieveFile = nullptr;
    }

    m_retrieveFile = new QFile(filePath, this);
    if (!m_retrieveFile->exists() || !m_retrieveFile->open(QIODevice::ReadOnly)) {
        qWarning() << "[StudentAgent] Cannot open file for retrieval:" << filePath;
        delete m_retrieveFile;
        m_retrieveFile = nullptr;
        return;
    }

    m_retrieveFileName = QFileInfo(filePath).fileName();
    m_retrieveFileSize = m_retrieveFile->size();

    // Send start
    QByteArray startPayload = createFileRetrieveStart(m_retrieveFileName, m_retrieveFileSize);
    m_socket->write(createPacket(MsgType::FILE_RETRIEVE_START, startPayload));

    // Send first chunk asynchronously (yields event loop between chunks)
    QTimer::singleShot(0, this, &StudentAgent::sendNextRetrieveChunk);
}

void StudentAgent::sendNextRetrieveChunk()
{
    if (!m_retrieveFile || !m_retrieveFile->isOpen()) return;
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) {
        // Connection lost during transfer, clean up
        m_retrieveFile->close();
        delete m_retrieveFile;
        m_retrieveFile = nullptr;
        return;
    }

    // ── Increased chunk size for faster transfers (512 KB, was 64 KB) ──
    constexpr int CHUNK_SIZE = 512 * 1024;
    QByteArray chunk = m_retrieveFile->read(CHUNK_SIZE);

    if (!chunk.isEmpty()) {
        m_socket->write(createPacket(MsgType::FILE_RETRIEVE_CHUNK, chunk));
    }

    if (m_retrieveFile->atEnd() || chunk.isEmpty()) {
        // Send end
        QByteArray endPayload = createFileEndPayload(m_retrieveFileName);
        m_socket->write(createPacket(MsgType::FILE_RETRIEVE_END, endPayload));

        qInfo() << "[StudentAgent] File sent to teacher:" << m_retrieveFileName
                 << "(" << m_retrieveFileSize << "bytes)";

        m_retrieveFile->close();
        delete m_retrieveFile;
        m_retrieveFile = nullptr;
    } else {
        // Adaptive delay: back off if socket write buffer is filling up
        int delay = (m_socket->bytesToWrite() > 2 * 1024 * 1024) ? 20 : 1;
        QTimer::singleShot(delay, this, &StudentAgent::sendNextRetrieveChunk);
    }
}

QString StudentAgent::parseCmdPayload(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_0);
    QString cmd;
    stream >> cmd;
    return cmd;
}

} // namespace LabMonitor

