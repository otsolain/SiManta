#pragma once

#include <QMainWindow>
#include <QSlider>
#include <QLabel>
#include <QShortcut>
#include <QSettings>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollBar>

#include "toolbar_widget.h"
#include "sidebar_widget.h"
#include "student_grid.h"
#include "lesson_panel.h"
#include "connection_manager.h"

namespace LabMonitor {

class TutorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TutorWindow(QWidget* parent = nullptr);
    ~TutorWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onStudentConnected(const StudentInfo& info);
    void onStudentDisconnected(const QString& studentId);
    void onFrameReceived(const QString& studentId, const QPixmap& frame);
    void onThumbSizeChanged(int value);
    void onSelectionChanged();
    void onRefresh();

private:
    void setupUi();
    void setupStatusBar();
    void setupShortcuts();
    void setupConnectionManager();
    void updateStatusLabel();
    void saveSettings();
    void loadSettings();
    ToolbarWidget*     m_toolbar = nullptr;
    SidebarWidget*     m_sidebar = nullptr;
    StudentGrid*       m_grid = nullptr;
    LessonPanel*       m_lessonPanel = nullptr;
    ConnectionManager* m_connManager = nullptr;
    QWidget* m_statusBarWidget = nullptr;
    QLabel*  m_updateSpeedLabel = nullptr;
    QSlider* m_updateSpeedSlider = nullptr;
    QLabel*  m_thumbSizeLabel = nullptr;
    QSlider* m_thumbSizeSlider = nullptr;
    QLabel*  m_statusLabel = nullptr;
    QShortcut* m_selectAllShortcut = nullptr;
    QShortcut* m_deselectShortcut = nullptr;
    QShortcut* m_refreshShortcut = nullptr;
    QStringList m_helpRequests;
};

}
