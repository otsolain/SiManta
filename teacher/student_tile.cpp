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
#include <QMenu>
#include <QAction>
#include "../common/lang.h"

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
    // Extra height for: hostname + username + cpuRam + appInfo row
    setFixedSize(m_thumbSize.width() + 20, m_thumbSize.height() + 90);
    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(14);
    m_shadowEffect->setOffset(0, 2);
    m_shadowEffect->setColor(QColor(15, 23, 42, 40));
    setGraphicsEffect(m_shadowEffect);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 6);
    m_layout->setSpacing(1);
    m_fullscreenThrottle = new QTimer(this);
    m_fullscreenThrottle->setInterval(100); // Fast refresh for live fullscreen view
    m_fullscreenThrottle->setSingleShot(true);
    connect(m_fullscreenThrottle, &QTimer::timeout, this, [this]() {
        if (m_fullscreenDirty) {
            m_fullscreenDirty = false;
            updateFullscreenView();
        }
    });

    // ── Screenshot container with overlays ──
    m_screenshotContainer = new QWidget(this);
    m_screenshotContainer->setFixedSize(m_thumbSize);

    m_screenshotLabel = new QLabel(m_screenshotContainer);
    m_screenshotLabel->setFixedSize(m_thumbSize);
    m_screenshotLabel->setAlignment(Qt::AlignCenter);
    m_screenshotLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #E8EEF4; border: none; border-radius: 6px;"
        " color: %1; font-family: %2; font-size: 9pt; }"
    ).arg(Styles::Colors::TextMuted, Styles::Fonts::Family));
    m_screenshotLabel->setText(Lang::get().t("Connecting...", "Menyambung..."));

    // App icon & badge will be added to the bottom row instead of the screenshot container

    m_layout->addWidget(m_screenshotContainer);

    // ── Row 1: status dot + hostname ──
    auto* infoLayout = new QHBoxLayout();
    infoLayout->setContentsMargins(2, 4, 2, 0);
    infoLayout->setSpacing(6);

    m_statusDot = new QLabel(this);
    m_statusDot->setFixedSize(10, 10);
    updateStatusDot();

    m_hostnameLabel = new QLabel(this);
    m_hostnameLabel->setStyleSheet(Styles::studentNameStyle());

    infoLayout->addWidget(m_statusDot);
    infoLayout->addWidget(m_hostnameLabel, 1);
    m_layout->addLayout(infoLayout);

    // ── Row 2: username ──
    m_usernameLabel = new QLabel(this);
    m_usernameLabel->setStyleSheet(Styles::studentUserStyle());
    m_usernameLabel->setContentsMargins(18, 0, 0, 0);
    m_layout->addWidget(m_usernameLabel);

    // ── Row 3: CPU/RAM ──
    auto* cpuLayout = new QHBoxLayout();
    cpuLayout->setContentsMargins(18, 0, 8, 0);
    cpuLayout->setSpacing(0);

    m_cpuRamLabel = new QLabel(this);
    m_cpuRamLabel->setStyleSheet(
        "QLabel { color: #2E8B57; background: transparent;"
        " font-size: 7pt;"
        " font-family: 'Consolas', 'Courier New', monospace; border: none; }"
    );
    m_cpuRamLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_cpuRamLabel->hide();
    cpuLayout->addWidget(m_cpuRamLabel);
    cpuLayout->addStretch();
    m_layout->addLayout(cpuLayout);

    // ── Row 4: Active App Info ──
    auto* appLayout = new QHBoxLayout();
    appLayout->setContentsMargins(18, 0, 8, 4);
    appLayout->setSpacing(6);

    m_appIconLabel = new QLabel(this);
    m_appIconLabel->setFixedSize(14, 14);
    m_appIconLabel->setAlignment(Qt::AlignCenter);
    m_appIconLabel->setStyleSheet("background: transparent;");
    m_appIconLabel->hide();

    m_appBadge = new QLabel(this);
    m_appBadge->setStyleSheet(
        "QLabel { color: #64748B; background: transparent;"
        " font-size: 8pt; font-weight: 500; border: none; }"
    );
    m_appBadge->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_appBadge->hide();

    appLayout->addWidget(m_appIconLabel);
    appLayout->addWidget(m_appBadge, 1);
    m_layout->addLayout(appLayout);

    updateSelectionStyle();
}

void StudentTile::updateScreenshot(const QPixmap& pixmap)
{
    if (pixmap.isNull()) return;

    // Any incoming frame implies the student is alive -> flip back to online.
    if (!m_online) {
        m_online = true;
        updateStatusDot();
    }

    m_lastPixmap = pixmap;
    QPixmap scaled = pixmap.scaled(m_thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_screenshotLabel->setText(QString()); // clear any "Reconnecting..." text
    m_screenshotLabel->setPixmap(scaled);
    m_screenshotLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #E8EEF4; border: none; border-radius: 6px; }"
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
    // If no custom display name is set, show hostname. Otherwise keep custom.
    if (m_displayName.isEmpty()) {
        m_hostnameLabel->setText(info.hostname);
    } else {
        m_hostnameLabel->setText(m_displayName);
    }
    m_usernameLabel->setText(info.username);
}

void StudentTile::setDisplayName(const QString& name)
{
    QString trimmed = name.trimmed();
    if (m_displayName == trimmed) return;
    m_displayName = trimmed;
    if (m_hostnameLabel) {
        m_hostnameLabel->setText(m_displayName.isEmpty() ? m_info.hostname : m_displayName);
    }
    emit displayNameChanged(m_info.id, displayName());
}

QString StudentTile::displayName() const
{
    return m_displayName.isEmpty() ? m_info.hostname : m_displayName;
}

void StudentTile::setOnline(bool online)
{
    if (m_online == online) return;
    m_online = online;
    updateStatusDot();

    // When we go offline, keep the last pixmap visible and just dim the status
    // dot. Clearing the pixmap every time the watchdog flips state causes a
    // visible flicker between the real screenshot and "Menyambung..." text.
    // We only show the text placeholder if we actually have no frame yet.
    if (!online && m_lastPixmap.isNull()) {
        m_screenshotLabel->setText(Lang::get().t("Reconnecting...", "Menyambung ulang..."));
    }
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

        m_shadowEffect->setBlurRadius(20);
        m_shadowEffect->setOffset(0, 3);
        m_shadowEffect->setColor(QColor(31, 111, 235, 50));
    } else {
        setStyleSheet(QStringLiteral(
            "#StudentTile {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 10px;"
            "}"
        ).arg(Styles::Colors::CardBg, Styles::Colors::CardBorder));

        m_shadowEffect->setBlurRadius(14);
        m_shadowEffect->setOffset(0, 2);
        m_shadowEffect->setColor(QColor(15, 23, 42, 40));
    }
}

void StudentTile::setThumbnailSize(const QSize& size)
{
    m_thumbSize = size;
    m_screenshotContainer->setFixedSize(size);
    m_screenshotLabel->setFixedSize(size);
    setFixedSize(size.width() + 20, size.height() + 90);
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
        m_appIconLabel->setPixmap(icon.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_appIconLabel->show();
    } else {
        m_appIconLabel->hide();
    }
    QString display = appClass;
    if (!display.isEmpty()) {
        display[0] = display[0].toUpper();
    }
    if (display.length() > 20) {
        display = display.left(17) + "...";
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

    QString color;
    if (cpu >= 80) color = "#FF6B6B";      // red = high
    else if (cpu >= 50) color = "#FFD93D"; // yellow = medium
    else color = "#A0D4A0";                // green = normal

    m_cpuRamLabel->setStyleSheet(
        QStringLiteral(
            "QLabel { color: %1; background: transparent;"
            " font-size: 7pt;"
            " font-family: 'Consolas', 'Courier New', monospace; border: none; }"
        ).arg(color)
    );
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
        emit fullscreenOpened(m_info.id);
    }
    QFrame::mouseDoubleClickEvent(event);
}

void StudentTile::createFullscreenDialog()
{
    m_fullscreenDialog = new QDialog(nullptr);
    m_fullscreenDialog->setWindowTitle(displayName() + " - " + m_info.username + " [LIVE]");
    m_fullscreenDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_fullscreenDialog->setMinimumSize(800, 600);
    m_fullscreenDialog->setStyleSheet("QDialog { background: #F0F4F8; }");

    auto* mainLayout = new QVBoxLayout(m_fullscreenDialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    auto* toolbar = new QWidget(m_fullscreenDialog);
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(
        "QWidget { background: #0F2A44; border-bottom: 1px solid rgba(0,60,120,0.12); }"
    );

    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 4, 12, 4);
    tbLayout->setSpacing(8);

    auto* studentLabel = new QLabel(
        QStringLiteral("%1 - %2").arg(displayName(), m_info.username), toolbar);
    studentLabel->setStyleSheet("color: #1A2233; font-weight: bold; font-size: 10pt; border: none;");

    // Keep the title bar and toolbar label in sync if the teacher renames
    // this student while the fullscreen view is open.
    connect(this, &StudentTile::displayNameChanged,
            m_fullscreenDialog, [this, studentLabel](const QString&, const QString& newName) {
        if (m_fullscreenDialog) {
            m_fullscreenDialog->setWindowTitle(newName + " - " + m_info.username + " [LIVE]");
        }
        if (studentLabel) {
            studentLabel->setText(QStringLiteral("%1 - %2").arg(newName, m_info.username));
        }
    });

    auto createZoomBtn = [&](const QString& text, const QString& tooltip) {
        auto* btn = new QPushButton(text, toolbar);
        btn->setToolTip(tooltip);
        btn->setFixedSize(32, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: rgba(0,60,120,0.12); color: #1A2233;"
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
        "QScrollArea { background: #F0F4F8; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 10px; }"
        "QScrollBar::handle:vertical { background: #E8EEF4; border-radius: 5px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #484F58; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: transparent; height: 10px; }"
        "QScrollBar::handle:horizontal { background: #E8EEF4; border-radius: 5px; min-width: 30px; }"
        "QScrollBar::handle:horizontal:hover { background: #484F58; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
    );

    m_fullscreenLabel = new QLabel();
    m_fullscreenLabel->setAlignment(Qt::AlignCenter);
    m_fullscreenLabel->setStyleSheet("background: #F0F4F8; border: none;");
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
        emit fullscreenClosed(m_info.id);
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
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background:#FFFFFF; color:#1E293B; border:1px solid #CBD5E1;"
        " border-radius:8px; padding:4px; }"
        "QMenu::item { padding:8px 24px 8px 16px; border-radius:6px; }"
        "QMenu::item:selected { background:#E8F0FE; color:#1A73E8; }"
        "QMenu::separator { height:1px; background:#E2E8F0; margin:4px 6px; }"
    );

    QAction* renameAct = menu.addAction(
        Lang::get().t("Rename this computer...", "Ubah nama komputer ini..."));
    menu.addSeparator();
    QAction* clearAct = nullptr;
    if (!m_displayName.isEmpty()) {
        clearAct = menu.addAction(
            Lang::get().t("Reset to hostname", "Kembalikan ke nama host"));
    }

    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen) return;

    if (chosen == renameAct) {
        emit renameRequested(m_info.id);
    } else if (clearAct && chosen == clearAct) {
        setDisplayName(QString());
        emit renameCleared(m_info.id);
    }

    emit contextMenuRequested(m_info.id, event->globalPos());
}

void StudentTile::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)
    m_shadowEffect->setBlurRadius(20);
    m_shadowEffect->setOffset(0, 3);
    m_shadowEffect->setColor(QColor(15, 23, 42, 70));

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
    m_shadowEffect->setBlurRadius(14);
    m_shadowEffect->setOffset(0, 2);
    m_shadowEffect->setColor(QColor(15, 23, 42, 40));

    updateSelectionStyle();
}

} // namespace LabMonitor

