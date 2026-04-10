#include "student_agent.h"

#include <QDebug>
#include <QDateTime>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace LabMonitor {

StudentAgent::StudentAgent(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_capturer(new ScreenCapturer(this))
    , m_captureTimer(new QTimer(this))
    , m_pingTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
{
    // Socket signals
    connect(m_socket, &QTcpSocket::connected,
            this, &StudentAgent::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &StudentAgent::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &StudentAgent::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &StudentAgent::onReadyRead);

    // Capture timer
    m_captureTimer->setTimerType(Qt::PreciseTimer);
    connect(m_captureTimer, &QTimer::timeout,
            this, &StudentAgent::captureAndSend);

    // Ping timer
    m_pingTimer->setInterval(PING_INTERVAL_MS);
    connect(m_pingTimer, &QTimer::timeout,
            this, &StudentAgent::sendPing);

    // Reconnect timer (single shot)
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &StudentAgent::attemptReconnect);

    // Capturer error
    connect(m_capturer, &ScreenCapturer::captureError,
            this, [this](const QString& err) {
        qWarning() << "[StudentAgent] Capture error:" << err;
    });
}

StudentAgent::~StudentAgent()
{
    stop();
}

void StudentAgent::setCaptureQuality(int quality)
{
    m_capturer->setQuality(quality);
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

    if (!ScreenCapturer::isGrimAvailable()) {
        qCritical() << "[StudentAgent] grim is not installed! Cannot capture screen.";
        emit error("grim is not installed");
        return;
    }

    m_running = true;
    m_captureTimer->setInterval(m_captureInterval);
    resetReconnectBackoff();
    connectToTeacher();
}

void StudentAgent::stop()
{
    if (!m_running) return;

    qInfo() << "[StudentAgent] Stopping...";
    m_running = false;

    m_captureTimer->stop();
    m_pingTimer->stop();
    m_reconnectTimer->stop();

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

    sendHello();

    // Start capture and ping timers
    m_captureTimer->start();
    m_pingTimer->start();

    emit connected();
}

void StudentAgent::onDisconnected()
{
    qInfo() << "[StudentAgent] Disconnected from teacher";
    m_connected = false;

    m_captureTimer->stop();
    m_pingTimer->stop();

    emit disconnected();

    // Schedule reconnect
    if (m_running) {
        qInfo() << "[StudentAgent] Reconnecting in" << m_reconnectDelay << "ms";
        m_reconnectTimer->start(m_reconnectDelay);

        // Exponential backoff
        m_reconnectDelay = qMin(m_reconnectDelay * 2, m_maxReconnectDelay);
    }
}

void StudentAgent::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    if (m_connected) {
        qWarning() << "[StudentAgent] Socket error:" << m_socket->errorString();
    }

    // If not connected yet, trigger reconnect
    if (!m_connected && m_running) {
        qInfo() << "[StudentAgent] Connection failed, retrying in" << m_reconnectDelay << "ms";
        m_reconnectTimer->start(m_reconnectDelay);
        m_reconnectDelay = qMin(m_reconnectDelay * 2, m_maxReconnectDelay);
    }
}

void StudentAgent::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());
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

        int totalSize = HEADER_SIZE + static_cast<int>(header.payloadLength);
        if (m_readBuffer.size() < totalSize) {
            // Need more data
            return;
        }

        // Extract payload
        QByteArray payload = m_readBuffer.mid(HEADER_SIZE, static_cast<int>(header.payloadLength));
        m_readBuffer.remove(0, totalSize);

        MsgType type = static_cast<MsgType>(header.msgType & 0xFF);

        switch (type) {
        case MsgType::ACK:
            // Teacher acknowledged
            break;
        case MsgType::PING:
            // Respond with PONG
            if (m_socket->state() == QAbstractSocket::ConnectedState) {
                m_socket->write(createPacket(MsgType::PONG));
            }
            break;
        case MsgType::PONG:
            // Keepalive response received
            break;
        case MsgType::CMD:
            // Future: handle commands
            qInfo() << "[StudentAgent] Received CMD (not yet implemented)";
            break;
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
    m_socket->flush();

    qInfo() << "[StudentAgent] Sent HELLO (" << payload.size() << "bytes payload)";
}

void StudentAgent::captureAndSend()
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray jpegData = m_capturer->capture();
    if (jpegData.isEmpty()) {
        return;
    }

    QByteArray packet = createPacket(MsgType::FRAME, jpegData);
    qint64 written = m_socket->write(packet);
    m_socket->flush();

    if (written > 0) {
        emit frameSent(static_cast<int>(written));
    }

    // Also send active app status
    sendAppStatus();
}

void StudentAgent::sendAppStatus()
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) return;

    QString appName;
    QString appClass;

#ifdef Q_OS_WIN
    // Windows: use Win32 API
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        // Get window title
        wchar_t title[256];
        GetWindowTextW(hwnd, title, 256);
        appName = QString::fromWCharArray(title);

        // Get process name as "class"
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                wchar_t exePath[MAX_PATH];
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
                    QString path = QString::fromWCharArray(exePath);
                    appClass = QFileInfo(path).baseName(); // e.g. "firefox", "chrome"
                }
                CloseHandle(hProc);
            }
        }
    }
#else
    // Linux: use hyprctl (Hyprland) or fallback
    QProcess proc;
    proc.start("hyprctl", QStringList() << "activewindow" << "-j");
    proc.waitForFinished(500);

    if (proc.exitCode() == 0) {
        QByteArray output = proc.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(output);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            appName = obj.value("title").toString();
            appClass = obj.value("class").toString();
        }
    }

    if (appClass.isEmpty()) {
        // Fallback: try xdotool
        QProcess proc2;
        proc2.start("sh", QStringList() << "-c"
            << "xdotool getactivewindow getwindowname 2>/dev/null");
        proc2.waitForFinished(500);
        if (proc2.exitCode() == 0) {
            appName = QString::fromUtf8(proc2.readAllStandardOutput()).trimmed();
        }
    }
#endif

    if (appName.isEmpty() && appClass.isEmpty()) return;

    // Truncate long titles
    if (appName.length() > 80) {
        appName = appName.left(77) + "...";
    }

    QJsonObject obj;
    obj["app"]      = appName;
    obj["class"]    = appClass;

    QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray packet = createPacket(MsgType::APP_STATUS, payload);
    m_socket->write(packet);
    m_socket->flush();
}

void StudentAgent::sendPing()
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    m_socket->write(createPacket(MsgType::PING));
    m_socket->flush();
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

void StudentAgent::sendChat(const QString& message)
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray payload = createChatPayload(getLocalHostname(), message);
    QByteArray packet = createPacket(MsgType::CHAT_MSG, payload);
    m_socket->write(packet);
    m_socket->flush();
    qInfo() << "[StudentAgent] Sent chat message";
}

void StudentAgent::sendHelpRequest(const QString& message)
{
    if (!m_connected || m_socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray payload = createHelpPayload(getLocalHostname(), message);
    QByteArray packet = createPacket(MsgType::HELP_REQUEST, payload);
    m_socket->write(packet);
    m_socket->flush();
    qInfo() << "[StudentAgent] Sent help request";
}

} // namespace LabMonitor
