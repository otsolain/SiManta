#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QHostInfo>
#include <cstdint>

namespace LabMonitor {
constexpr uint16_t DEFAULT_PORT       = 5400;
constexpr uint16_t MAGIC_BYTES        = 0xABCD;
constexpr int      HEADER_SIZE        = 12;
constexpr int      PING_INTERVAL_MS   = 10000;
constexpr int      TIMEOUT_MS         = 15000;
constexpr int      DEFAULT_CAPTURE_MS = 2000;
constexpr int      DEFAULT_QUALITY    = 92;
constexpr double   DEFAULT_SCALE      = 1.0;
constexpr int      MAX_CONNECTIONS    = 40;
constexpr int      MAX_PAYLOAD_SIZE   = 20 * 1024 * 1024;
enum class MsgType : uint8_t {
    HELLO        = 0x01,
    FRAME        = 0x02,
    ACK          = 0x03,
    PING         = 0x04,
    PONG         = 0x05,
    CMD          = 0x10,
    MESSAGE      = 0x11,
    LOCK_SCREEN  = 0x12,
    UNLOCK_SCREEN= 0x13,
    SEND_URL     = 0x14,
    CHAT_MSG     = 0x15,
    HELP_REQUEST = 0x16,
    HELP_CLEAR   = 0x17,
    APP_STATUS   = 0x18,
    TRANSFER_START = 0x20,
    TRANSFER_CHUNK = 0x21,
    TRANSFER_END   = 0x22,
};
struct PacketHeader {
    uint16_t magic          = MAGIC_BYTES;
    uint16_t msgType        = 0;
    uint32_t payloadLength  = 0;
    uint32_t reserved       = 0;
};
struct StudentInfo {
    QString id;
    QString hostname;
    QString username;
    QString os;
    QString screenRes;
    QString ipAddress;
    qint64  connectTime = 0;
    qint64  lastFrameTime = 0;
    bool    online = false;
    QString activeApp;
    QString activeAppClass;
};
QByteArray serializeHeader(MsgType type, uint32_t payloadLength);
bool parseHeader(const QByteArray& data, PacketHeader& header);
QByteArray createHelloPayload(const QString& hostname,
                              const QString& username,
                              const QString& os,
                              const QString& screenRes);
bool parseHelloPayload(const QByteArray& data, StudentInfo& info);
QByteArray createPacket(MsgType type, const QByteArray& payload = {});
QString getLocalHostname();
QString getLocalUsername();
QString getOsString();
QString getScreenResolution();
QByteArray createMessagePayload(const QString& title, const QString& body,
                                const QString& sender);
struct MessageData {
    QString title;
    QString body;
    QString sender;
    qint64  timestamp = 0;
};
bool parseMessagePayload(const QByteArray& data, MessageData& msg);
struct ChatData {
    QString sender;
    QString message;
    qint64  timestamp = 0;
};
QByteArray createChatPayload(const QString& sender, const QString& message);
bool parseChatPayload(const QByteArray& data, ChatData& chat);
QByteArray createUrlPayload(const QString& url);
QString parseUrlPayload(const QByteArray& data);
QByteArray createHelpPayload(const QString& studentName, const QString& message);
struct HelpData {
    QString studentName;
    QString message;
    qint64  timestamp = 0;
};
bool parseHelpPayload(const QByteArray& data, HelpData& help);
struct FileStartData {
    QString fileName;
    qint64  fileSize = 0;
    bool    isFolder = false;
};
QByteArray createFileStartPayload(const QString& fileName, qint64 fileSize, bool isFolder);
bool parseFileStartPayload(const QByteArray& data, FileStartData& fsd);

QByteArray createFileEndPayload(const QString& fileName);
QString parseFileEndPayload(const QByteArray& data);

}
