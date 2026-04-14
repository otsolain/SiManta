#include "protocol.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QHostInfo>
#include <QDateTime>
#include <QGuiApplication>
#include <QScreen>

namespace LabMonitor {

QByteArray serializeHeader(MsgType type, uint32_t payloadLength)
{
    QByteArray data;
    data.resize(HEADER_SIZE);

    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << static_cast<uint16_t>(MAGIC_BYTES);
    stream << static_cast<uint16_t>(static_cast<uint8_t>(type));
    stream << payloadLength;
    stream << static_cast<uint32_t>(0);

    return data;
}

bool parseHeader(const QByteArray& data, PacketHeader& header)
{
    if (data.size() < HEADER_SIZE) {
        return false;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> header.magic;
    stream >> header.msgType;
    stream >> header.payloadLength;
    stream >> header.reserved;

    if (header.magic != MAGIC_BYTES) {
        return false;
    }

    if (header.payloadLength > MAX_PAYLOAD_SIZE) {
        return false;
    }

    return true;
}

QByteArray createHelloPayload(const QString& hostname,
                              const QString& username,
                              const QString& os,
                              const QString& screenRes)
{
    QJsonObject obj;
    obj["hostname"]   = hostname;
    obj["username"]   = username;
    obj["os"]         = os;
    obj["screen"]     = screenRes;
    obj["timestamp"]  = QDateTime::currentMSecsSinceEpoch();

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool parseHelloPayload(const QByteArray& data, StudentInfo& info)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    info.hostname  = obj.value("hostname").toString("unknown");
    info.username  = obj.value("username").toString("unknown");
    info.os        = obj.value("os").toString("unknown");
    info.screenRes = obj.value("screen").toString("unknown");
    info.connectTime = QDateTime::currentMSecsSinceEpoch();
    info.online = true;

    return true;
}

QByteArray createPacket(MsgType type, const QByteArray& payload)
{
    QByteArray packet;
    packet.append(serializeHeader(type, static_cast<uint32_t>(payload.size())));
    if (!payload.isEmpty()) {
        packet.append(payload);
    }
    return packet;
}

QString getLocalHostname()
{
    return QHostInfo::localHostName();
}

QString getLocalUsername()
{
    QString user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty()) {
        user = qEnvironmentVariable("USER");
    }
    if (user.isEmpty()) {
        user = "unknown";
    }
    return user;
}

QString getOsString()
{
    return QSysInfo::prettyProductName();
}

QString getScreenResolution()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QSize size = screen->size();
        return QStringLiteral("%1x%2").arg(size.width()).arg(size.height());
    }
    return QStringLiteral("1920x1080");
}

QByteArray createMessagePayload(const QString& title, const QString& body,
                                const QString& sender)
{
    QJsonObject obj;
    obj["title"]     = title;
    obj["body"]      = body;
    obj["sender"]    = sender;
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool parseMessagePayload(const QByteArray& data, MessageData& msg)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    msg.title     = obj.value("title").toString("Message");
    msg.body      = obj.value("body").toString();
    msg.sender    = obj.value("sender").toString("Teacher");
    msg.timestamp = obj.value("timestamp").toVariant().toLongLong();

    return true;
}

QByteArray createChatPayload(const QString& sender, const QString& message)
{
    QJsonObject obj;
    obj["sender"]    = sender;
    obj["message"]   = message;
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool parseChatPayload(const QByteArray& data, ChatData& chat)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
    QJsonObject obj = doc.object();
    chat.sender    = obj.value("sender").toString("Unknown");
    chat.message   = obj.value("message").toString();
    chat.timestamp = obj.value("timestamp").toVariant().toLongLong();
    return true;
}

QByteArray createUrlPayload(const QString& url)
{
    QJsonObject obj;
    obj["url"]       = url;
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString parseUrlPayload(const QByteArray& data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object().value("url").toString();
}

QByteArray createHelpPayload(const QString& studentName, const QString& message)
{
    QJsonObject obj;
    obj["student"]   = studentName;
    obj["message"]   = message;
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool parseHelpPayload(const QByteArray& data, HelpData& help)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
    QJsonObject obj = doc.object();
    help.studentName = obj.value("student").toString("Unknown");
    help.message     = obj.value("message").toString();
    help.timestamp   = obj.value("timestamp").toVariant().toLongLong();
    return true;
}

QByteArray createFileStartPayload(const QString& fileName, qint64 fileSize, bool isFolder)
{
    QJsonObject obj;
    obj["fileName"]  = fileName;
    obj["fileSize"]  = fileSize;
    obj["isFolder"]  = isFolder;
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool parseFileStartPayload(const QByteArray& data, FileStartData& fsd)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
    QJsonObject obj = doc.object();
    fsd.fileName = obj.value("fileName").toString();
    fsd.fileSize = obj.value("fileSize").toVariant().toLongLong();
    fsd.isFolder = obj.value("isFolder").toBool(false);
    return !fsd.fileName.isEmpty();
}

QByteArray createFileEndPayload(const QString& fileName)
{
    QJsonObject obj;
    obj["fileName"]  = fileName;
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString parseFileEndPayload(const QByteArray& data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object().value("fileName").toString();
}

}
