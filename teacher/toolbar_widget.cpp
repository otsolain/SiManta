#include "toolbar_widget.h"
#include "styles.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QIcon>

namespace LabMonitor {

ToolbarWidget::ToolbarWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("ToolbarWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setStyleSheet(Styles::toolbarStyle());
    setFixedHeight(76);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 4, 8, 4);
    m_layout->setSpacing(2);

    // ── Group 1: Connection ──
    auto* btnRefresh  = createToolButton(":/icons/icons/refresh.svg", "Refresh");
    auto* btnConnect  = createToolButton(":/icons/icons/connect.svg", "Connect");
    auto* btnRegister = createToolButton(":/icons/icons/register.svg", "Student\nRegister");
    auto* btnSendUrl  = createToolButton(":/icons/icons/send_url.svg", "Send Url");

    m_layout->addWidget(btnRefresh);
    m_layout->addWidget(btnConnect);
    m_layout->addWidget(btnRegister);
    m_layout->addWidget(btnSendUrl);

    m_layout->addWidget(createSeparator());

    // ── Group 2: Control ──
    auto* btnBlockAll = createToolButton(":/icons/icons/block_all.svg", "Block All", true);
    auto* btnMessage  = createToolButton(":/icons/icons/message.svg", "Message", true);
    auto* btnLock     = createToolButton(":/icons/icons/lock.svg", "Lock", true);
    auto* btnUnlock   = createToolButton(":/icons/icons/unlock.svg", "Unlock", true);

    m_layout->addWidget(btnBlockAll);
    m_layout->addWidget(btnMessage);
    m_layout->addWidget(btnLock);
    m_layout->addWidget(btnUnlock);

    m_layout->addWidget(createSeparator());

    // ── Group 3: Communication ──
    auto* btnChat = createToolButton(":/icons/icons/chat.svg", "Chat", true);
    auto* btnHelp = createToolButton(":/icons/icons/help.svg", "Help\nRequests", true);

    m_layout->addWidget(btnChat);
    m_layout->addWidget(btnHelp);

    // Spacer to push About to the right
    m_layout->addStretch();

    // ── Group 4: About ──
    auto* btnAbout = createToolButton(":/icons/icons/about.svg", "About");
    m_layout->addWidget(btnAbout);

    // ── Connections ──
    connect(btnRefresh, &QToolButton::clicked, this, &ToolbarWidget::refreshClicked);
    connect(btnConnect, &QToolButton::clicked, this, &ToolbarWidget::connectClicked);
    connect(btnRegister, &QToolButton::clicked, this, &ToolbarWidget::registerClicked);
    connect(btnSendUrl, &QToolButton::clicked, this, &ToolbarWidget::sendUrlClicked);
    connect(btnBlockAll, &QToolButton::clicked, this, &ToolbarWidget::blockAllClicked);
    connect(btnMessage, &QToolButton::clicked, this, &ToolbarWidget::messageClicked);
    connect(btnLock, &QToolButton::clicked, this, &ToolbarWidget::lockClicked);
    connect(btnUnlock, &QToolButton::clicked, this, &ToolbarWidget::unlockClicked);
    connect(btnChat, &QToolButton::clicked, this, &ToolbarWidget::chatClicked);
    connect(btnHelp, &QToolButton::clicked, this, &ToolbarWidget::helpRequestsClicked);
    connect(btnAbout, &QToolButton::clicked, this, &ToolbarWidget::aboutClicked);
}

QToolButton* ToolbarWidget::createToolButton(const QString& iconPath, const QString& label,
                                              bool enabled)
{
    auto* btn = new QToolButton(this);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(24, 24));
    btn->setText(label);
    btn->setEnabled(enabled);
    btn->setCursor(enabled ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
    btn->setMinimumWidth(72);
    btn->setMaximumHeight(66);
    btn->setAutoRaise(true);

    btn->setStyleSheet(Styles::toolbarButtonStyle());

    return btn;
}

QFrame* ToolbarWidget::createSeparator()
{
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet(Styles::toolbarSeparatorStyle());
    sep->setFixedHeight(54);
    return sep;
}

} // namespace LabMonitor
