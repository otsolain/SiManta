#include "tutor_window.h"
#include "styles.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDebug>
#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QDateTime>
#include <QStatusBar>

namespace LabMonitor {

static const char* DIALOG_STYLE = R"(
    QDialog { background-color: #FFFFFF; color: #2C3E35; }
    QLabel { color: #2C3E35; background: transparent; }
    QTextEdit { color: #2C3E35; background-color: #F0F7F2; }
    QLineEdit { color: #2C3E35; background-color: #FFFFFF; }
    QCheckBox { color: #2C3E35; }
    QTableWidget { color: #2C3E35; background-color: #FFFFFF; }
)";

TutorWindow::TutorWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupStatusBar();
    setupShortcuts();
    setupConnectionManager();
    loadSettings();
}

TutorWindow::~TutorWindow()
{
    saveSettings();
}

void TutorWindow::setupUi()
{
    // Window properties
    setWindowTitle("Lab Monitor - Teacher Console");
    setMinimumSize(900, 600);
    resize(1200, 800);
    setStyleSheet(Styles::mainWindowStyle());

    // Central widget
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Toolbar (top) ──
    m_toolbar = new ToolbarWidget(this);
    mainLayout->addWidget(m_toolbar);

    // ── Middle section: Sidebar + Grid ──
    auto* middleWidget = new QWidget(this);
    auto* middleLayout = new QHBoxLayout(middleWidget);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(0);

    // Sidebar (left)
    m_sidebar = new SidebarWidget(this);
    middleLayout->addWidget(m_sidebar);

    // Student Grid (center, takes remaining space)
    m_grid = new StudentGrid(this);
    middleLayout->addWidget(m_grid, 1);

    mainLayout->addWidget(middleWidget, 1);

    // ── Lesson Panel (bottom, collapsible) ──
    m_lessonPanel = new LessonPanel(this);
    mainLayout->addWidget(m_lessonPanel);

    // ── Toolbar connections ──
    connect(m_toolbar, &ToolbarWidget::refreshClicked,
            this, &TutorWindow::onRefresh);
    connect(m_toolbar, &ToolbarWidget::aboutClicked, this, [this]() {
        QMessageBox::about(this, "About Lab Monitor",
            "<h2>Lab Monitor v1.0</h2>"
            "<p>Professional Classroom Management System</p>"
            "<p>Monitor student screens in real-time.</p>"
            "<hr>"
            "<p><b>Architecture:</b> C++ / Qt6 / TCP</p>"
            "<p><b>Protocol:</b> Binary packet (12-byte header)</p>"
            "<p>Built for institutional deployment.</p>"
        );
    });

    // Message button
    connect(m_toolbar, &ToolbarWidget::messageClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, "Message", "No students connected.");
            return;
        }

        // Create message dialog
        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Send Message to Students");
        dialog->setMinimumSize(450, 300);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(DIALOG_STYLE);

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(10);
        layout->setContentsMargins(16, 16, 16, 16);

        // Title field
        auto* titleLabel = new QLabel("Title:", dialog);
        titleLabel->setStyleSheet("font-weight: bold; font-size: 10pt;");
        auto* titleEdit = new QLineEdit(dialog);
        titleEdit->setPlaceholderText("Message title...");
        titleEdit->setText("Message from Teacher");
        titleEdit->setStyleSheet("padding: 6px; font-size: 10pt; border: 1px solid #BDC7C0; border-radius: 4px;");

        // Body field
        auto* bodyLabel = new QLabel("Message:", dialog);
        bodyLabel->setStyleSheet("font-weight: bold; font-size: 10pt;");
        auto* bodyEdit = new QTextEdit(dialog);
        bodyEdit->setPlaceholderText("Type your message here...");
        bodyEdit->setStyleSheet("padding: 6px; font-size: 10pt; border: 1px solid #BDC7C0; border-radius: 4px;");

        // Send to all checkbox
        auto* sendAllCheck = new QCheckBox("Send to ALL students", dialog);
        auto selectedTiles = m_grid->selectedTiles();
        if (selectedTiles.isEmpty()) {
            sendAllCheck->setChecked(true);
            sendAllCheck->setEnabled(false);
        } else {
            sendAllCheck->setChecked(false);
            sendAllCheck->setText(QStringLiteral("Send to ALL students (otherwise %1 selected)")
                                 .arg(selectedTiles.size()));
        }

        // Buttons
        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
        buttons->button(QDialogButtonBox::Ok)->setText("Send");
        buttons->button(QDialogButtonBox::Ok)->setStyleSheet(
            "QPushButton { background: #27AE60; color: white; padding: 8px 24px;"
            " border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background: #2ECC71; }");

        layout->addWidget(titleLabel);
        layout->addWidget(titleEdit);
        layout->addWidget(bodyLabel);
        layout->addWidget(bodyEdit, 1);
        layout->addWidget(sendAllCheck);
        layout->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, dialog, [=]() {
            QString title = titleEdit->text().trimmed();
            QString body = bodyEdit->toPlainText().trimmed();

            if (body.isEmpty()) {
                QMessageBox::warning(dialog, "Empty Message", "Please enter a message.");
                return;
            }

            QString sender = "Teacher";

            if (sendAllCheck->isChecked()) {
                m_connManager->sendMessageToAll(title, body, sender);
                qInfo() << "[TutorWindow] Sent message to ALL students";
            } else {
                QStringList ids;
                for (auto* tile : m_grid->selectedTiles()) {
                    ids.append(tile->studentId());
                }
                m_connManager->sendMessage(ids, title, body, sender);
                qInfo() << "[TutorWindow] Sent message to" << ids.size() << "students";
            }

            dialog->accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

        dialog->show();
    });

    // ── Lock button ──
    connect(m_toolbar, &ToolbarWidget::lockClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            m_connManager->sendLockAll();
            statusBar()->showMessage("🔒 Locked ALL students", 3000);
        } else {
            QStringList ids;
            for (auto* t : selected) ids.append(t->studentId());
            m_connManager->sendLockScreen(ids);
            statusBar()->showMessage(QStringLiteral("🔒 Locked %1 student(s)").arg(ids.size()), 3000);
        }
    });

    // ── Unlock button ──
    connect(m_toolbar, &ToolbarWidget::unlockClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            m_connManager->sendUnlockAll();
            statusBar()->showMessage("🔓 Unlocked ALL students", 3000);
        } else {
            QStringList ids;
            for (auto* t : selected) ids.append(t->studentId());
            m_connManager->sendUnlockScreen(ids);
            statusBar()->showMessage(QStringLiteral("🔓 Unlocked %1 student(s)").arg(ids.size()), 3000);
        }
    });

    // ── Block All button (locks all screens) ──
    connect(m_toolbar, &ToolbarWidget::blockAllClicked, this, [this]() {
        m_connManager->sendLockAll();
        statusBar()->showMessage("🔒 Blocked ALL students", 3000);
    });

    // ── Send URL button ──
    connect(m_toolbar, &ToolbarWidget::sendUrlClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, "Send URL", "No students connected.");
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Send URL to Students");
        dialog->setMinimumWidth(450);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(DIALOG_STYLE);

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(10);
        layout->setContentsMargins(16, 16, 16, 16);

        auto* label = new QLabel("Enter URL to open on student browsers:", dialog);
        label->setStyleSheet("font-weight: bold; font-size: 10pt;");

        auto* urlEdit = new QLineEdit(dialog);
        urlEdit->setPlaceholderText("https://example.com");
        urlEdit->setText("https://");
        urlEdit->setStyleSheet("padding: 8px; font-size: 11pt; border: 1px solid #BDC7C0; border-radius: 4px;");

        auto* sendAllCheck = new QCheckBox("Send to ALL students", dialog);
        auto selectedTiles = m_grid->selectedTiles();
        if (selectedTiles.isEmpty()) {
            sendAllCheck->setChecked(true);
            sendAllCheck->setEnabled(false);
        } else {
            sendAllCheck->setChecked(false);
            sendAllCheck->setText(QStringLiteral("Send to ALL (otherwise %1 selected)").arg(selectedTiles.size()));
        }

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
        buttons->button(QDialogButtonBox::Ok)->setText("Send");
        buttons->button(QDialogButtonBox::Ok)->setStyleSheet(
            "QPushButton { background: #27AE60; color: white; padding: 8px 24px;"
            " border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background: #2ECC71; }");

        layout->addWidget(label);
        layout->addWidget(urlEdit);
        layout->addWidget(sendAllCheck);
        layout->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, dialog, [=]() {
            QString url = urlEdit->text().trimmed();
            if (url.isEmpty() || url == "https://") {
                QMessageBox::warning(dialog, "Invalid URL", "Please enter a valid URL.");
                return;
            }
            if (sendAllCheck->isChecked()) {
                m_connManager->sendUrlToAll(url);
            } else {
                QStringList ids;
                for (auto* t : m_grid->selectedTiles()) ids.append(t->studentId());
                m_connManager->sendUrl(ids, url);
            }
            dialog->accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        dialog->show();
    });

    // ── Student Register button ──
    connect(m_toolbar, &ToolbarWidget::registerClicked, this, [this]() {
        auto students = m_connManager->connectedStudents();
        if (students.isEmpty()) {
            QMessageBox::information(this, "Student Register", "No students connected.");
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Student Register");
        dialog->setMinimumSize(650, 350);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(DIALOG_STYLE);

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(16, 16, 16, 16);

        auto* header = new QLabel(QStringLiteral("📋 Connected Students (%1)").arg(students.size()), dialog);
        header->setStyleSheet("font-size: 13pt; font-weight: bold; color: #1B5E28; padding-bottom: 8px;");

        auto* table = new QTableWidget(students.size(), 6, dialog);
        table->setHorizontalHeaderLabels({"Hostname", "Username", "IP Address", "OS", "Resolution", "Connected"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->setAlternatingRowColors(true);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setStyleSheet("QTableWidget { font-size: 9pt; } QHeaderView::section { background: #1B5E28; color: white; padding: 6px; font-weight: bold; }");

        for (int i = 0; i < students.size(); ++i) {
            const auto& s = students[i];
            table->setItem(i, 0, new QTableWidgetItem(s.hostname));
            table->setItem(i, 1, new QTableWidgetItem(s.username));
            table->setItem(i, 2, new QTableWidgetItem(s.ipAddress));
            table->setItem(i, 3, new QTableWidgetItem(s.os));
            table->setItem(i, 4, new QTableWidgetItem(s.screenRes));
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(s.connectTime);
            table->setItem(i, 5, new QTableWidgetItem(dt.toString("hh:mm:ss")));
        }
        table->resizeColumnsToContents();

        layout->addWidget(header);
        layout->addWidget(table, 1);

        auto* closeBtn = new QPushButton("Close", dialog);
        closeBtn->setStyleSheet(
            "QPushButton { background: #27AE60; color: white; padding: 8px 24px;"
            " border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background: #2ECC71; }");
        layout->addWidget(closeBtn, 0, Qt::AlignRight);
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);

        dialog->show();
    });

    // ── Chat button ──
    connect(m_toolbar, &ToolbarWidget::chatClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            QMessageBox::information(this, "Chat", "Please select a student first (click on a tile).");
            return;
        }

        auto* tile = selected.first();
        QString studentId = tile->studentId();
        QString studentName = tile->studentName();

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(QStringLiteral("Chat with %1").arg(studentName));
        dialog->setMinimumSize(420, 400);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(DIALOG_STYLE);

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(8);
        layout->setContentsMargins(12, 12, 12, 12);

        auto* header = new QLabel(QStringLiteral("💬 Chat with %1").arg(studentName), dialog);
        header->setStyleSheet("font-size: 13pt; font-weight: bold; color: #1B5E28; padding: 4px;");

        auto* chatLog = new QTextEdit(dialog);
        chatLog->setReadOnly(true);
        chatLog->setStyleSheet(
            "QTextEdit { background: #F0F7F2; border: 1px solid #BDC7C0;"
            " border-radius: 6px; padding: 8px; font-size: 10pt; }");

        auto* inputLayout = new QHBoxLayout();
        auto* chatInput = new QLineEdit(dialog);
        chatInput->setPlaceholderText("Type a message...");
        chatInput->setStyleSheet("padding: 8px; font-size: 10pt; border: 1px solid #BDC7C0; border-radius: 4px;");

        auto* sendBtn = new QPushButton("Send", dialog);
        sendBtn->setStyleSheet(
            "QPushButton { background: #27AE60; color: white; padding: 8px 16px;"
            " border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background: #2ECC71; }");
        sendBtn->setCursor(Qt::PointingHandCursor);

        inputLayout->addWidget(chatInput, 1);
        inputLayout->addWidget(sendBtn);

        layout->addWidget(header);
        layout->addWidget(chatLog, 1);
        layout->addLayout(inputLayout);

        // Handle incoming chats
        auto chatConn = connect(m_connManager, &ConnectionManager::chatReceived,
                dialog, [chatLog, studentId](const QString& sid, const QString& sender, const QString& msg) {
            if (sid == studentId) {
                chatLog->append(QStringLiteral("<p><b style='color:#2C3E35'>%1:</b> %2</p>")
                    .arg(sender.toHtmlEscaped(), msg.toHtmlEscaped()));
                auto* sb = chatLog->verticalScrollBar();
                sb->setValue(sb->maximum());
            }
        });

        // Send handler
        auto sendHandler = [=]() {
            QString text = chatInput->text().trimmed();
            if (text.isEmpty()) return;
            m_connManager->sendChatTo(studentId, "Teacher", text);
            chatLog->append(QStringLiteral("<p><b style='color:#1B5E28'>Teacher:</b> %1</p>")
                .arg(text.toHtmlEscaped()));
            auto* sb = chatLog->verticalScrollBar();
            sb->setValue(sb->maximum());
            chatInput->clear();
        };

        connect(sendBtn, &QPushButton::clicked, dialog, sendHandler);
        connect(chatInput, &QLineEdit::returnPressed, dialog, sendHandler);

        // Cleanup
        connect(dialog, &QDialog::destroyed, this, [chatConn]() {
            QObject::disconnect(chatConn);
        });

        dialog->show();
    });

    // ── Help Requests button ──
    connect(m_toolbar, &ToolbarWidget::helpRequestsClicked, this, [this]() {
        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Help Requests");
        dialog->setMinimumSize(500, 350);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(DIALOG_STYLE);

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(16, 16, 16, 16);

        auto* header = new QLabel("🆘 Student Help Requests", dialog);
        header->setStyleSheet("font-size: 13pt; font-weight: bold; color: #1B5E28; padding-bottom: 8px;");

        auto* helpLog = new QTextEdit(dialog);
        helpLog->setReadOnly(true);
        helpLog->setStyleSheet(
            "QTextEdit { background: #F0F7F2; border: 1px solid #BDC7C0;"
            " border-radius: 6px; padding: 8px; font-size: 10pt; }");

        if (m_helpRequests.isEmpty()) {
            helpLog->setPlaceholderText("No help requests yet.");
        } else {
            for (const auto& req : m_helpRequests) {
                helpLog->append(req);
            }
        }

        // Live updates
        auto helpConn = connect(m_connManager, &ConnectionManager::helpRequestReceived,
                dialog, [helpLog](const QString&, const QString& name, const QString& msg) {
            QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
            helpLog->append(QStringLiteral("<p><b>[%1] %2:</b> %3</p>")
                .arg(time, name.toHtmlEscaped(), msg.toHtmlEscaped()));
        });

        auto* closeBtn = new QPushButton("Close", dialog);
        closeBtn->setStyleSheet(
            "QPushButton { background: #27AE60; color: white; padding: 8px 24px;"
            " border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background: #2ECC71; }");
        layout->addWidget(header);
        layout->addWidget(helpLog, 1);
        layout->addWidget(closeBtn, 0, Qt::AlignRight);
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);

        connect(dialog, &QDialog::destroyed, this, [helpConn]() {
            QObject::disconnect(helpConn);
        });

        dialog->show();
    });

    // ── Grid connections ──
    connect(m_grid, &StudentGrid::selectionChanged,
            this, &TutorWindow::onSelectionChanged);
}

void TutorWindow::setupStatusBar()
{
    m_statusBarWidget = new QWidget(this);
    m_statusBarWidget->setObjectName("StatusBarWidget");
    m_statusBarWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_statusBarWidget->setAutoFillBackground(true);
    m_statusBarWidget->setStyleSheet(Styles::statusBarStyle());
    m_statusBarWidget->setFixedHeight(36);

    auto* layout = new QHBoxLayout(m_statusBarWidget);
    layout->setContentsMargins(12, 4, 12, 4);
    layout->setSpacing(16);

    // Update Speed slider
    m_updateSpeedLabel = new QLabel("Update Speed:", m_statusBarWidget);
    m_updateSpeedSlider = new QSlider(Qt::Horizontal, m_statusBarWidget);
    m_updateSpeedSlider->setRange(500, 5000);
    m_updateSpeedSlider->setValue(2000);
    m_updateSpeedSlider->setFixedWidth(120);
    m_updateSpeedSlider->setToolTip("Capture interval (ms)");

    layout->addWidget(m_updateSpeedLabel);
    layout->addWidget(m_updateSpeedSlider);

    // Separator
    auto* sep1 = new QFrame(m_statusBarWidget);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet(QStringLiteral("color: %1;").arg(Styles::Colors::StatusBarBorder));
    layout->addWidget(sep1);

    // Thumbnail Size slider
    m_thumbSizeLabel = new QLabel("Thumbnail Size:", m_statusBarWidget);
    m_thumbSizeSlider = new QSlider(Qt::Horizontal, m_statusBarWidget);
    m_thumbSizeSlider->setRange(150, 400);
    m_thumbSizeSlider->setValue(240);
    m_thumbSizeSlider->setFixedWidth(120);
    m_thumbSizeSlider->setToolTip("Thumbnail width (px)");

    layout->addWidget(m_thumbSizeLabel);
    layout->addWidget(m_thumbSizeSlider);

    // Spacer
    layout->addStretch();

    // Connection status
    m_statusLabel = new QLabel(m_statusBarWidget);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; color: %1;"
    ).arg(Styles::Colors::TextPrimary));
    layout->addWidget(m_statusLabel);

    // Add to main layout
    auto* centralLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    centralLayout->addWidget(m_statusBarWidget);

    // Connections
    connect(m_thumbSizeSlider, &QSlider::valueChanged,
            this, &TutorWindow::onThumbSizeChanged);

    updateStatusLabel();
}

void TutorWindow::setupShortcuts()
{
    m_selectAllShortcut = new QShortcut(QKeySequence("Ctrl+A"), this);
    connect(m_selectAllShortcut, &QShortcut::activated,
            m_grid, &StudentGrid::selectAll);

    m_deselectShortcut = new QShortcut(QKeySequence("Escape"), this);
    connect(m_deselectShortcut, &QShortcut::activated,
            m_grid, &StudentGrid::deselectAll);

    m_refreshShortcut = new QShortcut(QKeySequence("F5"), this);
    connect(m_refreshShortcut, &QShortcut::activated,
            this, &TutorWindow::onRefresh);
}

void TutorWindow::setupConnectionManager()
{
    m_connManager = new ConnectionManager(this);

    connect(m_connManager, &ConnectionManager::studentConnected,
            this, &TutorWindow::onStudentConnected);
    connect(m_connManager, &ConnectionManager::studentDisconnected,
            this, &TutorWindow::onStudentDisconnected);
    connect(m_connManager, &ConnectionManager::frameReceived,
            this, &TutorWindow::onFrameReceived);
    connect(m_connManager, &ConnectionManager::listeningStarted, this, [this](uint16_t port) {
        qInfo() << "[TutorWindow] Server listening on port" << port;
        updateStatusLabel();
    });
    connect(m_connManager, &ConnectionManager::listenError, this, [this](const QString& err) {
        QMessageBox::critical(this, "Server Error",
            "Failed to start server: " + err + "\n\n"
            "Make sure port " + QString::number(DEFAULT_PORT) + " is not in use.");
    });

    // Help request handler — store and notify
    connect(m_connManager, &ConnectionManager::helpRequestReceived,
            this, [this](const QString&, const QString& name, const QString& msg) {
        QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
        QString entry = QStringLiteral("<p><b>[%1] %2:</b> %3</p>")
            .arg(time, name.toHtmlEscaped(), msg.toHtmlEscaped());
        m_helpRequests.append(entry);
        statusBar()->showMessage(QStringLiteral("🆘 Help request from %1").arg(name), 5000);
    });

    // App status handler — update tile with active app
    connect(m_connManager, &ConnectionManager::appStatusReceived,
            this, [this](const QString& studentId, const QString& appName, const QString& appClass) {
        if (auto* tile = m_grid->tileById(studentId)) {
            tile->setActiveApp(appName, appClass);
        }
    });

    // Start listening
    m_connManager->startListening(DEFAULT_PORT);
}

void TutorWindow::onStudentConnected(const StudentInfo& info)
{
    qInfo() << "[TutorWindow] Student connected:" << info.hostname
             << "(" << info.username << ")";

    auto* tile = m_grid->addStudent(info);
    if (tile) {
        tile->setOnline(true);
    }
    updateStatusLabel();
}

void TutorWindow::onStudentDisconnected(const QString& studentId)
{
    qInfo() << "[TutorWindow] Student disconnected:" << studentId;

    // Option 1: Mark as offline (keep tile)
    m_grid->setStudentOnline(studentId, false);

    // Option 2: Remove tile entirely (uncomment if preferred)
    // m_grid->removeStudent(studentId);

    updateStatusLabel();
}

void TutorWindow::onFrameReceived(const QString& studentId, const QPixmap& frame)
{
    m_grid->updateScreenshot(studentId, frame);
}

void TutorWindow::onThumbSizeChanged(int value)
{
    m_grid->setThumbnailSize(value);
}

void TutorWindow::onSelectionChanged()
{
    updateStatusLabel();
}

void TutorWindow::onRefresh()
{
    qInfo() << "[TutorWindow] Refresh requested";
    // Force re-layout
    m_grid->update();
    updateStatusLabel();
}

void TutorWindow::updateStatusLabel()
{
    int connected = 0;
    for (auto* tile : m_grid->allTiles()) {
        if (tile->isOnline()) connected++;
    }
    int selected = m_grid->selectedTiles().size();
    int total = m_grid->studentCount();

    QString status;
    if (total == 0) {
        status = QStringLiteral("🔵 Listening on port %1 — No students connected")
                 .arg(DEFAULT_PORT);
    } else {
        status = QStringLiteral("🟢 %1 Connected").arg(connected);
        if (selected > 0) {
            status += QStringLiteral(" — %1 Selected").arg(selected);
        }
    }

    m_statusLabel->setText(status);
}

void TutorWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    m_lessonPanel->saveSettings();
    m_connManager->stopListening();
    event->accept();
}

void TutorWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
}

void TutorWindow::saveSettings()
{
    QSettings settings("LabMonitor", "TeacherConsole");
    settings.beginGroup("Window");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("thumbSize", m_thumbSizeSlider->value());
    settings.setValue("updateSpeed", m_updateSpeedSlider->value());
    settings.endGroup();
}

void TutorWindow::loadSettings()
{
    QSettings settings("LabMonitor", "TeacherConsole");
    settings.beginGroup("Window");

    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("thumbSize")) {
        m_thumbSizeSlider->setValue(settings.value("thumbSize").toInt());
    }
    if (settings.contains("updateSpeed")) {
        m_updateSpeedSlider->setValue(settings.value("updateSpeed").toInt());
    }

    settings.endGroup();
}

} // namespace LabMonitor
