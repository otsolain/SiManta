#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
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
 * - PING/PONG keepalive
 * - Auto-reconnect with exponential backoff
 */
class StudentAgent : public QObject
{
    Q_OBJECT

public:
    explicit StudentAgent(QObject* parent = nullptr);
    ~StudentAgent() override;

    // Configuration
    void setTeacherHost(const QString& host) { m_teacherHost = host; }
    void setTeacherPort(uint16_t port) { m_teacherPort = port; }
    void setCaptureInterval(int ms) { m_captureInterval = ms; }
    void setCaptureQuality(int quality);
    void setCaptureScale(double scale);

    // Start the agent
    void start();

    // Stop the agent
    void stop();

    // Send a chat message to teacher
    void sendChat(const QString& message);

    // Send a help request to teacher
    void sendHelpRequest(const QString& message);

    // Send current active app status
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

private slots:
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void captureAndSend();
    void sendPing();
    void attemptReconnect();

private:
    void connectToTeacher();
    void sendHello();
    void processIncomingData();
    void resetReconnectBackoff();

    // Connection
    QTcpSocket* m_socket = nullptr;
    QString     m_teacherHost = "127.0.0.1";
    uint16_t    m_teacherPort = DEFAULT_PORT;
    bool        m_running = false;
    bool        m_connected = false;

    // Capture
    ScreenCapturer* m_capturer = nullptr;
    QTimer*         m_captureTimer = nullptr;
    int             m_captureInterval = DEFAULT_CAPTURE_MS;

    // Keepalive
    QTimer* m_pingTimer = nullptr;

    // Reconnect
    QTimer* m_reconnectTimer = nullptr;
    int     m_reconnectDelay = 1000;    // starts at 1s
    int     m_maxReconnectDelay = 30000; // max 30s

    // Buffer for incoming data
    QByteArray m_readBuffer;
};

} // namespace LabMonitor
