#pragma once

#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QDialog>
#include <QPointer>

#include "protocol.h"

namespace LabMonitor {

/**
 * StudentTile — Individual student thumbnail card with screenshot,
 * status indicator, hostname, and username.
 * 
 * Features:
 * - White card with rounded corners and drop shadow
 * - Screenshot display with aspect ratio preservation
 * - Online/offline status dot
 * - Selection highlight (blue border)
 * - Hover effect (deepened shadow, subtle scale)
 * - Double-click for fullscreen view
 */
class StudentTile : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected)

public:
    explicit StudentTile(const StudentInfo& info, QWidget* parent = nullptr);
    ~StudentTile() override;

    // Update the screenshot
    void updateScreenshot(const QPixmap& pixmap);

    // Update student info
    void updateInfo(const StudentInfo& info);

    // Set online/offline status
    void setOnline(bool online);
    bool isOnline() const { return m_online; }

    // Selection
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    void toggleSelected();

    // Get student info
    const StudentInfo& studentInfo() const { return m_info; }
    QString studentId() const { return m_info.id; }
    QString studentName() const { return m_info.hostname; }

    // Set thumbnail size
    void setThumbnailSize(const QSize& size);

    // Set active app display
    void setActiveApp(const QString& appName, const QString& appClass);

signals:
    void clicked(const QString& studentId);
    void doubleClicked(const QString& studentId);
    void contextMenuRequested(const QString& studentId, const QPoint& pos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void setupUi();
    void updateStatusDot();
    void updateSelectionStyle();
    void updateFullscreenView();

    StudentInfo  m_info;
    bool         m_selected = false;
    bool         m_online = false;
    QSize        m_thumbSize = QSize(240, 160);

    // UI elements
    QVBoxLayout* m_layout = nullptr;
    QLabel*      m_screenshotLabel = nullptr;
    QLabel*      m_statusDot = nullptr;
    QLabel*      m_hostnameLabel = nullptr;
    QLabel*      m_usernameLabel = nullptr;
    QLabel*      m_activeAppLabel = nullptr;

    QGraphicsDropShadowEffect* m_shadowEffect = nullptr;
    QPixmap m_lastPixmap;

    // Live fullscreen viewer
    QPointer<QDialog> m_fullscreenDialog;
    QLabel*           m_fullscreenLabel = nullptr;
};

} // namespace LabMonitor
