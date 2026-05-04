#include "toolbar_widget.h"
#include "styles.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QIcon>
#include "../common/lang.h"

namespace LabMonitor {

ToolbarWidget::ToolbarWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("ToolbarWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setStyleSheet(Styles::toolbarStyle());
    setFixedHeight(88);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 4, 8, 4);
    m_layout->setSpacing(2);

    m_btnRefresh  = createToolButton(":/icons/icons/refresh.svg", Lang::get().t("Refresh", "Segarkan"));
    m_btnRegister = createToolButton(":/icons/icons/register.svg", Lang::get().t("Student\nRegister", "Daftar\nSiswa"));
    m_btnSendUrl  = createToolButton(":/icons/icons/send_url.svg", Lang::get().t("Send Url", "Kirim URL"));

    m_layout->addWidget(m_btnRefresh);
    m_layout->addWidget(m_btnRegister);
    m_layout->addWidget(m_btnSendUrl);

    m_btnBroadcast = createToolButton(":/icons/icons/message.svg", Lang::get().t("Broadcast", "Siaran"), true);
    m_btnLock      = createToolButton(":/icons/icons/lock.svg", Lang::get().t("Lock", "Kunci"), true);
    m_btnUnlock    = createToolButton(":/icons/icons/unlock.svg", Lang::get().t("Unlock", "Buka Kunci"), true);
    m_btnDisconnect = createToolButton(":/icons/icons/block_all.svg", Lang::get().t("Disconnect", "Putuskan"), true);

    m_layout->addWidget(m_btnBroadcast);
    m_layout->addWidget(m_btnLock);
    m_layout->addWidget(m_btnUnlock);
    m_layout->addWidget(m_btnDisconnect);

    m_btnChat = createToolButton(":/icons/icons/chat.svg", Lang::get().t("Chat", "Obrolan"), true);
    m_btnHelp = createToolButton(":/icons/icons/help.svg", Lang::get().t("Help\nRequests", "Permintaan\nBantuan"), true);

    m_helpBadge = new QLabel(m_btnHelp);
    m_helpBadge->setFixedSize(20, 20);
    m_helpBadge->setAlignment(Qt::AlignCenter);
    m_helpBadge->setStyleSheet(
        "QLabel { background: #EA4335; color: white; font-size: 8pt;"
        " font-weight: bold; border-radius: 10px; border: none; }");
    m_helpBadge->hide();
    m_helpBadge->move(50, 4);

    m_layout->addWidget(m_btnChat);
    m_layout->addWidget(m_btnHelp);



    m_btnSendFile    = createToolButton(":/icons/icons/send_url.svg", Lang::get().t("Send\nFile", "Kirim\nFile"), true);
    m_btnSendFolder  = createToolButton(":/icons/icons/send_url.svg", Lang::get().t("Send\nFolder", "Kirim\nFolder"), true);
    m_btnRetrieve    = createToolButton(":/icons/icons/help.svg", Lang::get().t("Retrieve\nFile", "Tarik\nFile"), true);

    m_layout->addWidget(m_btnSendFile);
    m_layout->addWidget(m_btnSendFolder);
    m_layout->addWidget(m_btnRetrieve);

    m_layout->addStretch();



    // Connect signals
    connect(m_btnRefresh,    &QToolButton::clicked, this, &ToolbarWidget::refreshClicked);
    connect(m_btnRegister,   &QToolButton::clicked, this, &ToolbarWidget::registerClicked);
    connect(m_btnSendUrl,    &QToolButton::clicked, this, &ToolbarWidget::sendUrlClicked);
    connect(m_btnBroadcast,  &QToolButton::clicked, this, &ToolbarWidget::broadcastClicked);
    connect(m_btnLock,       &QToolButton::clicked, this, &ToolbarWidget::lockClicked);
    connect(m_btnUnlock,     &QToolButton::clicked, this, &ToolbarWidget::unlockClicked);
    connect(m_btnDisconnect, &QToolButton::clicked, this, &ToolbarWidget::disconnectClicked);
    connect(m_btnChat,       &QToolButton::clicked, this, &ToolbarWidget::chatClicked);
    connect(m_btnHelp,       &QToolButton::clicked, this, &ToolbarWidget::helpRequestsClicked);
    connect(m_btnSendFile,   &QToolButton::clicked, this, &ToolbarWidget::sendFileClicked);
    connect(m_btnSendFolder, &QToolButton::clicked, this, &ToolbarWidget::sendFolderClicked);
    connect(m_btnRetrieve,   &QToolButton::clicked, this, &ToolbarWidget::retrieveFileClicked);

}

QToolButton* ToolbarWidget::createToolButton(const QString& iconPath, const QString& label,
                                              bool enabled)
{
    auto* btn = new QToolButton(this);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(28, 28));
    btn->setText(label);
    btn->setEnabled(enabled);
    btn->setCursor(enabled ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
    btn->setFixedWidth(82);
    btn->setMaximumHeight(76);
    btn->setAutoRaise(true);

    btn->setStyleSheet(Styles::toolbarButtonStyle());

    return btn;
}

void ToolbarWidget::setUnreadHelpCount(int count)
{
    if (m_helpBadge) {
        if (count > 0) {
            m_helpBadge->setText(QString::number(count > 99 ? 99 : count));
            m_helpBadge->show();
        } else {
            m_helpBadge->hide();
        }
    }
}

void ToolbarWidget::updateTranslations()
{
    if (m_btnRefresh) m_btnRefresh->setText(Lang::get().t("Refresh", "Segarkan"));
    if (m_btnRegister) m_btnRegister->setText(Lang::get().t("Student\nRegister", "Daftar\nSiswa"));
    if (m_btnSendUrl) m_btnSendUrl->setText(Lang::get().t("Send Url", "Kirim URL"));
    if (m_btnBroadcast) m_btnBroadcast->setText(Lang::get().t("Broadcast", "Siaran"));
    if (m_btnLock) m_btnLock->setText(Lang::get().t("Lock", "Kunci"));
    if (m_btnUnlock) m_btnUnlock->setText(Lang::get().t("Unlock", "Buka Kunci"));
    if (m_btnDisconnect) m_btnDisconnect->setText(Lang::get().t("Disconnect", "Putuskan"));
    if (m_btnChat) m_btnChat->setText(Lang::get().t("Chat", "Obrolan"));
    if (m_btnHelp) m_btnHelp->setText(Lang::get().t("Help\nRequests", "Permintaan\nBantuan"));
    if (m_btnSendFile) m_btnSendFile->setText(Lang::get().t("Send\nFile", "Kirim\nFile"));
    if (m_btnSendFolder) m_btnSendFolder->setText(Lang::get().t("Send\nFolder", "Kirim\nFolder"));
    if (m_btnRetrieve) m_btnRetrieve->setText(Lang::get().t("Retrieve\nFile", "Tarik\nFile"));
}

} // namespace LabMonitor
