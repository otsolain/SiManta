#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include "protocol.h"
#include "screen_capturer.h"

namespace LabMonitor {

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
    void fileReceiveStarted(const QString& fileName, qint64 fileSize, bool isFolder);
    void fileReceiveProgress(const QString& fileName, int percent);
    void fileReceiveCompleted(const QString& fileName, const QString& savePath, bool isFolder);

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
    void handleFileStart(const QByteArray& payload);
    void handleFileChunk(const QByteArray& payload);
    void handleFileEnd(const QByteArray& payload);
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
    int     m_reconnectDelay = 1000;
    int     m_maxReconnectDelay = 30000;
    QByteArray m_readBuffer;
    QFile*   m_incomingFile = nullptr;
    QString  m_incomingFileName;
    qint64   m_incomingFileSize = 0;
    qint64   m_incomingReceived = 0;
    bool     m_incomingIsFolder = false;
    quint64  m_lastCpuIdle = 0;
    quint64  m_lastCpuKernel = 0;
    quint64  m_lastCpuUser = 0;
    bool     m_cpuInitialized = false;
};

}
