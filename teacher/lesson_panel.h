#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QPropertyAnimation>

namespace LabMonitor {

/**
 * LessonPanel — Collapsible panel showing lesson details.
 * Header bar (dark gradient) toggles body visibility with smooth animation.
 * Fields: Teacher, Lesson Name, Objectives, Outcome.
 * Data persists via QSettings.
 */
class LessonPanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int bodyHeight READ bodyHeight WRITE setBodyHeight)

public:
    explicit LessonPanel(QWidget* parent = nullptr);

    // Save/load from QSettings
    void saveSettings();
    void loadSettings();

    // Visibility
    bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool collapsed);

    int bodyHeight() const;
    void setBodyHeight(int height);

signals:
    void collapsedChanged(bool collapsed);

private:
    void toggleCollapsed();
    void setupUi();

    bool m_collapsed = false;

    // Header
    QWidget* m_header = nullptr;
    QLabel*  m_headerIcon = nullptr;
    QLabel*  m_headerLabel = nullptr;
    QLabel*  m_collapseIndicator = nullptr;

    // Body
    QWidget*   m_body = nullptr;
    QLineEdit* m_teacherEdit = nullptr;
    QLineEdit* m_lessonEdit = nullptr;
    QTextEdit* m_objectivesEdit = nullptr;
    QTextEdit* m_outcomeEdit = nullptr;

    // Animation
    QPropertyAnimation* m_animation = nullptr;
    int m_expandedHeight = 130;
};

} // namespace LabMonitor
