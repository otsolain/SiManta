#include "screen_capturer.h"

#include <QDebug>
#include <QBuffer>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QImage>

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

QByteArray ScreenCapturer::capture()
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
                            hScreenDC, screenX, screenY, SRCCOPY | CAPTUREBLT);

    if (!bltResult) {
        SelectObject(hMemDC, hOld);
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        ReleaseDC(nullptr, hScreenDC);
        emit captureError("BitBlt failed");
        return {};
    }
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
    bi.biHeight = -screenH;
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
    if (m_scale < 0.99) {
        QSize newSize(
            static_cast<int>(image.width() * m_scale),
            static_cast<int>(image.height() * m_scale)
        );
        image = image.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPEG", m_quality)) {
        emit captureError("Failed to encode JPEG");
        return {};
    }

    return data;
}

}
