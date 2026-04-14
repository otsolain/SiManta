#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QPropertyAnimation>

namespace LabMonitor {

class LessonPanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int bodyHeight READ bodyHeight WRITE setBodyHeight)

public:
    explicit LessonPanel(QWidget* parent = nullptr);
    void saveSettings();
    void loadSettings();
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
    QWidget* m_header = nullptr;
    QLabel*  m_headerIcon = nullptr;
    QLabel*  m_headerLabel = nullptr;
    QLabel*  m_collapseIndicator = nullptr;
    QWidget*   m_body = nullptr;
    QLineEdit* m_teacherEdit = nullptr;
    QLineEdit* m_lessonEdit = nullptr;
    QTextEdit* m_objectivesEdit = nullptr;
    QTextEdit* m_outcomeEdit = nullptr;
    QPropertyAnimation* m_animation = nullptr;
    int m_expandedHeight = 130;
};

}
