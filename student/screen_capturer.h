#pragma once

#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <QTimer>

namespace LabMonitor {

/**
 * ScreenCapturer — cross-platform screen capture
 *
 * Linux/Wayland: uses grim for efficient capture
 * Windows:       uses Qt QScreen::grabWindow()
 */
class ScreenCapturer : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapturer(QObject* parent = nullptr);
    ~ScreenCapturer() override;

    // Configuration
    void setQuality(int quality);    // JPEG quality 1-100 (default 60)
    void setScale(double scale);     // Scale factor 0.1-1.0 (default 0.5)

    int quality() const { return m_quality; }
    double scale() const { return m_scale; }

    // Capture a single frame. Returns JPEG data, or empty on failure.
    QByteArray capture();

    // Check if screen capture is available on this platform
    static bool isGrimAvailable();

signals:
    void captureError(const QString& error);

private:
    QByteArray captureLinux();
    QByteArray captureWindows();

    int    m_quality = 60;
    double m_scale   = 0.5;
};

} // namespace LabMonitor
