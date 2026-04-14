#include "lesson_panel.h"
#include "styles.h"

#include <QHBoxLayout>
#include <QGridLayout>
#include <QSettings>
#include <QMouseEvent>

namespace LabMonitor {
class ClickableWidget : public QWidget
{
public:
    using QWidget::QWidget;
    std::function<void()> onClick;
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onClick)
            onClick();
        QWidget::mousePressEvent(event);
    }
};

LessonPanel::LessonPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    loadSettings();
}

void LessonPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    auto* header = new ClickableWidget(this);
    header->setObjectName("LessonHeader");
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setAutoFillBackground(true);
    header->setStyleSheet(Styles::lessonHeaderStyle());
    header->setCursor(Qt::PointingHandCursor);
    header->setFixedHeight(32);
    m_header = header;

    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 4, 14, 4);
    headerLayout->setSpacing(8);

    m_headerIcon = new QLabel(header);
    m_headerIcon->setStyleSheet("font-size: 11pt; background: transparent; border: none;");
    m_headerLabel = new QLabel("Lesson Details", header);
    m_collapseIndicator = new QLabel("v", header);
    m_collapseIndicator->setStyleSheet(
        QStringLiteral("font-size: 8pt; background: transparent; border: none; color: %1;")
        .arg(Styles::Colors::TextSecondary));

    headerLayout->addWidget(m_headerIcon);
    headerLayout->addWidget(m_headerLabel, 1);
    headerLayout->addWidget(m_collapseIndicator);

    static_cast<ClickableWidget*>(header)->onClick = [this]() { toggleCollapsed(); };

    mainLayout->addWidget(header);
    m_body = new QWidget(this);
    m_body->setObjectName("LessonBody");
    m_body->setAttribute(Qt::WA_StyledBackground, true);
    m_body->setAutoFillBackground(true);
    m_body->setStyleSheet(Styles::lessonBodyStyle());
    m_body->setFixedHeight(m_expandedHeight);

    auto* bodyLayout = new QGridLayout(m_body);
    bodyLayout->setContentsMargins(14, 12, 14, 12);
    bodyLayout->setHorizontalSpacing(20);
    bodyLayout->setVerticalSpacing(8);
    auto* teacherLabel = new QLabel("Teacher:", m_body);
    m_teacherEdit = new QLineEdit(m_body);
    m_teacherEdit->setPlaceholderText("Enter teacher name...");
    auto* lessonLabel = new QLabel("Lesson Name:", m_body);
    m_lessonEdit = new QLineEdit(m_body);
    m_lessonEdit->setPlaceholderText("Enter lesson name...");
    auto* objectivesLabel = new QLabel("Objectives:", m_body);
    m_objectivesEdit = new QTextEdit(m_body);
    m_objectivesEdit->setPlaceholderText("Enter lesson objectives...");
    m_objectivesEdit->setMaximumHeight(50);
    auto* outcomeLabel = new QLabel("Outcome:", m_body);
    m_outcomeEdit = new QTextEdit(m_body);
    m_outcomeEdit->setPlaceholderText("Enter expected outcomes...");
    m_outcomeEdit->setMaximumHeight(50);

    bodyLayout->addWidget(teacherLabel,     0, 0);
    bodyLayout->addWidget(m_teacherEdit,    0, 1);
    bodyLayout->addWidget(lessonLabel,      0, 2);
    bodyLayout->addWidget(m_lessonEdit,     0, 3);
    bodyLayout->addWidget(objectivesLabel,  1, 0);
    bodyLayout->addWidget(m_objectivesEdit, 1, 1);
    bodyLayout->addWidget(outcomeLabel,     1, 2);
    bodyLayout->addWidget(m_outcomeEdit,    1, 3);

    bodyLayout->setColumnStretch(1, 1);
    bodyLayout->setColumnStretch(3, 1);

    mainLayout->addWidget(m_body);
    m_animation = new QPropertyAnimation(this, "bodyHeight", this);
    m_animation->setDuration(250);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

void LessonPanel::toggleCollapsed()
{
    setCollapsed(!m_collapsed);
}

void LessonPanel::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) return;
    m_collapsed = collapsed;

    m_animation->stop();
    m_animation->setStartValue(m_body->height());
    m_animation->setEndValue(collapsed ? 0 : m_expandedHeight);
    m_animation->start();

    m_collapseIndicator->setText(collapsed ? ">" : "v");

    emit collapsedChanged(collapsed);
}

int LessonPanel::bodyHeight() const
{
    return m_body->height();
}

void LessonPanel::setBodyHeight(int height)
{
    m_body->setFixedHeight(height);
    if (height == 0) {
        m_body->setVisible(false);
    } else {
        m_body->setVisible(true);
    }
}

void LessonPanel::saveSettings()
{
    QSettings settings("SiManta", "TeacherConsole");
    settings.beginGroup("LessonDetails");
    settings.setValue("teacher", m_teacherEdit->text());
    settings.setValue("lesson", m_lessonEdit->text());
    settings.setValue("objectives", m_objectivesEdit->toPlainText());
    settings.setValue("outcome", m_outcomeEdit->toPlainText());
    settings.setValue("collapsed", m_collapsed);
    settings.endGroup();
}

void LessonPanel::loadSettings()
{
    QSettings settings("SiManta", "TeacherConsole");
    settings.beginGroup("LessonDetails");
    m_teacherEdit->setText(settings.value("teacher", "").toString());
    m_lessonEdit->setText(settings.value("lesson", "").toString());
    m_objectivesEdit->setPlainText(settings.value("objectives", "").toString());
    m_outcomeEdit->setPlainText(settings.value("outcome", "").toString());

    bool collapsed = settings.value("collapsed", false).toBool();
    if (collapsed) {
        m_collapsed = true;
        m_body->setFixedHeight(0);
        m_body->setVisible(false);
        m_collapseIndicator->setText(">");
    }
    settings.endGroup();
}

}
