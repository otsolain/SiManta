#pragma once

#include <QMainWindow>
#include <QSlider>
#include <QLabel>
#include <QShortcut>
#include <QSettings>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollBar>
#include <QMap>
#include <QSystemTrayIcon>
#include <QStackedWidget>
#include <QGroupBox>
#include <QFormLayout>
#include <QListWidget>
#include <QComboBox>
#include <QFormLayout>

#include "toolbar_widget.h"
#include "sidebar_widget.h"
#include "student_grid.h"

#include "connection_manager.h"

namespace LabMonitor {

/**
 * TutorWindow — Main application window for the Teacher Console.
 * 
 * Assembles all UI components:
 * - Toolbar (ribbon-style, top)
 * - Sidebar (navigation, left)
 * - Student Grid (thumbnail flow, center)
 * - Lesson Panel (collapsible, bottom)
 * - Status Bar (connection info + controls, bottom)
 *
 * Manages ConnectionManager and routes events to UI.
 */
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
    void setupInternetTab();
    void setupSecurityTab();
    void setupSettingsTab();
    void updateTranslations();
    void setupStatusBar();
    void setupShortcuts();
    void setupConnectionManager();
    void updateStatusLabel();
    void saveSettings();
    void loadSettings();
    ToolbarWidget*     m_toolbar = nullptr;
    SidebarWidget*     m_sidebar = nullptr;
    QStackedWidget*    m_stackedWidget = nullptr;
    StudentGrid*       m_grid = nullptr;
    QWidget*           m_internetWidget = nullptr;
    QWidget*           m_securityWidget = nullptr;
    QWidget*           m_settingsWidget = nullptr;

    QComboBox*         m_langCombo = nullptr;
    QLabel*            m_settingsHeader = nullptr;
    QLabel*            m_settingsDesc = nullptr;
    QLabel*            m_langLabel = nullptr;

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
    int m_unreadHelpCount = 0;

    // Chat history per student (persists across dialog open/close within session)
    struct ChatEntry {
        QString sender;
        QString message;
        QString time;
        bool isTeacher;
    };
    QMap<QString, QList<ChatEntry>> m_chatHistory;
    
    QSystemTrayIcon* m_trayIcon = nullptr;
};

} // namespace LabMonitor

