#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QHostInfo>
#include <cstdint>

namespace LabMonitor {

// ── Constants ──────────────────────────────────────────────
constexpr uint16_t DEFAULT_PORT       = 5400;
constexpr uint16_t MAGIC_BYTES        = 0xABCD;
constexpr int      HEADER_SIZE        = 12;
constexpr int      PING_INTERVAL_MS   = 10000;   // 10 seconds
constexpr int      TIMEOUT_MS         = 15000;   // 15 seconds
constexpr int      DEFAULT_CAPTURE_MS = 2000;     // 2 seconds
constexpr int      DEFAULT_QUALITY    = 60;       // JPEG quality
constexpr double   DEFAULT_SCALE      = 0.5;      // 50% scale
constexpr int      MAX_CONNECTIONS    = 40;
constexpr int      MAX_PAYLOAD_SIZE   = 10 * 1024 * 1024; // 10 MB sanity limit

// ── Message Types ──────────────────────────────────────────
enum class MsgType : uint8_t {
    HELLO        = 0x01,   // student → teacher: registration
    FRAME        = 0x02,   // student → teacher: screenshot JPEG
    ACK          = 0x03,   // teacher → student: acknowledgment
    PING         = 0x04,   // bidirectional: keepalive
    PONG         = 0x05,   // bidirectional: keepalive response
    CMD          = 0x10,   // teacher → student: command (future)
    MESSAGE      = 0x11,   // teacher → student: text message popup
    LOCK_SCREEN  = 0x12,   // teacher → student: lock student screen
    UNLOCK_SCREEN= 0x13,   // teacher → student: unlock student screen
    SEND_URL     = 0x14,   // teacher → student: open URL in browser
    CHAT_MSG     = 0x15,   // bidirectional: chat message
    HELP_REQUEST = 0x16,   // student → teacher: request help
    HELP_CLEAR   = 0x17,   // teacher → student: clear help request
    APP_STATUS   = 0x18,   // student → teacher: active app info
};

// ── Packet Header (12 bytes) ───────────────────────────────
//  [0-1]  uint16  magic (0xABCD)
//  [2-3]  uint16  msg_type (only low byte used, high byte reserved)
//  [4-7]  uint32  payload_length
//  [8-11] uint32  reserved (0)
struct PacketHeader {
    uint16_t magic          = MAGIC_BYTES;
    uint16_t msgType        = 0;
    uint32_t payloadLength  = 0;
    uint32_t reserved       = 0;
};

// ── Student Info ───────────────────────────────────────────
struct StudentInfo {
    QString id;           // unique id (hostname + ip)
    QString hostname;
    QString username;
    QString os;
    QString screenRes;    // e.g. "1920x1080"
    QString ipAddress;
    qint64  connectTime = 0;
    qint64  lastFrameTime = 0;
    bool    online = false;
    QString activeApp;        // currently active application name
    QString activeAppClass;   // application class (e.g. "firefox")
};

// ── Serialization Helpers ──────────────────────────────────

// Serialize a packet header into 12 bytes
QByteArray serializeHeader(MsgType type, uint32_t payloadLength);

// Parse a packet header from 12 bytes. Returns true on success.
bool parseHeader(const QByteArray& data, PacketHeader& header);

// Create a HELLO payload (JSON)
QByteArray createHelloPayload(const QString& hostname,
                              const QString& username,
                              const QString& os,
                              const QString& screenRes);

// Parse a HELLO payload (JSON). Returns true on success.
bool parseHelloPayload(const QByteArray& data, StudentInfo& info);

// Create a simple packet (header + payload)
QByteArray createPacket(MsgType type, const QByteArray& payload = {});

// Get the local hostname
QString getLocalHostname();

// Get the current username
QString getLocalUsername();

// Get the OS description
QString getOsString();

// Get screen resolution string
QString getScreenResolution();

// Create a MESSAGE payload (JSON with title + body)
QByteArray createMessagePayload(const QString& title, const QString& body,
                                const QString& sender);

// Parse a MESSAGE payload. Returns true on success.
struct MessageData {
    QString title;
    QString body;
    QString sender;
    qint64  timestamp = 0;
};
bool parseMessagePayload(const QByteArray& data, MessageData& msg);

// Chat message data
struct ChatData {
    QString sender;
    QString message;
    qint64  timestamp = 0;
};
QByteArray createChatPayload(const QString& sender, const QString& message);
bool parseChatPayload(const QByteArray& data, ChatData& chat);

// URL payload
QByteArray createUrlPayload(const QString& url);
QString parseUrlPayload(const QByteArray& data);

// Help request payload
QByteArray createHelpPayload(const QString& studentName, const QString& message);
struct HelpData {
    QString studentName;
    QString message;
    qint64  timestamp = 0;
};
bool parseHelpPayload(const QByteArray& data, HelpData& help);

} // namespace LabMonitor
