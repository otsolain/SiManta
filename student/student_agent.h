#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QFile>
#include <QElapsedTimer>
#include "protocol.h"
#include "screen_capturer.h"

namespace LabMonitor {

/**
 * StudentAgent — background agent that captures screen and streams to teacher
 * 
 * Handles:
 * - TCP connection to teacher console
 * - HELLO handshake on connect
 * - Periodic screen capture and FRAME streaming
 * - PING/PONG keepalive with health monitoring
 * - Auto-reconnect with fast recovery (Veyon-like)
 * - UDP auto-discovery of teacher beacon
 * - Adaptive quality based on network conditions
 * - App status reporting (foreground window + CPU/RAM)
 */
class StudentAgent : public QObject
{
    Q_OBJECT

public:
    explicit StudentAgent(QObject* parent = nullptr);
    ~StudentAgent() override;
    void setTeacherHost(const QString& host) { m_teacherHost = host; }
    void setTeacherPort(uint16_t port) { m_teacherPort = port; }
    void setCaptureInterval(int ms) { m_captureInterval = ms; }
    void setCaptureQuality(int quality);
    void setCaptureScale(double scale);
    void start();
    void stop();
    void startDiscovery();
    void stopDiscovery();
    void setAutoDiscovery(bool enabled) { m_autoDiscovery = enabled; }
    void sendChat(const QString& message);
    void sendHelpRequest(const QString& message);
    void sendAppStatus();

signals:
    void connected();
    void disconnected();
    void error(const QString& message);
    void frameSent(int bytes);
    void messageReceived(const QString& title, const QString& body, const QString& sender);
    void lockScreenRequested();
    void unlockScreenRequested();
    void urlReceived(const QString& url);
    void chatReceived(const QString& sender, const QString& message);
    void cmdReceived(const QString& cmd);
    void fileReceiveStarted(const QString& fileName, qint64 fileSize, bool isFolder);
    void fileReceiveProgress(const QString& fileName, int percent);
    void fileReceiveCompleted(const QString& fileName, const QString& savePath, bool isFolder);
    void teacherDiscovered(const QString& ip, uint16_t port, const QString& hostname);
    void kicked(); // emitted when teacher forcefully disconnects this student

private slots:
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void captureAndSend();
    void sendPing();
    void attemptReconnect();
    void onDiscoveryReadyRead();

private:
    void connectToTeacher();
    void sendHello();
    void processIncomingData();
    void resetReconnectBackoff();
    void handleFileStart(const QByteArray& payload);
    void handleFileChunk(const QByteArray& payload);
    void handleFileEnd(const QByteArray& payload);
    void handleDirListRequest(const QByteArray& payload);
    void handleMkdirRequest(const QByteArray& payload);
    void handleFileRetrieveRequest(const QByteArray& payload);
    void sendNextRetrieveChunk();
    QString parseCmdPayload(const QByteArray& payload);
    double getCpuUsage();
    double getRamUsage();

    QTcpSocket* m_socket = nullptr;
    QString     m_teacherHost = "127.0.0.1";
    uint16_t    m_teacherPort = DEFAULT_PORT;
    bool        m_running = false;
    bool        m_connected = false;

    ScreenCapturer* m_capturer = nullptr;
    QTimer*         m_captureTimer = nullptr;
    int             m_captureInterval = DEFAULT_CAPTURE_MS;

    QTimer* m_pingTimer = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    int     m_reconnectDelay = 1000;    // starts at 1s
    int     m_maxReconnectDelay = 5000; // max 5s (fast recovery like Veyon)

    QByteArray m_readBuffer;

    // File receive state
    QFile*   m_incomingFile = nullptr;
    QString  m_incomingFileName;
    qint64   m_incomingFileSize = 0;
    qint64   m_incomingReceived = 0;
    bool     m_incomingIsFolder = false;

    // CPU tracking (Windows)
    quint64  m_lastCpuIdle = 0;
    quint64  m_lastCpuKernel = 0;
    quint64  m_lastCpuUser = 0;
    bool     m_cpuInitialized = false;

    // Async file retrieve state
    QFile*   m_retrieveFile = nullptr;
    QString  m_retrieveFileName;
    qint64   m_retrieveFileSize = 0;

    // UDP auto-discovery
    QUdpSocket* m_discoverySocket = nullptr;
    bool        m_autoDiscovery = false;

    // ── Veyon-like adaptive quality & health monitoring ──
    int      m_baseQuality = DEFAULT_QUALITY;       // Original configured quality
    int      m_adaptiveQuality = DEFAULT_QUALITY;   // Current quality (adjusted dynamically)
    int      m_consecutiveSkips = 0;                // Count of skipped frames due to backpressure
    int      m_appStatusCounter = 0;                // Only send app status every N frames
    QElapsedTimer m_lastPongTimer;                  // Track PONG response time
    bool     m_waitingForPong = false;              // Whether we're waiting for a PONG
    int      m_missedPongs = 0;                     // Consecutive missed PONGs
};

} 
