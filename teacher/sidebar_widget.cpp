#include "sidebar_widget.h"
#include "styles.h"

#include <QIcon>
#include "../common/lang.h"

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

    m_btnMonitor  = createSidebarButton(":/icons/icons/monitor.svg", Lang::get().t("All Students", "Semua Siswa"));
    m_btnInternet = createSidebarButton(":/icons/icons/send_url.svg", Lang::get().t("Internet", "Internet"));
    m_btnSecurity = createSidebarButton(":/icons/icons/security.svg", Lang::get().t("Security", "Keamanan"));
    m_btnSettings = createSidebarButton(":/icons/icons/settings.svg", Lang::get().t("Settings", "Pengaturan"));

    m_buttonGroup->addButton(m_btnMonitor, 0);
    m_buttonGroup->addButton(m_btnInternet, 1);
    m_buttonGroup->addButton(m_btnSecurity, 2);
    m_buttonGroup->addButton(m_btnSettings, 3);

    m_layout->addWidget(m_btnMonitor);
    m_layout->addWidget(m_btnInternet);
    m_layout->addWidget(m_btnSecurity);
    m_layout->addWidget(m_btnSettings);
    m_layout->addStretch();
    m_btnMonitor->setChecked(true);

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

void SidebarWidget::updateTranslations()
{
    m_btnMonitor->setToolTip(Lang::get().t("All Students", "Semua Siswa"));
    m_btnInternet->setToolTip(Lang::get().t("Internet", "Internet"));
    m_btnSecurity->setToolTip(Lang::get().t("Security", "Keamanan"));
    m_btnSettings->setToolTip(Lang::get().t("Settings", "Pengaturan"));
}

} // namespace LabMonitor

