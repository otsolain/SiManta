#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QButtonGroup>

namespace LabMonitor {

class SidebarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget* parent = nullptr);

signals:
    void viewChanged(int index);

public:
    void updateTranslations();

private:
    QPushButton* createSidebarButton(const QString& iconPath, const QString& tooltip);

    QVBoxLayout*  m_layout;
    QButtonGroup* m_buttonGroup;
    QPushButton* m_btnMonitor;
    QPushButton* m_btnInternet;
    QPushButton* m_btnSecurity;
    QPushButton* m_btnSettings;
};

} // namespace LabMonitor
