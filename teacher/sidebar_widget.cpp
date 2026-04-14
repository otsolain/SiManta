#include "sidebar_widget.h"
#include "styles.h"

#include <QIcon>

namespace LabMonitor {

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("SidebarWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setStyleSheet(Styles::sidebarStyle());
    setFixedWidth(50);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 6, 0, 6);
    m_layout->setSpacing(2);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    auto* btnMonitor  = createSidebarButton(":/icons/icons/monitor.svg", "All Students");
    auto* btnInternet = createSidebarButton(":/icons/icons/send_url.svg", "Internet");
    auto* btnSecurity = createSidebarButton(":/icons/icons/security.svg", "Security");
    auto* btnSettings = createSidebarButton(":/icons/icons/settings.svg", "Settings");

    m_buttonGroup->addButton(btnMonitor, 0);
    m_buttonGroup->addButton(btnInternet, 1);
    m_buttonGroup->addButton(btnSecurity, 2);
    m_buttonGroup->addButton(btnSettings, 3);

    m_layout->addWidget(btnMonitor);
    m_layout->addWidget(btnInternet);
    m_layout->addWidget(btnSecurity);
    m_layout->addWidget(btnSettings);
    m_layout->addStretch();
    btnMonitor->setChecked(true);

    connect(m_buttonGroup, &QButtonGroup::idClicked,
            this, &SidebarWidget::viewChanged);
}

QPushButton* SidebarWidget::createSidebarButton(const QString& iconPath, const QString& tooltip)
{
    auto* btn = new QPushButton(this);
    btn->setStyleSheet(Styles::sidebarButtonStyle());
    btn->setCheckable(true);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(22, 22));
    btn->setToolTip(tooltip);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(50, 46);
    return btn;
}

}
