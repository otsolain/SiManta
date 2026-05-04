#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QDataStream>
#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtGui/QImage>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QString srcPng = "installer/logo_cropped.png";
    QString dstIco = "installer/logo.ico";
    QString dstIco2 = "teacher/resources/icons/logo.ico";

    QImage src(srcPng);
    if (src.isNull()) {
        qCritical() << "Failed to load" << srcPng;
        return 1;
    }

    QList<int> sizes = {16, 32, 48, 256};

    struct IcoEntry { int size; QByteArray pngData; };
    QList<IcoEntry> entries;

    for (int s : sizes) {
        QImage scaled = src.scaled(s, s, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_ARGB32);
        QByteArray data;
        QBuffer buf(&data);
        buf.open(QIODevice::WriteOnly);
        scaled.save(&buf, "PNG");
        entries.append({s, data});
    }

    auto writeIco = [&](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qCritical() << "Cannot write" << path;
            return;
        }
        QDataStream ds(&file);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds << quint16(0) << quint16(1) << quint16(entries.size());

        int dataOffset = 6 + entries.size() * 16;
        for (const auto& e : entries) {
            ds << quint8(e.size < 256 ? e.size : 0);
            ds << quint8(e.size < 256 ? e.size : 0);
            ds << quint8(0) << quint8(0);
            ds << quint16(1) << quint16(32);
            ds << quint32(e.pngData.size());
            ds << quint32(dataOffset);
            dataOffset += e.pngData.size();
        }
        for (const auto& e : entries) file.write(e.pngData);
        file.close();
        qInfo() << "Written" << path;
    };

    writeIco(dstIco);
    writeIco(dstIco2);
    qInfo() << "Done!";
    return 0;
}
