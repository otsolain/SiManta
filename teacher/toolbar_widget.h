#pragma once

#include <QWidget>
#include <QFrame>
#include <QToolButton>
#include <QHBoxLayout>
#include <QIcon>
#include <QList>
#include <QLabel>

namespace LabMonitor {

/**
 * ToolbarWidget — Custom ribbon-style toolbar matching NetSupport School's look.
 * 
 * Buttons grouped by function with vertical separators, SVG icons above text labels.
 */
class ToolbarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ToolbarWidget(QWidget* parent = nullptr);
    void setUnreadHelpCount(int count);

signals:
    void refreshClicked();
    void registerClicked();
    void sendUrlClicked();
    void broadcastClicked();
    void lockClicked();
    void unlockClicked();
    void disconnectClicked();
    void chatClicked();
    void helpRequestsClicked();
    void sendFileClicked();
    void sendFolderClicked();
    void retrieveFileClicked();
public:
    void updateTranslations();

private:
    QToolButton* createToolButton(const QString& iconPath, const QString& label,
                                   bool enabled = true);

    QHBoxLayout* m_layout;
    QToolButton* m_btnRefresh = nullptr;
    QToolButton* m_btnRegister = nullptr;
    QToolButton* m_btnSendUrl = nullptr;
    QToolButton* m_btnBroadcast = nullptr;
    QToolButton* m_btnLock = nullptr;
    QToolButton* m_btnUnlock = nullptr;
    QToolButton* m_btnDisconnect = nullptr;
    QToolButton* m_btnChat = nullptr;
    QToolButton* m_btnHelp = nullptr;
    QToolButton* m_btnSendFile = nullptr;
    QToolButton* m_btnSendFolder = nullptr;
    QToolButton* m_btnRetrieve = nullptr;
    QLabel* m_helpBadge = nullptr;
};

} // namespace LabMonitor
