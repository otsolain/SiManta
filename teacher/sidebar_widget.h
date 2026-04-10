#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QButtonGroup>

namespace LabMonitor {

/**
 * SidebarWidget — Narrow left navigation sidebar with icon-only buttons.
 * Dark navy background matching NetSupport School's look.
 */
class SidebarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget* parent = nullptr);

signals:
    void viewChanged(int index);

private:
    QPushButton* createSidebarButton(const QString& iconPath, const QString& tooltip);

    QVBoxLayout*  m_layout;
    QButtonGroup* m_buttonGroup;
};

} // namespace LabMonitor
