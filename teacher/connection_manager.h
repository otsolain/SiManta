#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>
#include <QPixmap>
#include <QBuffer>
#include <QFile>

#include "protocol.h"

namespace LabMonitor {

struct ClientState {
    QTcpSocket*  socket = nullptr;
    StudentInfo  info;
    QByteArray   readBuffer;
    bool         helloReceived = false;
    QTimer*      timeoutTimer = nullptr;
    QPixmap      lastFrame;
};

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(QObject* parent = nullptr);
    ~ConnectionManager() override;
    bool startListening(uint16_t port = DEFAULT_PORT);
    void stopListening();
    bool isListening() const;
    int connectedCount() const { return m_clients.size(); }
    QList<StudentInfo> connectedStudents() const;
    void sendMessage(const QStringList& studentIds, const QString& title,
                     const QString& body, const QString& sender);
    void sendMessageToAll(const QString& title, const QString& body,
                          const QString& sender);
    void sendLockScreen(const QStringList& studentIds);
    void sendUnlockScreen(const QStringList& studentIds);
    void sendLockAll();
    void sendUnlockAll();
    void sendUrl(const QStringList& studentIds, const QString& url);
    void sendUrlToAll(const QString& url);
    void sendChatTo(const QString& studentId, const QString& sender, const QString& message);
    void sendFile(const QStringList& studentIds, const QString& filePath, bool isFolder);
    void sendFileToAll(const QString& filePath, bool isFolder);

signals:
    void studentConnected(const StudentInfo& info);
    void studentDisconnected(const QString& studentId);
    void frameReceived(const QString& studentId, const QPixmap& frame);
    void listeningStarted(uint16_t port);
    void listenError(const QString& error);
    void chatReceived(const QString& studentId, const QString& sender, const QString& message);
    void helpRequestReceived(const QString& studentId, const QString& studentName, const QString& message);
    void appStatusReceived(const QString& studentId, const QString& appName,
                           const QString& appClass, const QPixmap& appIcon,
                           double cpuUsage, double ramUsage);
    void fileTransferProgress(const QString& fileName, int percentDone);
    void fileTransferComplete(const QString& fileName);

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

}
