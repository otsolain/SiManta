#pragma once

#include <QObject>
#include <QByteArray>
#include <QImage>
#include <QList>

namespace LabMonitor {

/**
 * DeltaTile — a single changed tile in a delta frame.
 */
struct DeltaTile {
    uint16_t   x = 0;
    uint16_t   y = 0;
    uint16_t   w = 0;
    uint16_t   h = 0;
    QByteArray jpegData;
};

/**
 * DeltaResult — result of a captureDelta() call.
 *
 *  isEmpty    — nothing changed since last frame (skip sending entirely)
 *  isFullFrame — send fullJpeg instead of individual tiles
 *  tiles       — changed tile list (only used when !isFullFrame)
 */
struct DeltaResult {
    bool             isEmpty     = false;
    bool             isFullFrame = false;
    QByteArray       fullJpeg;
    QList<DeltaTile> tiles;
};

/**
 * ScreenCapturer — Windows GDI screen capture with Veyon-like delta encoding.
 *
 * Features:
 * - GDI BitBlt capture (multi-monitor via SM_CXVIRTUALSCREEN)
 * - Cursor overlay drawn into captured frame
 * - JPEG compression with configurable quality
 * - Configurable scale factor for bandwidth optimization
 * - Delta frame: only sends changed 64×64 tiles (huge bandwidth saving)
 * - Falls back to full frame when >60% tiles change or on first capture
 */
class ScreenCapturer : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapturer(QObject* parent = nullptr);
    ~ScreenCapturer() override;

    void   setQuality(int quality);    // JPEG quality 1-100
    void   setScale(double scale);     // Scale factor 0.1-1.0

    int    quality() const { return m_quality; }
    double scale()   const { return m_scale;   }

    /** Full JPEG capture (backward compatible). */
    QByteArray capture();

    /**
     * Delta capture — compares against previous frame and returns only
     * the changed tiles.  Forces a full frame on first call or resolution change.
     */
    DeltaResult captureDelta();

    /** Force next captureDelta() to send a full frame (e.g. when switching quality modes). */
    void resetDelta() { m_previousFrame = QImage(); }

    static bool isAvailable() { return true; }

signals:
    void captureError(const QString& error);

private:
    /** Capture raw screen into a QImage (GDI-based, includes cursor). */
    QImage captureRaw();

    int    m_quality = 60;
    double m_scale   = 0.5;

    QImage m_previousFrame;   // last captured frame for delta comparison

    static constexpr int TILE_SIZE = 64; // pixels per tile side
};

} // namespace LabMonitor
