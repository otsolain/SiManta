#include "screen_capturer.h"

#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QTemporaryFile>
#include <QBuffer>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LabMonitor {

ScreenCapturer::ScreenCapturer(QObject* parent)
    : QObject(parent)
{
}

ScreenCapturer::~ScreenCapturer() = default;

void ScreenCapturer::setQuality(int quality)
{
    m_quality = qBound(1, quality, 100);
}

void ScreenCapturer::setScale(double scale)
{
    m_scale = qBound(0.1, scale, 1.0);
}

QByteArray ScreenCapturer::capture()
{
#ifdef Q_OS_WIN
    return captureWindows();
#else
    return captureLinux();
#endif
}

bool ScreenCapturer::isGrimAvailable()
{
#ifdef Q_OS_WIN
    // On Windows, we use Qt's built-in screen capture — always available
    return true;
#else
    QProcess proc;
    proc.start("which", QStringList() << "grim");
    proc.waitForFinished(2000);
    return proc.exitCode() == 0;
#endif
}

// ── Linux capture using grim ─────────────────────────────────

QByteArray ScreenCapturer::captureLinux()
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);

    QStringList args;
    args << "-c"  // capture cursor
         << "-t" << "jpeg"
         << "-q" << QString::number(m_quality)
         << "-s" << QString::number(m_scale, 'f', 2);

    // First try stdout output
    QStringList stdoutArgs = args;
    stdoutArgs << "-";

    proc.start("grim", stdoutArgs);

    if (!proc.waitForStarted(3000)) {
        emit captureError("Failed to start grim: " + proc.errorString());
        return {};
    }

    if (!proc.waitForFinished(5000)) {
        proc.kill();
        emit captureError("grim timed out");
        return {};
    }

    if (proc.exitCode() == 0) {
        QByteArray data = proc.readAllStandardOutput();
        if (!data.isEmpty()) {
            return data;
        }
    }

    // Fallback: use temp file if stdout doesn't work
    QTemporaryFile tmpFile;
    tmpFile.setFileTemplate("/tmp/labmonitor_XXXXXX.jpg");
    if (!tmpFile.open()) {
        emit captureError("Failed to create temp file");
        return {};
    }

    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    QStringList fileArgs = args;
    fileArgs << tmpPath;

    QProcess proc2;
    proc2.start("grim", fileArgs);

    if (!proc2.waitForStarted(3000)) {
        emit captureError("Failed to start grim (fallback): " + proc2.errorString());
        QFile::remove(tmpPath);
        return {};
    }

    if (!proc2.waitForFinished(5000)) {
        proc2.kill();
        emit captureError("grim timed out (fallback)");
        QFile::remove(tmpPath);
        return {};
    }

    if (proc2.exitCode() != 0) {
        emit captureError("grim failed: " + QString::fromUtf8(proc2.readAllStandardError()));
        QFile::remove(tmpPath);
        return {};
    }

    QFile file(tmpPath);
    QByteArray data;
    if (file.open(QIODevice::ReadOnly)) {
        data = file.readAll();
        file.close();
    }
    QFile::remove(tmpPath);

    return data;
}

// ── Windows capture using Qt / GDI ──────────────────────────

QByteArray ScreenCapturer::captureWindows()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        emit captureError("No screen available");
        return {};
    }

    // Grab entire screen (0 = entire desktop)
    QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        emit captureError("Screen grab returned null pixmap");
        return {};
    }

    // Apply scaling
    if (m_scale < 0.99) {
        QSize newSize(
            static_cast<int>(pixmap.width() * m_scale),
            static_cast<int>(pixmap.height() * m_scale)
        );
        pixmap = pixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // Encode as JPEG
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    if (!pixmap.save(&buffer, "JPEG", m_quality)) {
        emit captureError("Failed to encode JPEG");
        return {};
    }

    return data;
}

} // namespace LabMonitor
