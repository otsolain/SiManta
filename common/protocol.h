#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QHostInfo>
#include <cstdint>

namespace LabMonitor {
constexpr uint16_t DEFAULT_PORT       = 5400;
constexpr uint16_t DISCOVERY_PORT     = 5401;     // UDP broadcast port for auto-discovery
constexpr int      DISCOVERY_INTERVAL = 1000;     // broadcast every 1 second (fast discovery)
constexpr uint32_t DISCOVERY_MAGIC    = 0x53494D41; // "SIMA" - SiManta discovery identifier
constexpr uint16_t MAGIC_BYTES        = 0xABCD;
constexpr int      HEADER_SIZE        = 12;
constexpr int      PING_INTERVAL_MS   = 3000;    // 3 seconds (frequent keepalive like Veyon)
constexpr int      TIMEOUT_MS         = 120000;  // 120 seconds (2 min - resilient to lag spikes)
constexpr int      DEFAULT_CAPTURE_MS = 1000;     // 1 second (Veyon-like refresh rate)
constexpr int      DEFAULT_QUALITY    = 92;       // JPEG quality (near-lossless)
constexpr double   DEFAULT_SCALE      = 1.0;      // Full resolution capture
constexpr int      MAX_CONNECTIONS    = 100;      // Support large classrooms
constexpr int      MAX_PAYLOAD_SIZE   = 50 * 1024 * 1024; // 50 MB for full-res captures
enum class MsgType : uint8_t {
    HELLO        = 0x01,   // student → teacher: registration
    FRAME        = 0x02,   // student → teacher: screenshot JPEG (full frame)
    ACK          = 0x03,   // teacher → student: acknowledgment
    PING         = 0x04,   // bidirectional: keepalive
    PONG         = 0x05,   // bidirectional: keepalive response
    FRAME_DELTA  = 0x06,   // student → teacher: delta frame (changed tiles only)
    QUALITY_HIGH = 0x07,   // teacher → student: switch to high-res mode (fullscreen view)
    QUALITY_NORMAL = 0x08, // teacher → student: switch to normal mode (thumbnail view)
    SET_UPDATE_SPEED = 0x09, // teacher → student: set screenshot capture interval
    CMD          = 0x10,   // teacher → student: command (future)
    MESSAGE      = 0x11,   // teacher → student: text message popup
    LOCK_SCREEN  = 0x12,   // teacher → student: lock student screen
    UNLOCK_SCREEN= 0x13,   // teacher → student: unlock student screen
    SEND_URL     = 0x14,   // teacher → student: open URL in browser
    CHAT_MSG     = 0x15,   // bidirectional: chat message
    HELP_REQUEST = 0x16,   // student → teacher: request help
    HELP_CLEAR   = 0x17,   // teacher → student: clear help request
    APP_STATUS   = 0x18,   // student → teacher: active app info + CPU/RAM
    TRANSFER_START = 0x20, // teacher → student: file transfer begin
    TRANSFER_CHUNK = 0x21, // teacher → student: file data chunk
    TRANSFER_END   = 0x22, // teacher → student: file transfer complete
    DIR_LIST_REQUEST  = 0x30, // teacher → student: list directory contents
    DIR_LIST_RESPONSE = 0x31, // student → teacher: directory listing
    MKDIR_REQUEST     = 0x32, // teacher → student: create folder
    FILE_RETRIEVE_REQUEST = 0x33, // teacher → student: request file download
    FILE_RETRIEVE_START   = 0x34, // student → teacher: file retrieval begin
    FILE_RETRIEVE_CHUNK   = 0x35, // student → teacher: file retrieval data
    FILE_RETRIEVE_END     = 0x36, // student → teacher: file retrieval complete
    DEMO_START   = 0x40,   // teacher → student: start demo mode (teacher screen broadcast)
    DEMO_FRAME   = 0x41,   // teacher → student: demo screen frame
    DEMO_STOP    = 0x42,   // teacher → student: stop demo mode
    WAKE_ON_LAN  = 0x50,   // teacher internal: wake student PC via magic packet
    KICK         = 0x60,   // teacher → student: force disconnect (stop reconnecting)
};
struct PacketHeader {
    uint16_t magic          = MAGIC_BYTES;
    uint16_t msgType        = 0;
    uint32_t payloadLength  = 0;
    uint32_t reserved       = 0;
};
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
    bool    isFolder = false;   // true = was zipped from a folder
    QString destPath;           // destination folder on student (empty = default Downloads/SiManta)
};
QByteArray createFileStartPayload(const QString& fileName, qint64 fileSize, bool isFolder, const QString& destPath = {});
bool parseFileStartPayload(const QByteArray& data, FileStartData& fsd);

QByteArray createFileEndPayload(const QString& fileName);
QString parseFileEndPayload(const QByteArray& data);

// ── Directory listing ──
QByteArray createDirListRequest(const QString& path);
QString parseDirListRequest(const QByteArray& data);
QByteArray createDirListResponse(const QString& path, const QJsonArray& entries);

// ── Mkdir ──
QByteArray createMkdirRequest(const QString& path);
QString parseMkdirRequest(const QByteArray& data);

// ── File retrieve ──
QByteArray createFileRetrieveRequest(const QString& filePath);
QString parseFileRetrieveRequest(const QByteArray& data);
QByteArray createFileRetrieveStart(const QString& fileName, qint64 fileSize);

} // namespace LabMonitor

