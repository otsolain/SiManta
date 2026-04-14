#pragma once

#include <QWidget>
#include <QFrame>
#include <QToolButton>
#include <QHBoxLayout>
#include <QIcon>
#include <QList>

namespace LabMonitor {

class ToolbarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ToolbarWidget(QWidget* parent = nullptr);

signals:
    void refreshClicked();
    void connectClicked();
    void registerClicked();
    void sendUrlClicked();
    void blockAllClicked();
    void messageClicked();
    void lockClicked();
    void unlockClicked();
    void chatClicked();
    void helpRequestsClicked();
    void sendFileClicked();
    void sendFolderClicked();
    void aboutClicked();

private:
    QToolButton* createToolButton(const QString& iconPath, const QString& label,
                                   bool enabled = true);
    QFrame* createSeparator();

    QHBoxLayout* m_layout;
};

}
