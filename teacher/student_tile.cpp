#include "student_tile.h"
#include "styles.h"

#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QDialog>
#include <QScreen>
#include <QApplication>
#include <QPainter>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

namespace LabMonitor {

StudentTile::StudentTile(const StudentInfo& info, QWidget* parent)
    : QFrame(parent)
    , m_info(info)
{
    setupUi();
    updateInfo(info);
    setOnline(info.online);
}

StudentTile::~StudentTile()
{
    if (m_fullscreenDialog) {
        m_fullscreenDialog->close();
        m_fullscreenDialog->deleteLater();
    }
}

void StudentTile::setupUi()
{
    setObjectName("StudentTile");
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(m_thumbSize.width() + 20, m_thumbSize.height() + 48);
    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(16);
    m_shadowEffect->setOffset(0, 4);
    m_shadowEffect->setColor(QColor(0, 0, 0, 80));
    setGraphicsEffect(m_shadowEffect);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 6);
    m_layout->setSpacing(4);
    m_fullscreenThrottle = new QTimer(this);
    m_fullscreenThrottle->setInterval(500);
    m_fullscreenThrottle->setSingleShot(true);
    connect(m_fullscreenThrottle, &QTimer::timeout, this, [this]() {
        if (m_fullscreenDirty) {
            m_fullscreenDirty = false;
            updateFullscreenView();
        }
    });
    m_screenshotContainer = new QWidget(this);
    m_screenshotContainer->setFixedSize(m_thumbSize);

    m_screenshotLabel = new QLabel(m_screenshotContainer);
    m_screenshotLabel->setFixedSize(m_thumbSize);
    m_screenshotLabel->setAlignment(Qt::AlignCenter);
    m_screenshotLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #21262D; border: 1px solid rgba(255,255,255,0.06);"
        " border-radius: 6px; color: %1; font-family: %2; font-size: 9pt; }"
    ).arg(Styles::Colors::TextMuted, Styles::Fonts::Family));
    m_screenshotLabel->setText("Connecting...");
    m_appIconLabel = new QLabel(m_screenshotContainer);
    m_appIconLabel->setFixedSize(20, 20);
    m_appIconLabel->setStyleSheet(
        "QLabel { background: rgba(0,0,0,0.6); border-radius: 4px;"
        " padding: 2px; border: none; }"
    );
    m_appIconLabel->setAlignment(Qt::AlignCenter);
    m_appIconLabel->move(6, 6);
    m_appIconLabel->hide();
    m_appBadge = new QLabel(m_screenshotContainer);
    m_appBadge->setStyleSheet(
        "QLabel { color: #FFFFFF; background: rgba(0,0,0,0.6);"
        " border-radius: 4px; padding: 2px 6px; font-size: 7pt;"
        " font-weight: bold; border: none; }"
    );
    m_appBadge->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_appBadge->setMaximumHeight(20);
    m_appBadge->move(30, 6);
    m_appBadge->hide();

    m_layout->addWidget(m_screenshotContainer);
    m_cpuRamLabel = new QLabel(m_screenshotContainer);
    m_cpuRamLabel->setStyleSheet(
        "QLabel { color: #A0D4A0; background: rgba(0,0,0,0.65);"
        " border-radius: 4px; padding: 2px 6px; font-size: 7pt;"
        " font-family: 'Consolas', 'Courier New', monospace; border: none; }"
    );
    m_cpuRamLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_cpuRamLabel->hide();
    auto* infoLayout = new QHBoxLayout();
    infoLayout->setContentsMargins(2, 0, 2, 0);
    infoLayout->setSpacing(6);
    m_statusDot = new QLabel(this);
    m_statusDot->setFixedSize(10, 10);
    updateStatusDot();
    m_hostnameLabel = new QLabel(this);
    m_hostnameLabel->setStyleSheet(Styles::studentNameStyle());

    infoLayout->addWidget(m_statusDot);
    infoLayout->addWidget(m_hostnameLabel, 1);

    m_layout->addLayout(infoLayout);
    m_usernameLabel = new QLabel(this);
    m_usernameLabel->setStyleSheet(Styles::studentUserStyle());
    m_usernameLabel->setContentsMargins(18, 0, 0, 0);

    m_layout->addWidget(m_usernameLabel);

    updateSelectionStyle();
}

void StudentTile::updateScreenshot(const QPixmap& pixmap)
{
    if (pixmap.isNull()) return;

    m_lastPixmap = pixmap;
    QPixmap scaled = pixmap.scaled(m_thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_screenshotLabel->setPixmap(scaled);
    m_screenshotLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #000; border: 1px solid rgba(255,255,255,0.06); border-radius: 6px; }"
    ));
    if (m_fullscreenDialog && m_fullscreenLabel) {
        m_fullscreenDirty = true;
        if (!m_fullscreenThrottle->isActive()) {
            m_fullscreenThrottle->start();
        }
    }
}

void StudentTile::updateInfo(const StudentInfo& info)
{
    m_info = info;
    m_hostnameLabel->setText(info.hostname);
    m_usernameLabel->setText(info.username);
}

void StudentTile::setOnline(bool online)
{
    m_online = online;
    updateStatusDot();
}

void StudentTile::updateStatusDot()
{
    if (m_online) {
        m_statusDot->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; border-radius: 5px; border: none; }"
        ).arg(Styles::Colors::StatusOnline));
    } else {
        m_statusDot->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; border-radius: 5px; border: none; }"
        ).arg(Styles::Colors::StatusOffline));
    }
}

void StudentTile::setSelected(bool selected)
{
    if (m_selected == selected) return;
    m_selected = selected;
    setProperty("selected", selected);
    updateSelectionStyle();
}

void StudentTile::toggleSelected()
{
    setSelected(!m_selected);
}

void StudentTile::updateSelectionStyle()
{
    if (m_selected) {
        setStyleSheet(QStringLiteral(
            "#StudentTile {"
            "  background: %1;"
            "  border: 2px solid %2;"
            "  border-radius: 10px;"
            "}"
        ).arg(Styles::Colors::CardBgSelected, Styles::Colors::CardBorderSelected));

        m_shadowEffect->setBlurRadius(24);
        m_shadowEffect->setColor(QColor(31, 111, 235, 60));
    } else {
        setStyleSheet(QStringLiteral(
            "#StudentTile {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 10px;"
            "}"
        ).arg(Styles::Colors::CardBg, Styles::Colors::CardBorder));

        m_shadowEffect->setBlurRadius(16);
        m_shadowEffect->setOffset(0, 4);
        m_shadowEffect->setColor(QColor(0, 0, 0, 80));
    }
}

void StudentTile::setThumbnailSize(const QSize& size)
{
    m_thumbSize = size;
    m_screenshotContainer->setFixedSize(size);
    m_screenshotLabel->setFixedSize(size);
    setFixedSize(size.width() + 20, size.height() + 48);
    if (!m_lastPixmap.isNull()) {
        QPixmap scaled = m_lastPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_screenshotLabel->setPixmap(scaled);
    }
}

void StudentTile::setActiveApp(const QString& appName, const QString& appClass, const QPixmap& icon)
{
    Q_UNUSED(appName)
    if (appClass.isEmpty()) {
        m_appBadge->hide();
        m_appIconLabel->hide();
        return;
    }
    if (!icon.isNull()) {
        m_appIconLabel->setPixmap(icon.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_appIconLabel->show();
        m_appBadge->move(30, 6);
    } else {
        m_appIconLabel->hide();
        m_appBadge->move(6, 6);
    }
    QString display = appClass;
    if (!display.isEmpty()) {
        display[0] = display[0].toUpper();
    }
    if (display.length() > 18) {
        display = display.left(15) + "...";
    }

    m_appBadge->setText(display);
    m_appBadge->adjustSize();
    m_appBadge->show();
}

void StudentTile::setCpuRam(double cpu, double ram)
{
    if (cpu < 0 && ram < 0) {
        m_cpuRamLabel->hide();
        return;
    }

    QString text = QStringLiteral("CPU %1%  RAM %2%")
        .arg(static_cast<int>(cpu))
        .arg(static_cast<int>(ram));

    m_cpuRamLabel->setText(text);
    m_cpuRamLabel->adjustSize();
    QString color;
    if (cpu >= 80) color = "#FF6B6B";
    else if (cpu >= 50) color = "#FFD93D";
    else color = "#A0D4A0";

    m_cpuRamLabel->setStyleSheet(
        QStringLiteral(
            "QLabel { color: %1; background: rgba(0,0,0,0.65);"
            " border-radius: 4px; padding: 2px 6px; font-size: 7pt;"
            " font-family: 'Consolas', 'Courier New', monospace; border: none; }"
        ).arg(color)
    );
    int x = m_thumbSize.width() - m_cpuRamLabel->width() - 6;
    int y = m_thumbSize.height() - m_cpuRamLabel->height() - 6;
    m_cpuRamLabel->move(x, y);
    m_cpuRamLabel->show();
}

void StudentTile::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_info.id);
    }
    QFrame::mousePressEvent(event);
}

void StudentTile::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (!m_fullscreenDialog) {
            createFullscreenDialog();
        }

        m_fullscreenDialog->show();
        m_fullscreenDialog->raise();
        m_fullscreenDialog->activateWindow();
        m_zoomLevel = 1.0;
        updateFullscreenView();

        emit doubleClicked(m_info.id);
    }
    QFrame::mouseDoubleClickEvent(event);
}

void StudentTile::createFullscreenDialog()
{
    m_fullscreenDialog = new QDialog(nullptr);
    m_fullscreenDialog->setWindowTitle(m_info.hostname + " - " + m_info.username + " [LIVE]");
    m_fullscreenDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_fullscreenDialog->setMinimumSize(800, 600);
    m_fullscreenDialog->setStyleSheet("QDialog { background: #0D1117; }");

    auto* mainLayout = new QVBoxLayout(m_fullscreenDialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    auto* toolbar = new QWidget(m_fullscreenDialog);
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(
        "QWidget { background: #0F2A44; border-bottom: 1px solid rgba(255,255,255,0.1); }"
    );

    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 4, 12, 4);
    tbLayout->setSpacing(8);

    auto* studentLabel = new QLabel(
        QStringLiteral("%1 - %2").arg(m_info.hostname, m_info.username), toolbar);
    studentLabel->setStyleSheet("color: #E6EDF3; font-weight: bold; font-size: 10pt; border: none;");

    auto createZoomBtn = [&](const QString& text, const QString& tooltip) {
        auto* btn = new QPushButton(text, toolbar);
        btn->setToolTip(tooltip);
        btn->setFixedSize(32, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: rgba(255,255,255,0.1); color: #E6EDF3;"
            " border: 1px solid rgba(255,255,255,0.15); border-radius: 4px;"
            " font-weight: bold; font-size: 12pt; }"
            "QPushButton:hover { background: rgba(88,166,255,0.2); }"
            "QPushButton:pressed { background: rgba(88,166,255,0.3); }"
        );
        return btn;
    };

    auto* btnZoomOut = createZoomBtn("-", "Zoom Out (Ctrl+-)");
    auto* zoomLabel = new QLabel("100%", toolbar);
    zoomLabel->setStyleSheet("color: #B0C4DE; font-size: 9pt; min-width: 45px; border: none;");
    zoomLabel->setAlignment(Qt::AlignCenter);
    auto* btnZoomIn = createZoomBtn("+", "Zoom In (Ctrl++)");
    auto* btnFit = createZoomBtn("[ ]", "Fit to Window");
    btnFit->setFixedWidth(40);

    tbLayout->addWidget(studentLabel);
    tbLayout->addStretch();
    tbLayout->addWidget(btnZoomOut);
    tbLayout->addWidget(zoomLabel);
    tbLayout->addWidget(btnZoomIn);
    tbLayout->addWidget(btnFit);

    mainLayout->addWidget(toolbar);
    m_fullscreenScroll = new QScrollArea(m_fullscreenDialog);
    m_fullscreenScroll->setWidgetResizable(false);
    m_fullscreenScroll->setAlignment(Qt::AlignCenter);
    m_fullscreenScroll->setStyleSheet(
        "QScrollArea { background: #0D1117; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 10px; }"
        "QScrollBar::handle:vertical { background: #21262D; border-radius: 5px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #484F58; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: transparent; height: 10px; }"
        "QScrollBar::handle:horizontal { background: #21262D; border-radius: 5px; min-width: 30px; }"
        "QScrollBar::handle:horizontal:hover { background: #484F58; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
    );

    m_fullscreenLabel = new QLabel();
    m_fullscreenLabel->setAlignment(Qt::AlignCenter);
    m_fullscreenLabel->setStyleSheet("background: #0D1117; border: none;");
    m_fullscreenScroll->setWidget(m_fullscreenLabel);

    mainLayout->addWidget(m_fullscreenScroll, 1);

    m_fullscreenDialog->resize(1100, 700);
    connect(btnZoomIn, &QPushButton::clicked, m_fullscreenDialog, [=]() {
        m_zoomLevel = qMin(m_zoomLevel + 0.25, 4.0);
        zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoomLevel * 100)));
        updateFullscreenView();
    });

    connect(btnZoomOut, &QPushButton::clicked, m_fullscreenDialog, [=]() {
        m_zoomLevel = qMax(m_zoomLevel - 0.25, 0.25);
        zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoomLevel * 100)));
        updateFullscreenView();
    });

    connect(btnFit, &QPushButton::clicked, m_fullscreenDialog, [=]() {
        m_zoomLevel = 1.0;
        zoomLabel->setText("100%");
        updateFullscreenView();
    });

    connect(m_fullscreenDialog, &QDialog::destroyed, this, [this]() {
        m_fullscreenLabel = nullptr;
        m_fullscreenScroll = nullptr;
    });
}

void StudentTile::updateFullscreenView()
{
    if (!m_fullscreenDialog || !m_fullscreenLabel || m_lastPixmap.isNull())
        return;

    if (m_zoomLevel == 1.0 && m_fullscreenScroll) {
        QSize viewSize = m_fullscreenScroll->viewport()->size();
        if (viewSize.width() < 100) viewSize = m_fullscreenDialog->size();

        QPixmap scaled = m_lastPixmap.scaled(
            viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation
        );
        m_fullscreenLabel->setPixmap(scaled);
        m_fullscreenLabel->resize(scaled.size());
    } else {
        QSize zoomedSize(
            static_cast<int>(m_lastPixmap.width() * m_zoomLevel),
            static_cast<int>(m_lastPixmap.height() * m_zoomLevel)
        );
        QPixmap scaled = m_lastPixmap.scaled(
            zoomedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation
        );
        m_fullscreenLabel->setPixmap(scaled);
        m_fullscreenLabel->resize(scaled.size());
    }
}

void StudentTile::contextMenuEvent(QContextMenuEvent* event)
{
    emit contextMenuRequested(m_info.id, event->globalPos());
}

void StudentTile::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)
    m_shadowEffect->setBlurRadius(24);
    m_shadowEffect->setOffset(0, 6);
    m_shadowEffect->setColor(QColor(0, 0, 0, 120));

    if (!m_selected) {
        setStyleSheet(QStringLiteral(
            "#StudentTile {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 10px;"
            "}"
        ).arg(Styles::Colors::CardBgHover, Styles::Colors::CardBorderHover));
    }
}

void StudentTile::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_shadowEffect->setBlurRadius(16);
    m_shadowEffect->setOffset(0, 4);
    m_shadowEffect->setColor(QColor(0, 0, 0, 80));

    updateSelectionStyle();
}

}
