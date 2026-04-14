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
#include <QPushButton>
#include <QFileDialog>
#include <QProcess>
#include <QTemporaryDir>
#include <QProgressDialog>
#include <QProgressBar>

namespace LabMonitor {

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
    setWindowTitle("SiManta - Teacher Console");
    setMinimumSize(1024, 700);
    resize(1440, 900);
    setStyleSheet(Styles::mainWindowStyle());
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    m_toolbar = new ToolbarWidget(this);
    mainLayout->addWidget(m_toolbar);
    auto* middleWidget = new QWidget(this);
    auto* middleLayout = new QHBoxLayout(middleWidget);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(0);
    m_sidebar = new SidebarWidget(this);
    middleLayout->addWidget(m_sidebar);
    m_grid = new StudentGrid(this);
    middleLayout->addWidget(m_grid, 1);

    mainLayout->addWidget(middleWidget, 1);
    m_lessonPanel = new LessonPanel(this);
    mainLayout->addWidget(m_lessonPanel);

    connect(m_toolbar, &ToolbarWidget::refreshClicked,
            this, &TutorWindow::onRefresh);
    connect(m_toolbar, &ToolbarWidget::connectClicked, this, [this]() {
        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Connection Settings");
        dialog->setMinimumWidth(440);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(12);
        layout->setContentsMargins(24, 24, 24, 24);

        auto* header = new QLabel("Server Status", dialog);
        header->setStyleSheet("font-size: 15pt; font-weight: bold; color: #58A6FF;");

        auto* infoLabel = new QLabel(dialog);
        bool listening = m_connManager->isListening();
        infoLabel->setText(QStringLiteral(
            "Status: %1\nPort: %2\nConnected Students: %3")
            .arg(listening ? "Active" : "Inactive")
            .arg(DEFAULT_PORT)
            .arg(m_connManager->connectedCount()));
        infoLabel->setStyleSheet(
            "font-size: 10pt; color: #E6EDF3; background: #161B22;"
            " border: 1px solid rgba(255,255,255,0.08);"
            " border-radius: 10px; padding: 16px;");

        auto* restartBtn = new QPushButton(listening ? "Restart Server" : "Start Server", dialog);
        restartBtn->setStyleSheet(Styles::primaryButtonStyle());
        restartBtn->setCursor(Qt::PointingHandCursor);
        restartBtn->setFixedHeight(38);

        auto* closeBtn = new QPushButton("Close", dialog);
        closeBtn->setStyleSheet(Styles::secondaryButtonStyle());
        closeBtn->setCursor(Qt::PointingHandCursor);

        auto* btnRow = new QHBoxLayout();
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        btnRow->addWidget(restartBtn);

        layout->addWidget(header);
        layout->addWidget(infoLabel, 1);
        layout->addLayout(btnRow);

        connect(restartBtn, &QPushButton::clicked, dialog, [=]() {
            m_connManager->stopListening();
            m_connManager->startListening(DEFAULT_PORT);
            infoLabel->setText(QStringLiteral(
                "Status: Active (restarted)\nPort: %1\nConnected Students: 0")
                .arg(DEFAULT_PORT));
            statusBar()->showMessage("Server restarted successfully", 3000);
        });
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);
        dialog->show();
    });
    connect(m_toolbar, &ToolbarWidget::aboutClicked, this, [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("About SiManta");
        dlg->setFixedSize(480, 340);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dlg);
        layout->setSpacing(12);
        layout->setContentsMargins(32, 28, 32, 28);
        auto* logoLabel = new QLabel(dlg);
        QPixmap logoPx = QIcon(":/icons/icons/logo.ico").pixmap(72, 72);
        logoLabel->setPixmap(logoPx);
        logoLabel->setAlignment(Qt::AlignCenter);
        logoLabel->setStyleSheet("border: none; background: transparent;");

        auto* title = new QLabel("SiManta", dlg);
        title->setStyleSheet("font-size: 22pt; font-weight: bold; color: #E6EDF3;");
        title->setAlignment(Qt::AlignCenter);

        layout->addWidget(logoLabel);

        auto* version = new QLabel("v1.0.0 - Professional Classroom Management", dlg);
        version->setStyleSheet("font-size: 10pt; color: #8B949E;");
        version->setAlignment(Qt::AlignCenter);

        auto* desc = new QLabel(dlg);
        desc->setText(
            "Real-time screen monitoring, student control,\n"
            "and classroom management for educational institutions.\n\n"
            "Architecture: C++ / Qt6 / TCP Binary Protocol\n"
            "Platform: Windows");
        desc->setStyleSheet(
            "font-size: 10pt; color: #8B949E; background: #161B22;"
            " border: 1px solid rgba(255,255,255,0.08);"
            " border-radius: 10px; padding: 16px;");
        desc->setAlignment(Qt::AlignCenter);
        desc->setWordWrap(true);

        auto* closeBtn = new QPushButton("Close", dlg);
        closeBtn->setStyleSheet(Styles::primaryButtonStyle());
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setFixedHeight(38);

        layout->addWidget(title);
        layout->addWidget(version);
        layout->addWidget(desc, 1);
        layout->addWidget(closeBtn);

        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        dlg->show();
    });
    connect(m_toolbar, &ToolbarWidget::messageClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, "Message", "No students connected.");
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Send Message to Students");
        dialog->setMinimumSize(500, 340);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(12);
        layout->setContentsMargins(24, 24, 24, 24);

        auto* headerLabel = new QLabel("Send Message", dialog);
        headerLabel->setStyleSheet("font-size: 15pt; font-weight: bold; color: #58A6FF;");
        auto* titleLabel = new QLabel("Title:", dialog);
        titleLabel->setStyleSheet("font-weight: bold; font-size: 10pt;");
        auto* titleEdit = new QLineEdit(dialog);
        titleEdit->setPlaceholderText("Message title...");
        titleEdit->setText("Message from Teacher");
        titleEdit->setStyleSheet(
            "padding: 8px 12px; font-size: 10pt; border: 1px solid rgba(255,255,255,0.1);"
            " border-radius: 8px; background: #161B22; color: #E6EDF3;");
        auto* bodyLabel = new QLabel("Message:", dialog);
        bodyLabel->setStyleSheet("font-weight: bold; font-size: 10pt;");
        auto* bodyEdit = new QTextEdit(dialog);
        bodyEdit->setPlaceholderText("Type your message here...");
        bodyEdit->setStyleSheet(
            "padding: 8px 12px; font-size: 10pt; border: 1px solid rgba(255,255,255,0.1);"
            " border-radius: 8px; background: #161B22; color: #E6EDF3;");
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
        auto* btnRow = new QHBoxLayout();
        auto* cancelBtn = new QPushButton("Cancel", dialog);
        cancelBtn->setStyleSheet(Styles::secondaryButtonStyle());
        cancelBtn->setCursor(Qt::PointingHandCursor);
        auto* sendBtn = new QPushButton("Send", dialog);
        sendBtn->setStyleSheet(Styles::primaryButtonStyle());
        sendBtn->setCursor(Qt::PointingHandCursor);
        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(sendBtn);

        layout->addWidget(headerLabel);
        layout->addWidget(titleLabel);
        layout->addWidget(titleEdit);
        layout->addWidget(bodyLabel);
        layout->addWidget(bodyEdit, 1);
        layout->addWidget(sendAllCheck);
        layout->addLayout(btnRow);

        connect(sendBtn, &QPushButton::clicked, dialog, [=]() {
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
        connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);

        dialog->show();
    });
    connect(m_toolbar, &ToolbarWidget::lockClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            m_connManager->sendLockAll();
            statusBar()->showMessage("All students locked", 3000);
        } else {
            QStringList ids;
            for (auto* t : selected) ids.append(t->studentId());
            m_connManager->sendLockScreen(ids);
            statusBar()->showMessage(QStringLiteral("%1 student(s) locked").arg(ids.size()), 3000);
        }
    });
    connect(m_toolbar, &ToolbarWidget::unlockClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            m_connManager->sendUnlockAll();
            statusBar()->showMessage("All students unlocked", 3000);
        } else {
            QStringList ids;
            for (auto* t : selected) ids.append(t->studentId());
            m_connManager->sendUnlockScreen(ids);
            statusBar()->showMessage(QStringLiteral("%1 student(s) unlocked").arg(ids.size()), 3000);
        }
    });
    connect(m_toolbar, &ToolbarWidget::blockAllClicked, this, [this]() {
        m_connManager->sendLockAll();
        statusBar()->showMessage("All students blocked", 3000);
    });
    connect(m_toolbar, &ToolbarWidget::sendUrlClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, "Send URL", "No students connected.");
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Send URL to Students");
        dialog->setMinimumWidth(500);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(12);
        layout->setContentsMargins(24, 24, 24, 24);

        auto* headerLabel = new QLabel("Send URL", dialog);
        headerLabel->setStyleSheet("font-size: 15pt; font-weight: bold; color: #58A6FF;");

        auto* label = new QLabel("Enter URL to open on student browsers:", dialog);
        label->setStyleSheet("font-weight: bold; font-size: 10pt;");

        auto* urlEdit = new QLineEdit(dialog);
        urlEdit->setPlaceholderText("https://example.com");
        urlEdit->setText("https://");
        urlEdit->setStyleSheet(
            "padding: 10px 14px; font-size: 11pt; border: 1px solid rgba(255,255,255,0.1);"
            " border-radius: 8px; background: #161B22; color: #E6EDF3;");

        auto* sendAllCheck = new QCheckBox("Send to ALL students", dialog);
        auto selectedTiles = m_grid->selectedTiles();
        if (selectedTiles.isEmpty()) {
            sendAllCheck->setChecked(true);
            sendAllCheck->setEnabled(false);
        } else {
            sendAllCheck->setChecked(false);
            sendAllCheck->setText(QStringLiteral("Send to ALL (otherwise %1 selected)").arg(selectedTiles.size()));
        }

        auto* btnRow = new QHBoxLayout();
        auto* cancelBtn = new QPushButton("Cancel", dialog);
        cancelBtn->setStyleSheet(Styles::secondaryButtonStyle());
        cancelBtn->setCursor(Qt::PointingHandCursor);
        auto* sendBtn = new QPushButton("Send", dialog);
        sendBtn->setStyleSheet(Styles::primaryButtonStyle());
        sendBtn->setCursor(Qt::PointingHandCursor);
        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(sendBtn);

        layout->addWidget(headerLabel);
        layout->addWidget(label);
        layout->addWidget(urlEdit);
        layout->addWidget(sendAllCheck);
        layout->addLayout(btnRow);

        connect(sendBtn, &QPushButton::clicked, dialog, [=]() {
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
        connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
        dialog->show();
    });
    connect(m_toolbar, &ToolbarWidget::registerClicked, this, [this]() {
        auto students = m_connManager->connectedStudents();
        if (students.isEmpty()) {
            QMessageBox::information(this, "Student Register", "No students connected.");
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Student Register");
        dialog->setMinimumSize(700, 400);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(12);

        auto* header = new QLabel(QStringLiteral("Connected Students (%1)").arg(students.size()), dialog);
        header->setStyleSheet("font-size: 14pt; font-weight: bold; color: #58A6FF; padding-bottom: 4px;");

        auto* table = new QTableWidget(students.size(), 6, dialog);
        table->setHorizontalHeaderLabels({"Hostname", "Username", "IP Address", "OS", "Resolution", "Connected"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->setAlternatingRowColors(true);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setStyleSheet(
            "QTableWidget { font-size: 9pt; alternate-background-color: #1C2128;"
            " background: #161B22; color: #E6EDF3; border: 1px solid rgba(255,255,255,0.08);"
            " border-radius: 8px; gridline-color: rgba(255,255,255,0.06); }");

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
        closeBtn->setStyleSheet(Styles::primaryButtonStyle());
        closeBtn->setCursor(Qt::PointingHandCursor);
        layout->addWidget(closeBtn, 0, Qt::AlignRight);
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);

        dialog->show();
    });
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
        dialog->setMinimumSize(460, 450);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(10);
        layout->setContentsMargins(16, 16, 16, 16);

        auto* header = new QLabel(QStringLiteral("Chat with %1").arg(studentName), dialog);
        header->setStyleSheet("font-size: 14pt; font-weight: bold; color: #58A6FF; padding: 4px;");

        auto* chatLog = new QTextEdit(dialog);
        chatLog->setReadOnly(true);
        chatLog->setStyleSheet(
            "QTextEdit { background: #161B22; color: #E6EDF3; border: 1px solid rgba(255,255,255,0.08);"
            " border-radius: 10px; padding: 12px; font-size: 10pt; }");

        auto* inputLayout = new QHBoxLayout();
        auto* chatInput = new QLineEdit(dialog);
        chatInput->setPlaceholderText("Type a message...");
        chatInput->setStyleSheet(
            "padding: 10px 14px; font-size: 10pt; border: 1px solid rgba(255,255,255,0.1);"
            " border-radius: 8px; background: #161B22; color: #E6EDF3;");

        auto* sendBtn = new QPushButton("Send", dialog);
        sendBtn->setStyleSheet(Styles::primaryButtonStyle());
        sendBtn->setCursor(Qt::PointingHandCursor);

        inputLayout->addWidget(chatInput, 1);
        inputLayout->addWidget(sendBtn);

        layout->addWidget(header);
        layout->addWidget(chatLog, 1);
        layout->addLayout(inputLayout);
        auto chatConn = connect(m_connManager, &ConnectionManager::chatReceived,
                dialog, [chatLog, studentId](const QString& sid, const QString& sender, const QString& msg) {
            if (sid == studentId) {
                chatLog->append(QStringLiteral("<p><b style='color:#8B949E'>%1:</b> <span style='color:#E6EDF3'>%2</span></p>")
                    .arg(sender.toHtmlEscaped(), msg.toHtmlEscaped()));
                auto* sb = chatLog->verticalScrollBar();
                sb->setValue(sb->maximum());
            }
        });
        auto sendHandler = [=]() {
            QString text = chatInput->text().trimmed();
            if (text.isEmpty()) return;
            m_connManager->sendChatTo(studentId, "Teacher", text);
            chatLog->append(QStringLiteral("<p><b style='color:#58A6FF'>Teacher:</b> <span style='color:#E6EDF3'>%1</span></p>")
                .arg(text.toHtmlEscaped()));
            auto* sb = chatLog->verticalScrollBar();
            sb->setValue(sb->maximum());
            chatInput->clear();
        };

        connect(sendBtn, &QPushButton::clicked, dialog, sendHandler);
        connect(chatInput, &QLineEdit::returnPressed, dialog, sendHandler);
        connect(dialog, &QDialog::destroyed, this, [chatConn]() {
            QObject::disconnect(chatConn);
        });

        dialog->show();
    });
    connect(m_toolbar, &ToolbarWidget::helpRequestsClicked, this, [this]() {
        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Help Requests");
        dialog->setMinimumSize(540, 380);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(12);

        auto* header = new QLabel("Student Help Requests", dialog);
        header->setStyleSheet("font-size: 14pt; font-weight: bold; color: #F85149; padding-bottom: 4px;");

        auto* helpLog = new QTextEdit(dialog);
        helpLog->setReadOnly(true);
        helpLog->setStyleSheet(
            "QTextEdit { background: #161B22; color: #E6EDF3; border: 1px solid rgba(255,255,255,0.08);"
            " border-radius: 10px; padding: 12px; font-size: 10pt; }");

        if (m_helpRequests.isEmpty()) {
            helpLog->setPlaceholderText("No help requests yet.");
        } else {
            for (const auto& req : m_helpRequests) {
                helpLog->append(req);
            }
        }
        auto helpConn = connect(m_connManager, &ConnectionManager::helpRequestReceived,
                dialog, [helpLog](const QString&, const QString& name, const QString& msg) {
            QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
            helpLog->append(QStringLiteral("<p><b style='color:#F85149'>[%1] %2:</b> <span style='color:#E6EDF3'>%3</span></p>")
                .arg(time, name.toHtmlEscaped(), msg.toHtmlEscaped()));
        });

        auto* closeBtn = new QPushButton("Close", dialog);
        closeBtn->setStyleSheet(Styles::primaryButtonStyle());
        closeBtn->setCursor(Qt::PointingHandCursor);
        layout->addWidget(header);
        layout->addWidget(helpLog, 1);
        layout->addWidget(closeBtn, 0, Qt::AlignRight);
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);

        connect(dialog, &QDialog::destroyed, this, [helpConn]() {
            QObject::disconnect(helpConn);
        });

        dialog->show();
    });
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
    m_statusBarWidget->setFixedHeight(38);

    auto* layout = new QHBoxLayout(m_statusBarWidget);
    layout->setContentsMargins(16, 4, 16, 4);
    layout->setSpacing(16);
    m_updateSpeedLabel = new QLabel("Update Speed:", m_statusBarWidget);
    m_updateSpeedSlider = new QSlider(Qt::Horizontal, m_statusBarWidget);
    m_updateSpeedSlider->setRange(500, 5000);
    m_updateSpeedSlider->setValue(2000);
    m_updateSpeedSlider->setFixedWidth(130);
    m_updateSpeedSlider->setToolTip("Capture interval: 2000 ms");

    layout->addWidget(m_updateSpeedLabel);
    layout->addWidget(m_updateSpeedSlider);
    auto* sep1 = new QFrame(m_statusBarWidget);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet(QStringLiteral("color: %1;").arg(Styles::Colors::StatusBarBorder));
    layout->addWidget(sep1);
    m_thumbSizeLabel = new QLabel("Thumbnail Size:", m_statusBarWidget);
    m_thumbSizeSlider = new QSlider(Qt::Horizontal, m_statusBarWidget);
    m_thumbSizeSlider->setRange(150, 400);
    m_thumbSizeSlider->setValue(240);
    m_thumbSizeSlider->setFixedWidth(130);
    m_thumbSizeSlider->setToolTip("Thumbnail width: 240 px");

    layout->addWidget(m_thumbSizeLabel);
    layout->addWidget(m_thumbSizeSlider);
    layout->addStretch();
    m_statusLabel = new QLabel(m_statusBarWidget);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; color: %1;"
    ).arg(Styles::Colors::TextPrimary));
    layout->addWidget(m_statusLabel);
    auto* centralLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    centralLayout->addWidget(m_statusBarWidget);
    connect(m_thumbSizeSlider, &QSlider::valueChanged,
            this, &TutorWindow::onThumbSizeChanged);
    connect(m_updateSpeedSlider, &QSlider::valueChanged, this, [this](int value) {
        m_updateSpeedSlider->setToolTip(QStringLiteral("Capture interval: %1 ms").arg(value));
    });
    connect(m_thumbSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_thumbSizeSlider->setToolTip(QStringLiteral("Thumbnail width: %1 px").arg(value));
    });

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
    connect(m_connManager, &ConnectionManager::helpRequestReceived,
            this, [this](const QString&, const QString& name, const QString& msg) {
        QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
        QString entry = QStringLiteral("<p><b style='color:#F85149'>[%1] %2:</b> <span style='color:#E6EDF3'>%3</span></p>")
            .arg(time, name.toHtmlEscaped(), msg.toHtmlEscaped());
        m_helpRequests.append(entry);
        statusBar()->showMessage(QStringLiteral("Help request from %1").arg(name), 5000);
    });
    connect(m_connManager, &ConnectionManager::appStatusReceived,
            this, [this](const QString& studentId, const QString& appName,
                         const QString& appClass, const QPixmap& appIcon,
                         double cpuUsage, double ramUsage) {
        if (auto* tile = m_grid->tileById(studentId)) {
            tile->setActiveApp(appName, appClass, appIcon);
            tile->setCpuRam(cpuUsage, ramUsage);
        }
    });
    connect(m_toolbar, &ToolbarWidget::sendFileClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, "Send File", "No students connected.");
            return;
        }

        QString filePath = QFileDialog::getOpenFileName(this, "Select File to Send");
        if (filePath.isEmpty()) return;

        auto selectedTiles = m_grid->selectedTiles();
        if (selectedTiles.isEmpty()) {
            m_connManager->sendFileToAll(filePath, false);
            statusBar()->showMessage(QStringLiteral("Sending file to ALL students..."), 5000);
        } else {
            QStringList ids;
            for (auto* tile : selectedTiles) ids.append(tile->studentId());
            m_connManager->sendFile(ids, filePath, false);
            statusBar()->showMessage(QStringLiteral("Sending file to %1 student(s)...").arg(ids.size()), 5000);
        }
    });

    connect(m_toolbar, &ToolbarWidget::sendFolderClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, "Send Folder", "No students connected.");
            return;
        }

        QString folderPath = QFileDialog::getExistingDirectory(this, "Select Folder to Send");
        if (folderPath.isEmpty()) return;
        QString folderName = QFileInfo(folderPath).fileName();
        QString tempZip = QDir::tempPath() + "/" + folderName + ".zip";
        QFile::remove(tempZip);

        statusBar()->showMessage(QStringLiteral("Compressing folder '%1'...").arg(folderName), 10000);
        auto* proc = new QProcess(this);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, tempZip, folderName](int exitCode, QProcess::ExitStatus) {
            proc->deleteLater();

            if (exitCode != 0) {
                QMessageBox::warning(this, "Error", "Failed to compress folder.");
                return;
            }

            auto selectedTiles = m_grid->selectedTiles();
            if (selectedTiles.isEmpty()) {
                m_connManager->sendFileToAll(tempZip, true);
                statusBar()->showMessage(QStringLiteral("Sending folder '%1' to ALL students...").arg(folderName), 5000);
            } else {
                QStringList ids;
                for (auto* tile : selectedTiles) ids.append(tile->studentId());
                m_connManager->sendFile(ids, tempZip, true);
                statusBar()->showMessage(QStringLiteral("Sending folder '%1' to %2 student(s)...").arg(folderName).arg(ids.size()), 5000);
            }
        });

        proc->start("powershell", QStringList()
            << "-NoProfile" << "-Command"
            << QStringLiteral("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force")
               .arg(folderPath, tempZip));
    });
    auto* m_transferProgress = new QProgressDialog("Sending file...", QString(), 0, 100, this);
    m_transferProgress->setWindowTitle("SiManta - File Transfer");
    m_transferProgress->setMinimumWidth(420);
    m_transferProgress->setMinimumDuration(0);
    m_transferProgress->setAutoClose(false);
    m_transferProgress->setAutoReset(false);
    m_transferProgress->setCancelButton(nullptr);
    m_transferProgress->setWindowModality(Qt::NonModal);
    m_transferProgress->close();
    auto* pbar = m_transferProgress->findChild<QProgressBar*>();
    if (pbar) {
        pbar->setStyleSheet(
            "QProgressBar { background: #21262D; border: none; border-radius: 4px; height: 10px; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #1F6FEB, stop:1 #58A6FF); border-radius: 4px; }"
        );
    }

    connect(m_connManager, &ConnectionManager::fileTransferProgress,
            this, [this, m_transferProgress](const QString& fileName, int percent) {
        if (!m_transferProgress->isVisible()) {
            m_transferProgress->show();
        }
        m_transferProgress->setLabelText(
            QStringLiteral("Sending '%1'\n%2% complete").arg(fileName).arg(percent));
        m_transferProgress->setValue(percent);
        statusBar()->showMessage(QStringLiteral("Sending '%1' ... %2%").arg(fileName).arg(percent));
    });

    connect(m_connManager, &ConnectionManager::fileTransferComplete,
            this, [this, m_transferProgress, pbar](const QString& fileName) {
        m_transferProgress->setValue(100);
        m_transferProgress->setLabelText(
            QStringLiteral("'%1' sent successfully!").arg(fileName));
        if (pbar) {
            pbar->setStyleSheet(
                "QProgressBar { background: #21262D; border: none; border-radius: 4px; height: 10px; }"
                "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 #238636, stop:1 #2EA043); border-radius: 4px; }"
            );
        }
        statusBar()->showMessage(QStringLiteral("File '%1' sent successfully!").arg(fileName), 5000);
        QTimer::singleShot(3000, m_transferProgress, [m_transferProgress, pbar]() {
            m_transferProgress->close();
            m_transferProgress->reset();
            if (pbar) {
                pbar->setStyleSheet(
                    "QProgressBar { background: #21262D; border: none; border-radius: 4px; height: 10px; }"
                    "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                    "stop:0 #1F6FEB, stop:1 #58A6FF); border-radius: 4px; }"
                );
            }
        });
    });
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
    m_grid->setStudentOnline(studentId, false);

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
        status = QStringLiteral("Listening on port %1  |  Waiting for students...")
                 .arg(DEFAULT_PORT);
    } else {
        status = QStringLiteral("%1 Online").arg(connected);
        if (selected > 0) {
            status += QStringLiteral("  |  %1 Selected").arg(selected);
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
    QSettings settings("SiManta", "TeacherConsole");
    settings.beginGroup("Window");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("thumbSize", m_thumbSizeSlider->value());
    settings.setValue("updateSpeed", m_updateSpeedSlider->value());
    settings.endGroup();
}

void TutorWindow::loadSettings()
{
    QSettings settings("SiManta", "TeacherConsole");
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

}
