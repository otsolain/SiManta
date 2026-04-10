#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>
#include <QPixmap>

#include "protocol.h"

namespace LabMonitor {

/**
 * ClientState — tracks the state and data buffer of each connected student.
 */
struct ClientState {
    QTcpSocket*  socket = nullptr;
    StudentInfo  info;
    QByteArray   readBuffer;
    bool         helloReceived = false;
    QTimer*      timeoutTimer = nullptr;
    QPixmap      lastFrame;
};

/**
 * ConnectionManager — TCP server that manages student connections.
 * 
 * Handles:
 * - Listening on configured port
 * - Accepting new student connections
 * - Parsing binary protocol (HELLO, FRAME, PING)
 * - Per-student state management
 * - Timeout detection (disconnects students who stop responding)
 * - Signals for new connections, disconnections, and frame updates
 */
class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(QObject* parent = nullptr);
    ~ConnectionManager() override;

    // Start/stop listening
    bool startListening(uint16_t port = DEFAULT_PORT);
    void stopListening();
    bool isListening() const;

    // Get connected count
    int connectedCount() const { return m_clients.size(); }

    // Get student info list
    QList<StudentInfo> connectedStudents() const;

    // Send a message to specific students
    void sendMessage(const QStringList& studentIds, const QString& title,
                     const QString& body, const QString& sender);

    // Send a message to all connected students
    void sendMessageToAll(const QString& title, const QString& body,
                          const QString& sender);

    // Lock/unlock screens
    void sendLockScreen(const QStringList& studentIds);
    void sendUnlockScreen(const QStringList& studentIds);
    void sendLockAll();
    void sendUnlockAll();

    // Send URL to students
    void sendUrl(const QStringList& studentIds, const QString& url);
    void sendUrlToAll(const QString& url);

    // Send chat message to a student
    void sendChatTo(const QString& studentId, const QString& sender, const QString& message);

signals:
    void studentConnected(const StudentInfo& info);
    void studentDisconnected(const QString& studentId);
    void frameReceived(const QString& studentId, const QPixmap& frame);
    void listeningStarted(uint16_t port);
    void listenError(const QString& error);
    void chatReceived(const QString& studentId, const QString& sender, const QString& message);
    void helpRequestReceived(const QString& studentId, const QString& studentName, const QString& message);
    void appStatusReceived(const QString& studentId, const QString& appName, const QString& appClass);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onClientTimeout();

private:
    void processClientData(QTcpSocket* socket);
    void handleHello(QTcpSocket* socket, const QByteArray& payload);
    void handleFrame(QTcpSocket* socket, const QByteArray& payload);
    void removeClient(QTcpSocket* socket);
    QString clientId(QTcpSocket* socket) const;

    QTcpServer* m_server = nullptr;
    QMap<QTcpSocket*, ClientState> m_clients;
};

} // namespace LabMonitor
