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
#include <QSet>
#include <QTimer>
#include <functional>
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
    void setupSettingsTab();
    void updateTranslations();
    void setupStatusBar();
    void setupShortcuts();
    void setupConnectionManager();
    void updateStatusLabel();
    void saveSettings();
    void loadSettings();

    // Apply the current internet/whitelist policy to a specific student
    // (used when a new student connects) or to the current target set.
    void applyPolicyToStudent(const QString& studentId);
    QStringList currentPolicyTargetIds() const;
    // Build the WHITELIST_SET command with expanded related domains so that
    // even older student builds (without relatedDomains logic) get the full
    // list of hosts needed for whitelisted sites to render correctly.
    QString buildWhitelistCommand() const;
    ToolbarWidget*     m_toolbar = nullptr;
    SidebarWidget*     m_sidebar = nullptr;
    QStackedWidget*    m_stackedWidget = nullptr;
    StudentGrid*       m_grid = nullptr;
    QWidget*           m_internetWidget = nullptr;
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

    // -- Policy state (for auto-applying to newly connected students) --
    QSet<QString> m_policyTargetIds;         // students that receive the policy
    bool          m_policyAutoAddNewStudents = false;  // auto-check newly connected students
    bool          m_internetBlocked = false;
    QStringList   m_whitelistUrls;

    QSystemTrayIcon* m_trayIcon = nullptr;
    QTimer*          m_staleFrameCheckTimer = nullptr;

    // Callback set by setupInternetTab() so onStudentConnected/Disconnected
    // can refresh the student list regardless of which order the subsystems
    // were constructed in.
    std::function<void()> m_rebuildInternetStudentList;
};

} // namespace LabMonitor

