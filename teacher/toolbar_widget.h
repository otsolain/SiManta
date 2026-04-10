#pragma once

#include <QWidget>
#include <QFrame>
#include <QToolButton>
#include <QHBoxLayout>
#include <QIcon>
#include <QList>

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
    void aboutClicked();

private:
    QToolButton* createToolButton(const QString& iconPath, const QString& label,
                                   bool enabled = true);
    QFrame* createSeparator();

    QHBoxLayout* m_layout;
};

} // namespace LabMonitor
