#pragma once

#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QDialog>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>

#include "protocol.h"

namespace LabMonitor {

class StudentTile : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected)

public:
    explicit StudentTile(const StudentInfo& info, QWidget* parent = nullptr);
    ~StudentTile() override;
    void updateScreenshot(const QPixmap& pixmap);
    void updateInfo(const StudentInfo& info);
    void setOnline(bool online);
    bool isOnline() const { return m_online; }
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    void toggleSelected();
    const StudentInfo& studentInfo() const { return m_info; }
    QString studentId() const { return m_info.id; }
    QString studentName() const { return m_info.hostname; }
    void setThumbnailSize(const QSize& size);
    void setActiveApp(const QString& appName, const QString& appClass, const QPixmap& icon);
    void setCpuRam(double cpu, double ram);

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
    void createFullscreenDialog();

    StudentInfo  m_info;
    bool         m_selected = false;
    bool         m_online = false;
    QSize        m_thumbSize = QSize(240, 160);
    QVBoxLayout* m_layout = nullptr;
    QWidget*     m_screenshotContainer = nullptr;
    QLabel*      m_screenshotLabel = nullptr;
    QLabel*      m_statusDot = nullptr;
    QLabel*      m_hostnameLabel = nullptr;
    QLabel*      m_usernameLabel = nullptr;
    QLabel*      m_appBadge = nullptr;
    QLabel*      m_appIconLabel = nullptr;
    QLabel*      m_cpuRamLabel = nullptr;

    QGraphicsDropShadowEffect* m_shadowEffect = nullptr;
    QPixmap m_lastPixmap;
    QPointer<QDialog> m_fullscreenDialog;
    QLabel*           m_fullscreenLabel = nullptr;
    QScrollArea*      m_fullscreenScroll = nullptr;
    double            m_zoomLevel = 1.0;
    QTimer*           m_fullscreenThrottle = nullptr;
    bool              m_fullscreenDirty = false;
};

}
