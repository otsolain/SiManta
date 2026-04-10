#include "student_tile.h"
#include "styles.h"

#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QDialog>
#include <QScreen>
#include <QApplication>
#include <QPainter>

namespace LabMonitor {

StudentTile::StudentTile(const StudentInfo& info, QWidget* parent)
    : QFrame(parent)
    , m_info(info)
{
    setupUi();
    updateInfo(info);
    setOnline(info.online);
}

StudentTile::~StudentTile() = default;

void StudentTile::setupUi()
{
    setObjectName("StudentTile");
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(m_thumbSize.width() + 16, m_thumbSize.height() + 52);

    // Drop shadow
    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(12);
    m_shadowEffect->setOffset(0, 2);
    m_shadowEffect->setColor(QColor(0, 0, 0, 30));
    setGraphicsEffect(m_shadowEffect);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(6, 6, 6, 6);
    m_layout->setSpacing(4);

    // Screenshot area
    m_screenshotLabel = new QLabel(this);
    m_screenshotLabel->setFixedSize(m_thumbSize);
    m_screenshotLabel->setAlignment(Qt::AlignCenter);
    m_screenshotLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #F0F3F5; border: 1px solid #DEE2E6; border-radius: 3px; }"
    ));
    m_screenshotLabel->setText("Connecting...");
    m_screenshotLabel->setStyleSheet(m_screenshotLabel->styleSheet() + QStringLiteral(
        "color: %1; font-family: %2; font-size: 9pt;"
    ).arg(Styles::Colors::TextMuted, Styles::Fonts::Family));

    m_layout->addWidget(m_screenshotLabel);

    // Bottom info row
    auto* infoLayout = new QHBoxLayout();
    infoLayout->setContentsMargins(2, 0, 2, 0);
    infoLayout->setSpacing(6);

    // Status dot
    m_statusDot = new QLabel(this);
    m_statusDot->setFixedSize(10, 10);
    m_statusDot->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border-radius: 5px; border: none; }"
    ).arg(Styles::Colors::StatusOffline));

    // Hostname
    m_hostnameLabel = new QLabel(this);
    m_hostnameLabel->setStyleSheet(Styles::studentNameStyle());

    infoLayout->addWidget(m_statusDot);
    infoLayout->addWidget(m_hostnameLabel, 1);

    m_layout->addLayout(infoLayout);

    // Username
    m_usernameLabel = new QLabel(this);
    m_usernameLabel->setStyleSheet(Styles::studentUserStyle());
    m_usernameLabel->setContentsMargins(18, 0, 0, 0); // align with hostname

    m_layout->addWidget(m_usernameLabel);

    // Active app label (overlay-style badge)
    m_activeAppLabel = new QLabel(this);
    m_activeAppLabel->setStyleSheet(
        "QLabel { color: #FFFFFF; background: rgba(27, 94, 40, 0.85);"
        " border-radius: 3px; padding: 1px 5px; font-size: 7pt;"
        " font-weight: bold; }"
    );
    m_activeAppLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_activeAppLabel->setMaximumHeight(16);
    m_activeAppLabel->hide();
    m_layout->addWidget(m_activeAppLabel);

    updateSelectionStyle();
}

void StudentTile::updateScreenshot(const QPixmap& pixmap)
{
    if (pixmap.isNull()) return;

    m_lastPixmap = pixmap;

    // Scale to fit, maintaining aspect ratio
    QPixmap scaled = pixmap.scaled(m_thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_screenshotLabel->setPixmap(scaled);
    m_screenshotLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #000; border: 1px solid #DEE2E6; border-radius: 3px; }"
    ));

    // Update live fullscreen view if open
    updateFullscreenView();
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
    const char* color = m_online ? Styles::Colors::StatusOnline : Styles::Colors::StatusOffline;
    m_statusDot->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border-radius: 5px; border: none; }"
    ).arg(color));
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
            "  border-radius: 6px;"
            "}"
        ).arg(Styles::Colors::CardBgSelected, Styles::Colors::CardBorderSelected));
    } else {
        setStyleSheet(QStringLiteral(
            "#StudentTile {"
            "  background: %1;"
            "  border: 2px solid %2;"
            "  border-radius: 6px;"
            "}"
        ).arg(Styles::Colors::CardBg, Styles::Colors::CardBorder));
    }
}

void StudentTile::setThumbnailSize(const QSize& size)
{
    m_thumbSize = size;
    m_screenshotLabel->setFixedSize(size);
    setFixedSize(size.width() + 16, size.height() + 52);

    // Re-scale existing pixmap
    if (!m_lastPixmap.isNull()) {
        QPixmap scaled = m_lastPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_screenshotLabel->setPixmap(scaled);
    }
}

void StudentTile::setActiveApp(const QString& appName, const QString& appClass)
{
    Q_UNUSED(appName)
    if (appClass.isEmpty()) {
        m_activeAppLabel->hide();
        return;
    }

    // Capitalize first letter of class name for display
    QString display = appClass;
    if (!display.isEmpty()) {
        display[0] = display[0].toUpper();
    }

    // Truncate if too long
    if (display.length() > 20) {
        display = display.left(17) + "...";
    }

    m_activeAppLabel->setText(QStringLiteral("▶ %1").arg(display));
    m_activeAppLabel->show();
}

void StudentTile::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Single click toggles selection
        toggleSelected();
        emit clicked(m_info.id);
    }
    QFrame::mousePressEvent(event);
}

void StudentTile::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Open or bring to front the live fullscreen viewer
        if (!m_fullscreenDialog) {
            m_fullscreenDialog = new QDialog(nullptr); // no parent so it's a real window
            m_fullscreenDialog->setWindowTitle(m_info.hostname + " - " + m_info.username + " [LIVE]");
            m_fullscreenDialog->setAttribute(Qt::WA_DeleteOnClose);
            m_fullscreenDialog->setMinimumSize(800, 600);

            auto* layout = new QVBoxLayout(m_fullscreenDialog);
            layout->setContentsMargins(0, 0, 0, 0);

            m_fullscreenLabel = new QLabel(m_fullscreenDialog);
            m_fullscreenLabel->setAlignment(Qt::AlignCenter);
            m_fullscreenLabel->setStyleSheet("background: #1a1a1a;");
            layout->addWidget(m_fullscreenLabel);

            m_fullscreenDialog->resize(960, 600);

            // Clear reference when dialog closes
            connect(m_fullscreenDialog, &QDialog::destroyed, this, [this]() {
                m_fullscreenLabel = nullptr;
            });
        }

        // Show and update
        m_fullscreenDialog->show();
        m_fullscreenDialog->raise();
        m_fullscreenDialog->activateWindow();
        updateFullscreenView();

        emit doubleClicked(m_info.id);
    }
    QFrame::mouseDoubleClickEvent(event);
}

void StudentTile::updateFullscreenView()
{
    if (!m_fullscreenDialog || !m_fullscreenLabel || m_lastPixmap.isNull())
        return;

    QSize viewSize = m_fullscreenLabel->size();
    if (viewSize.width() < 100) viewSize = m_fullscreenDialog->size();

    QPixmap scaled = m_lastPixmap.scaled(
        viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation
    );
    m_fullscreenLabel->setPixmap(scaled);
}

void StudentTile::contextMenuEvent(QContextMenuEvent* event)
{
    emit contextMenuRequested(m_info.id, event->globalPos());
}

void StudentTile::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)
    m_shadowEffect->setBlurRadius(20);
    m_shadowEffect->setOffset(0, 4);
    m_shadowEffect->setColor(QColor(0, 0, 0, 50));

    if (!m_selected) {
        setStyleSheet(QStringLiteral(
            "#StudentTile {"
            "  background: %1;"
            "  border: 2px solid %2;"
            "  border-radius: 6px;"
            "}"
        ).arg(Styles::Colors::CardBg, Styles::Colors::CardBorderSelected));
    }
}

void StudentTile::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_shadowEffect->setBlurRadius(12);
    m_shadowEffect->setOffset(0, 2);
    m_shadowEffect->setColor(QColor(0, 0, 0, 30));

    updateSelectionStyle();
}

} // namespace LabMonitor
