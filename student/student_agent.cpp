#include "student_agent.h"

#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QBuffer>
#include <QImage>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>

#include <windows.h>
#include <psapi.h>
#include <shellapi.h>

namespace LabMonitor {
static constexpr qint64 MAX_BUFFER_SIZE = 20 * 1024 * 1024;

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
    if (m_running && !m_reconnectTimer->isActive()) {
        qInfo() << "[StudentAgent] Reconnecting in" << m_reconnectDelay << "ms";
        m_reconnectTimer->start(m_reconnectDelay);
        m_reconnectDelay = qMin(m_reconnectDelay * 2, m_maxReconnectDelay);
    }
}

void StudentAgent::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    if (m_connected) {
        qWarning() << "[StudentAgent] Socket error:" << m_socket->errorString();
    }
    if (!m_connected && m_running && !m_reconnectTimer->isActive()) {
        qInfo() << "[StudentAgent] Connection failed, retrying in" << m_reconnectDelay << "ms";
        m_reconnectTimer->start(m_reconnectDelay);
        m_reconnectDelay = qMin(m_reconnectDelay * 2, m_maxReconnectDelay);
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
            break;
        case MsgType::CMD:
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
        case MsgType::TRANSFER_START:
            handleFileStart(payload);
            break;
        case MsgType::TRANSFER_CHUNK:
            handleFileChunk(payload);
            break;
        case MsgType::TRANSFER_END:
            handleFileEnd(payload);
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
    sendAppStatus();
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
                    appClass = QFileInfo(exeFullPath).baseName();
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
                    bi.biHeight = -h;
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

void StudentAgent::handleFileStart(const QByteArray& payload)
{
    FileStartData fsd;
    if (!parseFileStartPayload(payload, fsd)) {
        qWarning() << "[StudentAgent] Failed to parse FILE_START";
        return;
    }
    QString downloadsDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString siMantaDir = downloadsDir + "/SiManta";
    QDir().mkpath(siMantaDir);

    QString filePath = siMantaDir + "/" + fsd.fileName;
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
        extractDir.chop(4);
        QDir().mkpath(extractDir);
        QProcess* proc = new QProcess(this);
        QString fName = m_incomingFileName;
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, filePath, extractDir, fName](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                QFile::remove(filePath);
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
    return static_cast<double>(memInfo.dwMemoryLoad);
}

}
