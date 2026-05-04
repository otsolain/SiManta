#include "screen_capturer.h"

#include <QDebug>
#include <QBuffer>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QImage>
#include <cstring>

#include <windows.h>

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

// ── Core GDI capture → returns scaled QImage ──
QImage ScreenCapturer::captureRaw()
{
    HDC hScreenDC = GetDC(nullptr);
    if (!hScreenDC) {
        emit captureError("GetDC(NULL) failed");
        return {};
    }

    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

    if (screenW <= 0 || screenH <= 0) {
        screenW = GetSystemMetrics(SM_CXSCREEN);
        screenH = GetSystemMetrics(SM_CYSCREEN);
        screenX = 0;
        screenY = 0;
    }

    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) {
        ReleaseDC(nullptr, hScreenDC);
        emit captureError("CreateCompatibleDC failed");
        return {};
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenW, screenH);
    if (!hBitmap) {
        DeleteDC(hMemDC);
        ReleaseDC(nullptr, hScreenDC);
        emit captureError("CreateCompatibleBitmap failed");
        return {};
    }

    HGDIOBJ hOld = SelectObject(hMemDC, hBitmap);
    BOOL bltResult = BitBlt(hMemDC, 0, 0, screenW, screenH,
                            hScreenDC, screenX, screenY, SRCCOPY);

    if (!bltResult) {
        SelectObject(hMemDC, hOld);
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        ReleaseDC(nullptr, hScreenDC);
        emit captureError("BitBlt failed");
        return {};
    }

    // Draw cursor
    CURSORINFO ci = {};
    ci.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING)) {
        HCURSOR hCursor = ci.hCursor;
        ICONINFO iconInfo = {};
        if (hCursor && GetIconInfo(hCursor, &iconInfo)) {
            int curX = ci.ptScreenPos.x - screenX - static_cast<int>(iconInfo.xHotspot);
            int curY = ci.ptScreenPos.y - screenY - static_cast<int>(iconInfo.yHotspot);
            DrawIconEx(hMemDC, curX, curY, hCursor, 0, 0, 0, nullptr, DI_NORMAL);
            if (iconInfo.hbmMask)  DeleteObject(iconInfo.hbmMask);
            if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        }
    }

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = screenW;
    bi.biHeight = -screenH;  // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage image(screenW, screenH, QImage::Format_ARGB32);
    GetDIBits(hMemDC, hBitmap, 0, screenH, image.bits(),
              reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    SelectObject(hMemDC, hOld);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);

    if (image.isNull()) {
        emit captureError("GDI capture produced null image");
        return {};
    }

    // Scale down for bandwidth
    if (m_scale < 0.99) {
        QSize newSize(
            static_cast<int>(image.width() * m_scale),
            static_cast<int>(image.height() * m_scale)
        );
        image = image.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}

// ── Full JPEG capture (backward compatible) ──
QByteArray ScreenCapturer::capture()
{
    QImage image = captureRaw();
    if (image.isNull()) return {};

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPEG", m_quality)) {
        emit captureError("Failed to encode JPEG");
        return {};
    }
    return data;
}

DeltaResult ScreenCapturer::captureDelta()
{
    DeltaResult result;
    QImage current = captureRaw();
    if (current.isNull()) {
        result.isEmpty = true;
        return result;
    }

    // Ensure ARGB32 format for consistent pixel comparison
    if (current.format() != QImage::Format_ARGB32) {
        current = current.convertToFormat(QImage::Format_ARGB32);
    }

    // First frame or resolution changed → send full frame
    if (m_previousFrame.isNull() || m_previousFrame.size() != current.size()
        || m_previousFrame.format() != current.format()) {
        m_previousFrame = current.copy();
        result.isFullFrame = true;
        QBuffer buf(&result.fullJpeg);
        buf.open(QIODevice::WriteOnly);
        current.save(&buf, "JPEG", m_quality);
        return result;
    }

    // ── Tile comparison ──
    int imgW = current.width();
    int imgH = current.height();
    int tilesX = (imgW + TILE_SIZE - 1) / TILE_SIZE;
    int tilesY = (imgH + TILE_SIZE - 1) / TILE_SIZE;
    int totalTiles = tilesX * tilesY;

    for (int ty = 0; ty < tilesY; ty++) {
        for (int tx = 0; tx < tilesX; tx++) {
            int x = tx * TILE_SIZE;
            int y = ty * TILE_SIZE;
            int w = qMin(TILE_SIZE, imgW - x);
            int h = qMin(TILE_SIZE, imgH - y);

            // Fast comparison: sample every 4th row for speed
            bool changed = false;
            for (int row = y; row < y + h && !changed; row += 4) {
                const uchar* cur  = current.constScanLine(row) + x * 4;
                const uchar* prev = m_previousFrame.constScanLine(row) + x * 4;
                if (std::memcmp(cur, prev, static_cast<size_t>(w * 4)) != 0) {
                    changed = true;
                }
            }

            if (changed) {
                DeltaTile tile;
                tile.x = static_cast<uint16_t>(x);
                tile.y = static_cast<uint16_t>(y);
                tile.w = static_cast<uint16_t>(w);
                tile.h = static_cast<uint16_t>(h);
                QImage tileImg = current.copy(x, y, w, h);
                QBuffer buf(&tile.jpegData);
                buf.open(QIODevice::WriteOnly);
                tileImg.save(&buf, "JPEG", m_quality);
                result.tiles.append(tile);
            }
        }
    }

    // Save current as previous for next comparison
    m_previousFrame = current.copy();

    // Nothing changed → skip frame entirely (huge bandwidth saving!)
    if (result.tiles.isEmpty()) {
        result.isEmpty = true;
        result.isFullFrame = false;
        return result;
    }

    // If >60% tiles changed → send full frame (more efficient than many tiles)
    if (result.tiles.size() > totalTiles * 6 / 10) {
        result.isFullFrame = true;
        result.tiles.clear();
        QBuffer buf(&result.fullJpeg);
        buf.open(QIODevice::WriteOnly);
        current.save(&buf, "JPEG", m_quality);
    } else {
        result.isFullFrame = false;
    }

    return result;
}

} // namespace LabMonitor
