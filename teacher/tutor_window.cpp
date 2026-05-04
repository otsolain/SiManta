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
#include <QComboBox>
#include <QFileDialog>
#include <QProcess>
#include <QTemporaryDir>
#include <QProgressDialog>
#include <QProgressBar>
#include <QTreeWidget>
#include <QInputDialog>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QFrame>
#include <QTime>
#include <QSystemTrayIcon>
#include <QGraphicsDropShadowEffect>
#include "../common/lang.h"

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
    setWindowTitle("Simanta - Teacher Console");
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
    
    m_stackedWidget = new QStackedWidget(this);
    m_grid = new StudentGrid(this);
    setupInternetTab();
    setupSecurityTab();
    setupSettingsTab();
    
    m_stackedWidget->addWidget(m_grid);
    m_stackedWidget->addWidget(m_internetWidget);
    m_stackedWidget->addWidget(m_securityWidget);
    m_stackedWidget->addWidget(m_settingsWidget);
    
    middleLayout->addWidget(m_stackedWidget, 1);

    mainLayout->addWidget(middleWidget, 1);

    m_trayIcon = new QSystemTrayIcon(this);
    QIcon appIcon = QIcon("installer/logo.png"); // or generate a fallback icon
    if (!appIcon.isNull()) {
        m_trayIcon->setIcon(appIcon);
    } else {
        QPixmap px(32, 32);
        px.fill(QColor("#1A73E8"));
        m_trayIcon->setIcon(QIcon(px));
    }
    m_trayIcon->show();

    // Sidebar view changed -> Switch stacked widget index
    connect(m_sidebar, &SidebarWidget::viewChanged, m_stackedWidget, &QStackedWidget::setCurrentIndex);

    connect(m_toolbar, &ToolbarWidget::refreshClicked,
            this, &TutorWindow::onRefresh);

    connect(m_toolbar, &ToolbarWidget::broadcastClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, Lang::get().t("Broadcast", "Siaran"), Lang::get().t("No students connected.", "Tidak ada siswa yang terhubung."));
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(Lang::get().t("Broadcast Message", "Pesan Siaran"));
        dialog->setMinimumSize(540, 420);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(16);
        layout->setContentsMargins(32, 32, 32, 24);

        auto* headerLabel = new QLabel(Lang::get().t("Broadcast Message", "Pesan Siaran"), dialog);
        headerLabel->setStyleSheet("font-size: 18pt; font-weight: bold; color: #1A2233;");
        auto* infoLabel = new QLabel(Lang::get().t(QStringLiteral("This message will be sent to ALL %1 connected students.").arg(m_grid->studentCount()), QStringLiteral("Pesan ini akan dikirim ke SEMUA %1 siswa yang terhubung.").arg(m_grid->studentCount())), dialog);
        infoLabel->setStyleSheet("font-size: 10pt; color: #5A6A7E; padding-bottom: 8px;");

        auto* titleLabel = new QLabel(Lang::get().t("Title", "Judul"), dialog);
        titleLabel->setStyleSheet("font-weight: bold; font-size: 10pt; color: #2A3B52;");
        auto* titleEdit = new QLineEdit(dialog);
        titleEdit->setPlaceholderText(Lang::get().t("e.g., Important Announcement", "cth., Pengumuman Penting"));
        titleEdit->setText(Lang::get().t("Message from Teacher", "Pesan dari Guru"));
        titleEdit->setStyleSheet(
            "QLineEdit { padding: 10px 14px; font-size: 11pt; border: 1px solid #C4D2E0;"
            " border-radius: 6px; background: #FFFFFF; color: #1A2233; }"
            "QLineEdit:focus { border: 2px solid #1A73E8; padding: 9px 13px; }"
        );

        auto* bodyLabel = new QLabel(Lang::get().t("Message", "Pesan"), dialog);
        bodyLabel->setStyleSheet("font-weight: bold; font-size: 10pt; color: #2A3B52; margin-top: 8px;");
        auto* bodyEdit = new QTextEdit(dialog);
        bodyEdit->setPlaceholderText(Lang::get().t("Type your message here...", "Ketik pesan Anda di sini..."));
        bodyEdit->setStyleSheet(
            "QTextEdit { padding: 12px; font-size: 11pt; border: 1px solid #C4D2E0;"
            " border-radius: 6px; background: #FFFFFF; color: #1A2233; }"
            "QTextEdit:focus { border: 2px solid #1A73E8; padding: 11px; }"
        );

        auto* btnRow = new QHBoxLayout();
        btnRow->setContentsMargins(0, 12, 0, 0);
        btnRow->setSpacing(12);

        auto* cancelBtn = new QPushButton(Lang::get().t("Cancel", "Batal"), dialog);
        cancelBtn->setStyleSheet(Styles::secondaryButtonStyle());
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setFixedSize(110, 40);

        auto* sendBtn = new QPushButton(Lang::get().t("Broadcast", "Siaran"), dialog);
        sendBtn->setStyleSheet(Styles::primaryButtonStyle());
        sendBtn->setCursor(Qt::PointingHandCursor);
        sendBtn->setFixedSize(140, 40);

        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(sendBtn);

        layout->addWidget(headerLabel);
        layout->addWidget(infoLabel);
        layout->addWidget(titleLabel);
        layout->addWidget(titleEdit);
        layout->addWidget(bodyLabel);
        layout->addWidget(bodyEdit, 1);
        layout->addLayout(btnRow);

        connect(sendBtn, &QPushButton::clicked, dialog, [=]() {
            QString title = titleEdit->text().trimmed();
            QString body = bodyEdit->toPlainText().trimmed();

            if (body.isEmpty()) {
                QMessageBox::warning(dialog, Lang::get().t("Empty Message", "Pesan Kosong"), Lang::get().t("Please enter a message.", "Silakan masukkan pesan."));
                return;
            }

            m_connManager->sendMessageToAll(title, body, "Teacher");
            qInfo() << "[TutorWindow] Broadcast sent to ALL students";
            statusBar()->showMessage(Lang::get().t("Broadcast sent to all students", "Siaran terkirim ke semua siswa"), 3000);
            dialog->accept();
        });
        connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);

        dialog->show();
    });
    connect(m_toolbar, &ToolbarWidget::lockClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            m_connManager->sendLockAll();
            statusBar()->showMessage(Lang::get().t("All students locked", "Semua siswa dikunci"), 3000);
        } else {
            QStringList ids;
            for (auto* t : selected) ids.append(t->studentId());
            m_connManager->sendLockScreen(ids);
            statusBar()->showMessage(Lang::get().t(QStringLiteral("%1 student(s) locked").arg(ids.size()), QStringLiteral("%1 siswa dikunci").arg(ids.size())), 3000);
        }
    });
    connect(m_toolbar, &ToolbarWidget::unlockClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            m_connManager->sendUnlockAll();
            statusBar()->showMessage(Lang::get().t("All students unlocked", "Semua siswa dibuka kuncinya"), 3000);
        } else {
            QStringList ids;
            for (auto* t : selected) ids.append(t->studentId());
            m_connManager->sendUnlockScreen(ids);
            statusBar()->showMessage(Lang::get().t(QStringLiteral("%1 student(s) unlocked").arg(ids.size()), QStringLiteral("%1 siswa dibuka kuncinya").arg(ids.size())), 3000);
        }
    });
    // Disconnect button — teacher can disconnect selected students
    connect(m_toolbar, &ToolbarWidget::disconnectClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            QMessageBox::information(this, Lang::get().t("Disconnect", "Putuskan"), Lang::get().t("Please select student(s) to disconnect first.", "Silakan pilih siswa terlebih dahulu."));
            return;
        }

        int count = selected.size();
        auto answer = QMessageBox::question(this, Lang::get().t("Disconnect Students", "Putuskan Siswa"),
            Lang::get().t(QStringLiteral("Are you sure you want to disconnect %1 student(s)?").arg(count), QStringLiteral("Apakah Anda yakin ingin memutuskan %1 siswa?").arg(count)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (answer != QMessageBox::Yes) return;

        for (auto* tile : selected) {
            m_connManager->disconnectStudent(tile->studentId());
        }
        statusBar()->showMessage(Lang::get().t(QStringLiteral("%1 student(s) disconnected").arg(count), QStringLiteral("%1 siswa diputuskan").arg(count)), 3000);
    });
    connect(m_toolbar, &ToolbarWidget::sendUrlClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, Lang::get().t("Send URL", "Kirim URL"), Lang::get().t("No students connected.", "Tidak ada siswa yang terhubung."));
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(Lang::get().t("Send URL to Students", "Kirim URL ke Siswa"));
        dialog->setMinimumWidth(500);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(12);
        layout->setContentsMargins(24, 24, 24, 24);

        auto* headerLabel = new QLabel(Lang::get().t("Send URL", "Kirim URL"), dialog);
        headerLabel->setStyleSheet("font-size: 15pt; font-weight: bold; color: #1A73E8;");

        auto* label = new QLabel(Lang::get().t("Enter URL to open on student browsers:", "Masukkan URL untuk dibuka di browser siswa:"), dialog);
        label->setStyleSheet("font-weight: bold; font-size: 10pt;");

        auto* urlEdit = new QLineEdit(dialog);
        urlEdit->setPlaceholderText("https://example.com");
        urlEdit->setText("https://");
        urlEdit->setStyleSheet(
            "padding: 10px 14px; font-size: 11pt; border: 1px solid rgba(0,60,120,0.12);"
            " border-radius: 8px; background: #FFFFFF; color: #1A2233;");

        auto* sendAllCheck = new QCheckBox(Lang::get().t("Send to ALL students", "Kirim ke SEMUA siswa"), dialog);
        auto selectedTiles = m_grid->selectedTiles();
        if (selectedTiles.isEmpty()) {
            sendAllCheck->setChecked(true);
            sendAllCheck->setEnabled(false);
        } else {
            sendAllCheck->setChecked(false);
            sendAllCheck->setText(Lang::get().t(QStringLiteral("Send to ALL (otherwise %1 selected)").arg(selectedTiles.size()), QStringLiteral("Kirim ke SEMUA (jika tidak, %1 terpilih)").arg(selectedTiles.size())));
        }

        auto* btnRow = new QHBoxLayout();
        auto* cancelBtn = new QPushButton(Lang::get().t("Cancel", "Batal"), dialog);
        cancelBtn->setStyleSheet(Styles::secondaryButtonStyle());
        cancelBtn->setCursor(Qt::PointingHandCursor);
        auto* sendBtn = new QPushButton(Lang::get().t("Send", "Kirim"), dialog);
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
                QMessageBox::warning(dialog, Lang::get().t("Invalid URL", "URL Tidak Valid"), Lang::get().t("Please enter a valid URL.", "Silakan masukkan URL yang valid."));
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
            QMessageBox::information(this, Lang::get().t("Student Register", "Daftar Siswa"), Lang::get().t("No students connected.", "Tidak ada siswa yang terhubung."));
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(Lang::get().t("Student Register", "Daftar Siswa"));
        dialog->setMinimumSize(780, 450);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle() + 
                              "QHeaderView::section { background-color: #F0F4F8; color: #5A6A7E; font-weight: bold; border: none; padding: 6px; border-bottom: 1px solid #DCE3EB; }");

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(28, 28, 28, 28);
        layout->setSpacing(16);

        auto* header = new QLabel(Lang::get().t(QStringLiteral("Connected Students (%1)").arg(students.size()), QStringLiteral("Siswa Terhubung (%1)").arg(students.size())), dialog);
        header->setStyleSheet("font-size: 16pt; font-weight: bold; color: #1A2233; padding-bottom: 8px;");

        auto* table = new QTableWidget(students.size(), 6, dialog);
        table->setHorizontalHeaderLabels({
            Lang::get().t("Hostname", "Nama Host"), Lang::get().t("Username", "Nama Pengguna"),
            Lang::get().t("IP Address", "Alamat IP"), Lang::get().t("OS", "OS"),
            Lang::get().t("Resolution", "Resolusi"), Lang::get().t("Connected", "Terhubung")
        });
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false); // Hide the vertical header with black background
        table->setAlternatingRowColors(true);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setShowGrid(false);
        table->setFocusPolicy(Qt::NoFocus);
        table->setStyleSheet(
            "QTableWidget { font-size: 10pt; alternate-background-color: #FAFCFF;"
            " background: #FFFFFF; color: #2A3B52; border: 1px solid #DCE3EB;"
            " border-radius: 6px; }"
            "QTableWidget::item { border-bottom: 1px solid #F0F4F8; padding-left: 8px; }"
            "QTableWidget::item:selected { background-color: #E8F0FE; color: #1A73E8; }");

        for (int i = 0; i < students.size(); ++i) {
            const auto& s = students[i];
            
            // Clean up IPv4 mapped addresses (e.g., ::ffff:192.168.1.8 -> 192.168.1.8)
            QString cleanIp = s.ipAddress;
            if (cleanIp.startsWith("::ffff:")) {
                cleanIp = cleanIp.mid(7);
            }
            
            table->setItem(i, 0, new QTableWidgetItem(s.hostname));
            table->setItem(i, 1, new QTableWidgetItem(s.username));
            table->setItem(i, 2, new QTableWidgetItem(cleanIp));
            table->setItem(i, 3, new QTableWidgetItem(s.os));
            table->setItem(i, 4, new QTableWidgetItem(s.screenRes));
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(s.connectTime);
            table->setItem(i, 5, new QTableWidgetItem(dt.toString("hh:mm:ss")));
            
            // Make items align nicely
            for(int j=0; j<6; j++) {
                if(table->item(i, j)) {
                    table->item(i, j)->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
                }
            }
        }
        table->resizeColumnsToContents();
        table->horizontalHeader()->setMinimumSectionSize(100);

        layout->addWidget(header);
        layout->addWidget(table, 1);

        auto* btnLayout = new QHBoxLayout();
        auto* closeBtn = new QPushButton(Lang::get().t("Close", "Tutup"), dialog);
        closeBtn->setStyleSheet(Styles::primaryButtonStyle());
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setFixedSize(100, 36);
        btnLayout->addStretch();
        btnLayout->addWidget(closeBtn);
        layout->addLayout(btnLayout);

        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);

        dialog->show();
    });
    connect(m_toolbar, &ToolbarWidget::chatClicked, this, [this]() {
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) {
            QMessageBox::information(this, Lang::get().t("Chat", "Obrolan"), Lang::get().t("Please select a student first (click on a tile).", "Silakan pilih siswa terlebih dahulu (klik pada kartu)."));
            return;
        }

        auto* tile = selected.first();
        QString studentId = tile->studentId();
        QString studentName = tile->studentName();

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(Lang::get().t(QStringLiteral("Chat - %1").arg(studentName), QStringLiteral("Obrolan - %1").arg(studentName)));
        dialog->setMinimumSize(480, 560);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet("QDialog { background: #F8FAFC; }");

        auto* layout = new QVBoxLayout(dialog);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);

        // ── Header bar (Modern Clean) ──
        auto* headerBar = new QWidget(dialog);
        headerBar->setFixedHeight(68);
        headerBar->setStyleSheet("background: #FFFFFF; border-bottom: 1px solid #E2E8F0;");
        auto* hdrLayout = new QHBoxLayout(headerBar);
        hdrLayout->setContentsMargins(20, 0, 20, 0);
        hdrLayout->setSpacing(14);

        auto* avatar = new QLabel(headerBar);
        avatar->setFixedSize(42, 42);
        QString initials = studentName.left(1).toUpper();
        if (studentName.contains(' '))
            initials += studentName.mid(studentName.indexOf(' ') + 1, 1).toUpper();
        avatar->setText(initials);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3B82F6, stop:1 #2563EB);"
            " color: white; font-weight: bold; font-size: 14pt; border-radius: 21px; border: none;");

        auto* nameCol = new QVBoxLayout();
        nameCol->setSpacing(2);
        nameCol->setAlignment(Qt::AlignVCenter);
        auto* nameLabel = new QLabel(studentName, headerBar);
        nameLabel->setStyleSheet("color: #0F172A; font-size: 13pt; font-weight: 600; background: transparent; border: none;");
        auto* statusLabel = new QLabel("Online", headerBar);
        statusLabel->setStyleSheet("color: #10B981; font-size: 9pt; font-weight: 500; background: transparent; border: none;");
        nameCol->addWidget(nameLabel);
        nameCol->addWidget(statusLabel);

        hdrLayout->addWidget(avatar);
        hdrLayout->addLayout(nameCol, 1);

        // ── Chat area (QScrollArea) ──
        auto* chatScroll = new QScrollArea(dialog);
        chatScroll->setWidgetResizable(true);
        chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        chatScroll->setStyleSheet(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollBar:vertical { width: 6px; background: transparent; }"
            "QScrollBar::handle:vertical { background: rgba(15,23,42,0.15); border-radius: 3px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
        auto* chatContainer = new QWidget();
        chatContainer->setStyleSheet("background: transparent;");
        auto* chatLayout = new QVBoxLayout(chatContainer);
        chatLayout->setContentsMargins(16, 16, 16, 16);
        chatLayout->setSpacing(12);
        chatLayout->addStretch();
        chatScroll->setWidget(chatContainer);

        // ── Input area ──
        auto* inputBar = new QWidget(dialog);
        inputBar->setFixedHeight(72);
        inputBar->setStyleSheet("background: #FFFFFF; border-top: 1px solid #E2E8F0;");
        auto* inputLayout = new QHBoxLayout(inputBar);
        inputLayout->setContentsMargins(16, 12, 16, 12);
        inputLayout->setSpacing(12);

        auto* chatInput = new QLineEdit(inputBar);
        chatInput->setPlaceholderText(Lang::get().t("Type a message...", "Ketik pesan..."));
        chatInput->setStyleSheet(
            "QLineEdit { padding: 10px 16px; font-size: 10pt; color: #0F172A;"
            " background: #F1F5F9; border: 1px solid transparent;"
            " border-radius: 20px; }"
            "QLineEdit:focus { border: 1px solid #3B82F6; background: #FFFFFF; }");

        auto* sendBtn = new QPushButton(Lang::get().t("Send", "Kirim"), inputBar);
        sendBtn->setFixedSize(76, 40);
        sendBtn->setStyleSheet(
            "QPushButton { background: #3B82F6; color: white; border-radius: 20px;"
            " font-weight: 600; font-size: 10pt; border: none; }"
            "QPushButton:hover { background: #2563EB; }");
        sendBtn->setCursor(Qt::PointingHandCursor);

        inputLayout->addWidget(chatInput, 1);
        inputLayout->addWidget(sendBtn);

        layout->addWidget(headerBar);
        layout->addWidget(chatScroll, 1);
        layout->addWidget(inputBar);

        // ── Modern bubble creator ──
        auto addBubbleWidget = [chatLayout, chatScroll](const QString& text,
                bool isTeacher, const QString& sender, const QString& timeStr) {
            auto* row = new QHBoxLayout();
            row->setContentsMargins(0, 0, 0, 0);

            auto* bubble = new QFrame();
            bubble->setMaximumWidth(340);
            bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

            auto* effect = new QGraphicsDropShadowEffect(bubble);
            effect->setBlurRadius(8);
            effect->setOffset(0, 2);
            effect->setColor(QColor(0, 0, 0, 10));
            bubble->setGraphicsEffect(effect);

            auto* bLayout = new QVBoxLayout(bubble);
            bLayout->setContentsMargins(14, 10, 14, 10);
            bLayout->setSpacing(4);

            auto* msgLbl = new QLabel(text, bubble);
            msgLbl->setWordWrap(true);
            msgLbl->setStyleSheet(QStringLiteral(
                "font-size: 10pt; color: %1; background: transparent; border: none;")
                .arg(isTeacher ? "#FFFFFF" : "#1E293B"));

            auto* timeLbl = new QLabel(timeStr, bubble);
            timeLbl->setAlignment(Qt::AlignRight);
            timeLbl->setStyleSheet(QStringLiteral(
                "font-size: 8pt; color: %1; background: transparent; border: none;")
                .arg(isTeacher ? "rgba(255,255,255,0.7)" : "#94A3B8"));

            bLayout->addWidget(msgLbl);
            bLayout->addWidget(timeLbl);

            if (isTeacher) {
                bubble->setStyleSheet(
                    "QFrame { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3B82F6, stop:1 #2563EB);"
                    " border-radius: 16px; border-bottom-right-radius: 4px; border: none; }");
                row->addStretch();
                row->addWidget(bubble);
            } else {
                bubble->setStyleSheet(
                    "QFrame { background: #FFFFFF; border: 1px solid #E2E8F0;"
                    " border-radius: 16px; border-bottom-left-radius: 4px; }");
                row->addWidget(bubble);
                row->addStretch();
            }

            // Insert before the stretch at the end
            chatLayout->insertLayout(chatLayout->count() - 1, row);

            QTimer::singleShot(20, chatScroll, [chatScroll]() {
                chatScroll->verticalScrollBar()->setValue(
                    chatScroll->verticalScrollBar()->maximum());
            });
        };

        // ── Restore chat history for this student ──
        if (m_chatHistory.contains(studentId)) {
            for (const auto& entry : m_chatHistory[studentId]) {
                addBubbleWidget(entry.message, entry.isTeacher, entry.sender, entry.time);
            }
        }

        // ── Receive handler ──
        auto chatConn = connect(m_connManager, &ConnectionManager::chatReceived,
                dialog, [this, addBubbleWidget, studentId](const QString& sid,
                    const QString& sender, const QString& msg) {
            if (sid == studentId) {
                QString time = QTime::currentTime().toString("HH:mm");
                addBubbleWidget(msg, false, sender, time);
                // Save to history
                ChatEntry entry{sender, msg, time, false};
                m_chatHistory[studentId].append(entry);
            }
        });

        // ── Send handler ──
        auto sendHandler = [this, chatInput, studentId, addBubbleWidget]() {
            QString text = chatInput->text().trimmed();
            if (text.isEmpty()) return;
            m_connManager->sendChatTo(studentId, "Teacher", text);
            QString time = QTime::currentTime().toString("HH:mm");
            addBubbleWidget(text, true, "You", time);
            // Save to history
            ChatEntry entry{"You", text, time, true};
            m_chatHistory[studentId].append(entry);
            chatInput->clear();
        };

        connect(sendBtn, &QPushButton::clicked, dialog, sendHandler);
        connect(chatInput, &QLineEdit::returnPressed, dialog, sendHandler);
        connect(dialog, &QDialog::destroyed, this, [chatConn]() {
            QObject::disconnect(chatConn);
        });

        dialog->show();
        chatInput->setFocus();
    });

    connect(m_toolbar, &ToolbarWidget::helpRequestsClicked, this, [this]() {
        // Reset unread count
        m_unreadHelpCount = 0;
        m_toolbar->setUnreadHelpCount(0);

        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(Lang::get().t("Help Request", "Permintaan Bantuan"));
        dialog->setMinimumSize(540, 480);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(16);

        auto* header = new QLabel(Lang::get().t("Student Help Requests", "Permintaan Bantuan Siswa"), dialog);
        header->setStyleSheet("font-size: 16pt; font-weight: bold; color: #1E293B;");

        auto* scrollArea = new QScrollArea(dialog);
        scrollArea->setWidgetResizable(true);
        scrollArea->setStyleSheet("QScrollArea { border: none; background: #F8FAFC; border-radius: 12px; }");
        
        auto* scrollContainer = new QWidget(scrollArea);
        scrollContainer->setStyleSheet("background: transparent;");
        auto* listLayout = new QVBoxLayout(scrollContainer);
        listLayout->setContentsMargins(12, 12, 12, 12);
        listLayout->setSpacing(12);

        auto addCard = [listLayout](const QString& reqHtml) {
            auto* card = new QFrame();
            card->setStyleSheet(
                "QFrame { background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 8px; }"
                "QFrame:hover { border-color: #CBD5E1; }"
            );
            auto* cLayout = new QVBoxLayout(card);
            cLayout->setContentsMargins(16, 12, 16, 12);
            auto* lbl = new QLabel(reqHtml);
            lbl->setWordWrap(true);
            lbl->setTextFormat(Qt::RichText);
            lbl->setStyleSheet("QLabel { background: transparent; border: none; }");
            cLayout->addWidget(lbl);
            listLayout->insertWidget(0, card); // Add to top (newest first)
        };

        if (m_helpRequests.isEmpty()) {
            auto* emptyLbl = new QLabel(Lang::get().t("No help requests yet.", "Belum ada permintaan bantuan."));
            emptyLbl->setAlignment(Qt::AlignCenter);
            emptyLbl->setStyleSheet("color: #94A3B8; font-size: 11pt; font-style: italic; background: transparent;");
            listLayout->addWidget(emptyLbl);
        } else {
            for (const auto& req : m_helpRequests) {
                addCard(req);
            }
        }
        listLayout->addStretch();
        scrollArea->setWidget(scrollContainer);

        auto helpConn = connect(m_connManager, &ConnectionManager::helpRequestReceived,
                dialog, [addCard, listLayout](const QString&, const QString& name, const QString& msg) {
            // Remove the "empty" label if it exists
            if (listLayout->count() > 1 && qobject_cast<QLabel*>(listLayout->itemAt(listLayout->count() - 2)->widget())) {
                auto* w = listLayout->itemAt(listLayout->count() - 2)->widget();
                listLayout->removeWidget(w);
                w->deleteLater();
            }
            QString time = QDateTime::currentDateTime().toString("HH:mm");
            QString html = QStringLiteral("<b style='color:#F59E0B; font-size:11pt;'>%1</b> <span style='color:#94A3B8; font-size:9pt;'>— %2</span><br><br><span style='font-size:10pt; color:#334155;'>%3</span>")
                .arg(name.toHtmlEscaped(), time, msg.toHtmlEscaped());
            addCard(html);
        });

        auto* btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto* closeBtn = new QPushButton(Lang::get().t("Close", "Tutup"), dialog);
        closeBtn->setStyleSheet(Styles::primaryButtonStyle());
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setMinimumWidth(100);
        btnRow->addWidget(closeBtn);

        layout->addWidget(header);
        layout->addWidget(scrollArea, 1);
        layout->addLayout(btnRow);

        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);

        connect(dialog, &QDialog::destroyed, this, [helpConn]() {
            QObject::disconnect(helpConn);
        });

        dialog->show();
    });
    connect(m_grid, &StudentGrid::selectionChanged,
            this, &TutorWindow::onSelectionChanged);

    // ── Veyon-like: switch student to high-res mode when fullscreen view opens ──
    connect(m_grid, &StudentGrid::fullscreenOpened, this, [this](const QString& studentId) {
        m_connManager->sendQualityHigh(studentId);
        qInfo() << "[TutorWindow] Fullscreen opened for" << studentId << "— switching to high-res mode";
    });
    connect(m_grid, &StudentGrid::fullscreenClosed, this, [this](const QString& studentId) {
        m_connManager->sendQualityNormal(studentId);
        qInfo() << "[TutorWindow] Fullscreen closed for" << studentId << "— switching to normal mode";
    });
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
    m_updateSpeedLabel = new QLabel(Lang::get().t("Update Speed:", "Kecepatan Pembaruan:"), m_statusBarWidget);
    m_updateSpeedLabel->setFixedWidth(140);
    m_updateSpeedSlider = new QSlider(Qt::Horizontal, m_statusBarWidget);
    m_updateSpeedSlider->setRange(5, 50);
    m_updateSpeedSlider->setValue(35); // 5500 - 3500 = 2000 ms
    m_updateSpeedSlider->setFixedWidth(130);
    m_updateSpeedSlider->setToolTip("Capture interval: 2000 ms");

    layout->addWidget(m_updateSpeedLabel);
    layout->addWidget(m_updateSpeedSlider);
    auto* sep1 = new QFrame(m_statusBarWidget);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet(QStringLiteral("color: %1;").arg(Styles::Colors::StatusBarBorder));
    layout->addWidget(sep1);
    m_thumbSizeLabel = new QLabel(Lang::get().t("Thumbnail Size:", "Ukuran Thumbnail:"), m_statusBarWidget);
    m_thumbSizeLabel->setFixedWidth(130);
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
    ).arg(Styles::Colors::StatusBarText));
    layout->addWidget(m_statusLabel);
    auto* centralLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    centralLayout->addWidget(m_statusBarWidget);
    connect(m_thumbSizeSlider, &QSlider::valueChanged,
            this, &TutorWindow::onThumbSizeChanged);
    connect(m_updateSpeedSlider, &QSlider::valueChanged, this, [this](int value) {
        int ms = 5500 - (value * 100);
        m_updateSpeedSlider->setToolTip(QStringLiteral("Capture interval: %1 ms").arg(ms));
    });
    connect(m_updateSpeedSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_connManager) {
            int ms = 5500 - (m_updateSpeedSlider->value() * 100);
            m_connManager->broadcastUpdateSpeed(ms);
        }
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
        QString time = QDateTime::currentDateTime().toString("HH:mm");
        QString entry = QStringLiteral("<b style='color:#F59E0B; font-size:11pt;'>%1</b> <span style='color:#94A3B8; font-size:9pt;'>— %2</span><br><br><span style='font-size:10pt; color:#334155;'>%3</span>")
            .arg(name.toHtmlEscaped(), time, msg.toHtmlEscaped());
        m_helpRequests.append(entry);
        
        m_unreadHelpCount++;
        m_toolbar->setUnreadHelpCount(m_unreadHelpCount);

        // Show standard OS tray notification instead of a floating popup
        if (m_trayIcon) {
            m_trayIcon->showMessage(QStringLiteral("Request: %1").arg(name),
                                    msg, QSystemTrayIcon::Information, 5000);
        }
    });
    // ── Global chat notification ──
    connect(m_connManager, &ConnectionManager::chatReceived,
            this, [this](const QString& studentId, const QString& sender, const QString& message) {
        // Save to chat history so it persists across dialog open/close
        QString time = QTime::currentTime().toString("HH:mm");
        ChatEntry entry{sender, message, time, false};
        m_chatHistory[studentId].append(entry);

        if (m_trayIcon) {
            m_trayIcon->showMessage(QStringLiteral("Chat dari %1").arg(sender),
                                    message, QSystemTrayIcon::Information, 5000);
        }
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
            QMessageBox::information(this, Lang::get().t("Send File", "Kirim File"), Lang::get().t("No students connected.", "Tidak ada siswa yang terhubung."));
            return;
        }

        QString filePath = QFileDialog::getOpenFileName(this, Lang::get().t("Select File to Send", "Pilih File untuk Dikirim"));
        if (filePath.isEmpty()) return;

        // Ask for destination path on student PC
        bool ok;
        QString destPath = QInputDialog::getText(this, Lang::get().t("Destination Path", "Lokasi Tujuan"),
            Lang::get().t("Where to save on student PC?\n(Leave blank for default: Downloads/Simanta)", "Simpan di mana pada PC siswa?\n(Kosongkan untuk default: Downloads/Simanta)"),
            QLineEdit::Normal, "", &ok);
        if (!ok) return;  // cancelled

        auto selectedTiles = m_grid->selectedTiles();
        if (selectedTiles.isEmpty()) {
            QMessageBox::warning(this, Lang::get().t("Send File", "Kirim File"), 
                Lang::get().t("Please select at least one student first.\n(Press Ctrl+A to select all)", "Silakan pilih setidaknya satu siswa terlebih dahulu.\n(Tekan Ctrl+A untuk memilih semua)"));
            return;
        }
        
        QStringList ids;
        for (auto* tile : selectedTiles) ids.append(tile->studentId());
        m_connManager->sendFile(ids, filePath, false, destPath);
        statusBar()->showMessage(Lang::get().t(QStringLiteral("Sending file to %1 student(s)...").arg(ids.size()), QStringLiteral("Mengirim file ke %1 siswa...").arg(ids.size())), 5000);
    });

    connect(m_toolbar, &ToolbarWidget::sendFolderClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, Lang::get().t("Send Folder", "Kirim Folder"), Lang::get().t("No students connected.", "Tidak ada siswa yang terhubung."));
            return;
        }

        QString folderPath = QFileDialog::getExistingDirectory(this, Lang::get().t("Select Folder to Send", "Pilih Folder untuk Dikirim"));
        if (folderPath.isEmpty()) return;

        // Ask for destination path on student PC
        bool ok;
        QString destPath = QInputDialog::getText(this, Lang::get().t("Destination Path", "Lokasi Tujuan"),
            Lang::get().t("Where to save on student PC?\n(Leave blank for default: Downloads/Simanta)", "Simpan di mana pada PC siswa?\n(Kosongkan untuk default: Downloads/Simanta)"),
            QLineEdit::Normal, "", &ok);
        if (!ok) return;

        QString folderName = QFileInfo(folderPath).fileName();
        QString tempZip = QDir::tempPath() + "/" + folderName + ".zip";
        QFile::remove(tempZip);

        statusBar()->showMessage(QStringLiteral("Compressing folder '%1'...").arg(folderName), 10000);
        auto* proc = new QProcess(this);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, tempZip, folderName, destPath](int exitCode, QProcess::ExitStatus) {
            proc->deleteLater();

            if (exitCode != 0) {
                QMessageBox::warning(this, Lang::get().t("Error", "Kesalahan"), Lang::get().t("Failed to compress folder.", "Gagal mengompres folder."));
                return;
            }

            auto selectedTiles = m_grid->selectedTiles();
            if (selectedTiles.isEmpty()) {
                QMessageBox::warning(this, Lang::get().t("Send Folder", "Kirim Folder"), 
                    Lang::get().t("Please select at least one student first.\n(Press Ctrl+A to select all)", "Silakan pilih setidaknya satu siswa terlebih dahulu.\n(Tekan Ctrl+A untuk memilih semua)"));
                return;
            }
            
            QStringList ids;
            for (auto* tile : selectedTiles) ids.append(tile->studentId());
            m_connManager->sendFile(ids, tempZip, true, destPath);
            statusBar()->showMessage(QStringLiteral("Sending folder '%1' to %2 student(s)...").arg(folderName).arg(ids.size()), 5000);
        });

        proc->start("powershell", QStringList()
            << "-NoProfile" << "-Command"
            << QStringLiteral("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force")
               .arg(folderPath, tempZip));
    });
    auto* m_transferProgress = new QProgressDialog("Sending file...", QString(), 0, 100, this);
    m_transferProgress->setWindowTitle("Simanta - File Transfer");
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
            "QProgressBar { background: #E8EEF4; border: none; border-radius: 4px; height: 10px; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #1A73E8, stop:1 #1A73E8); border-radius: 4px; }"
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
                "QProgressBar { background: #E8EEF4; border: none; border-radius: 4px; height: 10px; }"
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
                    "QProgressBar { background: #E8EEF4; border: none; border-radius: 4px; height: 10px; }"
                    "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                    "stop:0 #1A73E8, stop:1 #1A73E8); border-radius: 4px; }"
                );
            }
        });
    });

    // ── Retrieve File from Student ──
    connect(m_toolbar, &ToolbarWidget::retrieveFileClicked, this, [this]() {
        if (m_grid->studentCount() == 0) {
            QMessageBox::information(this, Lang::get().t("Retrieve File", "Tarik File"), Lang::get().t("No students connected.", "Tidak ada siswa yang terhubung."));
            return;
        }

        // Pick a student
        auto selected = m_grid->selectedTiles();
        QString targetId;
        if (selected.size() == 1) {
            targetId = selected.first()->studentId();
        } else {
            // Show picker
            auto* pickDlg = new QDialog(this);
            pickDlg->setWindowTitle(Lang::get().t("Select Student", "Pilih Siswa"));
            pickDlg->setMinimumWidth(360);
            pickDlg->setAttribute(Qt::WA_DeleteOnClose);
            pickDlg->setStyleSheet(Styles::dialogStyle());

            auto* pickLayout = new QVBoxLayout(pickDlg);
            pickLayout->setSpacing(12);
            pickLayout->setContentsMargins(24, 24, 24, 24);

            auto* pickHeader = new QLabel(Lang::get().t("Select a student to browse files:", "Pilih siswa untuk melihat file:"), pickDlg);
            pickHeader->setStyleSheet("font-size: 11pt; font-weight: bold; color: #1A73E8;");

            auto* studentList = new QComboBox(pickDlg);
            studentList->setStyleSheet(
                "QComboBox { padding: 8px 12px; font-size: 10pt;"
                " border: 1px solid rgba(0,60,120,0.12); border-radius: 6px;"
                " background: #E8EEF4; color: #1A2233; }"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView { background: #E8EEF4; color: #1A2233;"
                " selection-background-color: rgba(26,115,232,0.15); }");

            for (auto* tile : m_grid->allTiles()) {
                studentList->addItem(tile->studentId(), tile->studentId());
            }

            auto* okBtn = new QPushButton("Browse Files", pickDlg);
            okBtn->setStyleSheet(Styles::primaryButtonStyle());
            okBtn->setCursor(Qt::PointingHandCursor);

            pickLayout->addWidget(pickHeader);
            pickLayout->addWidget(studentList);
            pickLayout->addWidget(okBtn);

            connect(okBtn, &QPushButton::clicked, pickDlg, [pickDlg, studentList]() {
                pickDlg->done(1);
            });

            if (pickDlg->exec() != 1) return;
            targetId = studentList->currentData().toString();
        }

        if (targetId.isEmpty()) return;

        // Create file browser dialog
        auto* browserDlg = new QDialog(this);
        browserDlg->setWindowTitle(QStringLiteral("File Browser - %1").arg(targetId));
        browserDlg->setMinimumSize(640, 500);
        browserDlg->setAttribute(Qt::WA_DeleteOnClose);
        browserDlg->setStyleSheet(Styles::dialogStyle());

        auto* layout = new QVBoxLayout(browserDlg);
        layout->setSpacing(10);
        layout->setContentsMargins(20, 20, 20, 20);

        auto* header = new QLabel("Remote File Browser", browserDlg);
        header->setStyleSheet("font-size: 14pt; font-weight: bold; color: #1A73E8;");

        auto* pathBar = new QHBoxLayout();
        auto* pathLabel = new QLabel("Path:", browserDlg);
        pathLabel->setStyleSheet("font-weight: bold;");
        auto* pathEdit = new QLineEdit(browserDlg);
        pathEdit->setStyleSheet(
            "padding: 6px 10px; font-size: 10pt; border: 1px solid rgba(0,60,120,0.12);"
            " border-radius: 6px; background: #E8EEF4; color: #1A2233;");
        pathEdit->setReadOnly(false);
        pathEdit->setPlaceholderText("Enter path or navigate by clicking folders...");
        auto* goBtn = new QPushButton("Navigate", browserDlg);
        goBtn->setStyleSheet(Styles::primaryButtonStyle() +
            "QPushButton { padding: 6px 18px; }");
        goBtn->setCursor(Qt::PointingHandCursor);
        pathBar->addWidget(pathLabel);
        pathBar->addWidget(pathEdit, 1);
        pathBar->addWidget(goBtn);

        // File list
        auto* fileList = new QTreeWidget(browserDlg);
        fileList->setHeaderLabels({"Name", "Size", "Type"});
        fileList->setColumnWidth(0, 340);
        fileList->setColumnWidth(1, 100);
        fileList->setStyleSheet(
            "QTreeWidget { background: #FFFFFF; color: #1A2233;"
            " border: 1px solid rgba(0,60,120,0.08); border-radius: 8px;"
            " font-size: 10pt; }"
            "QTreeWidget::item { padding: 4px 8px; }"
            "QTreeWidget::item:hover { background: rgba(31,111,235,0.12); }"
            "QTreeWidget::item:selected { background: rgba(31,111,235,0.25); }"
            "QHeaderView::section { background: #E8EEF4; color: #1A2233;"
            " border: 1px solid rgba(255,255,255,0.06); padding: 6px; font-weight: bold; }");
        fileList->setRootIsDecorated(false);
        fileList->setSelectionMode(QAbstractItemView::SingleSelection);

        // Buttons row
        auto* btnRow = new QHBoxLayout();
        auto* mkdirBtn = new QPushButton("New Folder", browserDlg);
        mkdirBtn->setStyleSheet(Styles::secondaryButtonStyle());
        mkdirBtn->setCursor(Qt::PointingHandCursor);
        auto* downloadBtn = new QPushButton("Download Selected", browserDlg);
        downloadBtn->setStyleSheet(Styles::primaryButtonStyle());
        downloadBtn->setCursor(Qt::PointingHandCursor);
        auto* closeBtn = new QPushButton("Close", browserDlg);
        closeBtn->setStyleSheet(Styles::secondaryButtonStyle());
        closeBtn->setCursor(Qt::PointingHandCursor);
        btnRow->addWidget(mkdirBtn);
        btnRow->addStretch();
        btnRow->addWidget(downloadBtn);
        btnRow->addWidget(closeBtn);

        layout->addWidget(header);
        layout->addLayout(pathBar);
        layout->addWidget(fileList, 1);
        layout->addLayout(btnRow);

        // State
        auto currentPath = QSharedPointer<QString>::create("");
        auto studentIdCopy = QSharedPointer<QString>::create(targetId);

        // Response handler
        auto conn = connect(m_connManager, &ConnectionManager::dirListReceived,
            browserDlg, [fileList, pathEdit, currentPath, studentIdCopy]
            (const QString& studentId, const QString& path, const QJsonArray& entries) {
                if (studentId != *studentIdCopy) return;

                *currentPath = path;
                pathEdit->setText(path);
                fileList->clear();

                for (const auto& entry : entries) {
                    QJsonObject obj = entry.toObject();
                    auto* item = new QTreeWidgetItem();
                    QString name = obj["name"].toString();
                    bool isDir = obj["isDir"].toBool();
                    qint64 size = obj["size"].toVariant().toLongLong();

                    item->setText(0, name);
                    item->setText(1, isDir ? "" : (size < 1024 ? QStringLiteral("%1 B").arg(size) :
                                   (size < 1048576 ? QStringLiteral("%1 KB").arg(size / 1024) :
                                   QStringLiteral("%1 MB").arg(size / 1048576))));
                    item->setText(2, isDir ? "Folder" : "File");
                    item->setData(0, Qt::UserRole, isDir);
                    item->setData(0, Qt::UserRole + 1, name);
                    fileList->addTopLevelItem(item);
                }
        });

        // Navigate on double click
        connect(fileList, &QTreeWidget::itemDoubleClicked, browserDlg,
            [this, studentIdCopy, currentPath](QTreeWidgetItem* item, int) {
                bool isDir = item->data(0, Qt::UserRole).toBool();
                if (!isDir) return;
                QString name = item->data(0, Qt::UserRole + 1).toString();
                QString newPath;
                if (name == "..") {
                    newPath = QFileInfo(*currentPath).path();
                } else {
                    newPath = *currentPath + "/" + name;
                }
                m_connManager->sendDirListRequest(*studentIdCopy, newPath);
        });

        // Navigate button
        connect(goBtn, &QPushButton::clicked, browserDlg,
            [this, pathEdit, studentIdCopy]() {
                QString path = pathEdit->text().trimmed();
                if (!path.isEmpty()) {
                    m_connManager->sendDirListRequest(*studentIdCopy, path);
                }
        });

        // Mkdir
        connect(mkdirBtn, &QPushButton::clicked, browserDlg,
            [this, studentIdCopy, currentPath]() {
                bool ok;
                QString folderName = QInputDialog::getText(this, "New Folder",
                    "Folder name:", QLineEdit::Normal, "New Folder", &ok);
                if (ok && !folderName.isEmpty()) {
                    QString fullPath = *currentPath + "/" + folderName;
                    m_connManager->sendMkdirRequest(*studentIdCopy, fullPath);
                    // Refresh after a short delay
                    QTimer::singleShot(500, this, [this, studentIdCopy, currentPath]() {
                        m_connManager->sendDirListRequest(*studentIdCopy, *currentPath);
                    });
                }
        });

        // Download
        connect(downloadBtn, &QPushButton::clicked, browserDlg,
            [this, fileList, studentIdCopy, currentPath]() {
                auto* selectedItem = fileList->currentItem();
                if (!selectedItem) {
                    QMessageBox::information(this, "Download", "Please select a file first.");
                    return;
                }
                bool isDir = selectedItem->data(0, Qt::UserRole).toBool();
                if (isDir) {
                    QMessageBox::information(this, "Download", "Folder download not yet supported.\nPlease select a file.");
                    return;
                }
                QString fileName = selectedItem->data(0, Qt::UserRole + 1).toString();
                QString remotePath = *currentPath + "/" + fileName;
                m_connManager->sendFileRetrieveRequest(*studentIdCopy, remotePath);
                statusBar()->showMessage(QStringLiteral("Requesting file '%1' from student...").arg(fileName), 5000);
        });

        connect(closeBtn, &QPushButton::clicked, browserDlg, &QDialog::close);

        // Clean up the connection when dialog closes
        connect(browserDlg, &QDialog::destroyed, this, [conn]() {
            QObject::disconnect(conn);
        });

        browserDlg->show();
        // Request initial directory listing
        m_connManager->sendDirListRequest(targetId, "");
    });

    // ── Handle incoming file retrieval from student ──
    {
        auto* retrieveFile = new QFile(this);
        auto* retrieveFileName = new QString();

        connect(m_connManager, &ConnectionManager::fileRetrieveStarted,
            this, [this, retrieveFile, retrieveFileName](const QString& studentId, const QString& fileName, qint64 fileSize) {
                Q_UNUSED(studentId)
                QString savePath = QFileDialog::getSaveFileName(this, "Save File As",
                    QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + fileName);
                if (savePath.isEmpty()) return;

                *retrieveFileName = fileName;
                retrieveFile->setFileName(savePath);
                if (!retrieveFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    QMessageBox::warning(this, "Error", "Cannot open file for saving.");
                    return;
                }
                statusBar()->showMessage(QStringLiteral("Downloading '%1' (%2 bytes)...").arg(fileName).arg(fileSize));
        });

        connect(m_connManager, &ConnectionManager::fileRetrieveChunk,
            this, [retrieveFile](const QString&, const QByteArray& data) {
                if (retrieveFile->isOpen()) {
                    retrieveFile->write(data);
                }
        });

        connect(m_connManager, &ConnectionManager::fileRetrieveCompleted,
            this, [this, retrieveFile, retrieveFileName](const QString&, const QString& fileName) {
                if (retrieveFile->isOpen()) {
                    retrieveFile->close();
                    statusBar()->showMessage(QStringLiteral("File '%1' downloaded successfully!").arg(fileName), 5000);
                    QMessageBox::information(this, "Download Complete",
                        QStringLiteral("File '%1' saved to:\n%2").arg(fileName, retrieveFile->fileName()));
                }
        });
    }

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

    // Broadcast current update speed so the new student gets it
    if (m_connManager && m_updateSpeedSlider) {
        m_connManager->broadcastUpdateSpeed(m_updateSpeedSlider->value());
    }
}

void TutorWindow::onStudentDisconnected(const QString& studentId)
{
    qInfo() << "[TutorWindow] Student disconnected:" << studentId;
    // Remove the tile entirely so shutdown/disconnect is immediately visible
    m_grid->removeStudent(studentId);

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
        status = Lang::get().t(
            QStringLiteral("Listening on port %1  |  Waiting for students...").arg(DEFAULT_PORT),
            QStringLiteral("Mendengarkan port %1  |  Menunggu siswa...").arg(DEFAULT_PORT)
        );
    } else {
        status = Lang::get().t(
            QStringLiteral("%1 Online").arg(connected),
            QStringLiteral("%1 Daring").arg(connected)
        );
        if (selected > 0) {
            status += Lang::get().t(
                QStringLiteral("  |  %1 Selected").arg(selected),
                QStringLiteral("  |  %1 Terpilih").arg(selected)
            );
        }
    }

    m_statusLabel->setText(status);
}

void TutorWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    m_connManager->stopListening();
    event->accept();
}

void TutorWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
}

void TutorWindow::saveSettings()
{
    QSettings settings("Simanta", "TeacherConsole");
    settings.beginGroup("Window");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("thumbSize", m_thumbSizeSlider->value());
    settings.setValue("updateSpeed", m_updateSpeedSlider->value());
    settings.endGroup();
}

void TutorWindow::loadSettings()
{
    QSettings settings("Simanta", "TeacherConsole");
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

void TutorWindow::setupInternetTab()
{
    m_internetWidget = new QWidget(this);
    m_internetWidget->setObjectName("InternetPage");
    m_internetWidget->setStyleSheet("#InternetPage { background: #F8FAFC; }");
    
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    
    auto* scrollContent = new QWidget();
    scrollContent->setStyleSheet("background: transparent;");
    auto* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(16);
    
    auto* header = new QLabel(Lang::get().t("Internet Access Control", "Kontrol Akses Internet"));
    header->setStyleSheet("font-size: 22pt; font-weight: bold; color: #0F172A; background: transparent;");
    auto* desc = new QLabel(Lang::get().t("Manage and restrict student web access. Useful for exams or studying.", "Atur dan batasi akses web siswa. Berguna saat ujian atau fokus belajar."));
    desc->setStyleSheet("font-size: 10pt; color: #64748B; background: transparent;");
    layout->addWidget(header);
    layout->addWidget(desc);
    
    // --- Block Internet Row with status indicator ---
    auto* blockRow = new QWidget();
    blockRow->setObjectName("InetRow");
    blockRow->setMinimumHeight(68);
    blockRow->setStyleSheet("#InetRow { background:#FFFFFF; border:1px solid #E2E8F0; border-radius:10px; }");
    auto* blockLay = new QHBoxLayout(blockRow);
    blockLay->setContentsMargins(20, 14, 20, 14);
    blockLay->setSpacing(14);
    
    auto* blockDot = new QLabel();
    blockDot->setFixedSize(10, 10);
    blockDot->setStyleSheet("background:#94A3B8; border-radius:5px; border:none;");
    
    auto* blockTextCol = new QVBoxLayout();
    blockTextCol->setSpacing(2);
    auto* blockTitle = new QLabel(Lang::get().t("Block All Internet Access", "Blokir Akses Internet Sepenuhnya"));
    blockTitle->setObjectName("InetTitle");
    blockTitle->setStyleSheet("#InetTitle { font-family:'Segoe UI'; font-size:11pt; font-weight:bold; color:#1E293B; background:transparent; border:none; }");
    auto* blockStatusLbl = new QLabel(Lang::get().t("Status: Inactive", "Status: Tidak aktif"));
    blockStatusLbl->setObjectName("InetStat");
    blockStatusLbl->setStyleSheet("#InetStat { font-family:'Segoe UI'; font-size:9pt; color:#94A3B8; background:transparent; border:none; }");
    blockTextCol->addWidget(blockTitle);
    blockTextCol->addWidget(blockStatusLbl);
    
    auto* blockInternetBtn = new QPushButton(Lang::get().t("Enable", "Aktifkan"));
    blockInternetBtn->setObjectName("InetBlock");
    blockInternetBtn->setCheckable(true);
    blockInternetBtn->setFixedSize(140, 36);
    blockInternetBtn->setStyleSheet(
        "#InetBlock { background:#F1F5F9; color:#475569; border-radius:8px; font-family:'Segoe UI'; font-weight:bold; font-size:10pt; border:1px solid #CBD5E1; }"
        "#InetBlock:checked { background:#EF4444; color:white; border:none; }"
        "#InetBlock:hover { background:#E2E8F0; }"
        "#InetBlock:checked:hover { background:#DC2626; }"
    );
    blockInternetBtn->setCursor(Qt::PointingHandCursor);
    connect(blockInternetBtn, &QPushButton::toggled, this, [this, blockInternetBtn, blockDot, blockStatusLbl, blockRow](bool on) {
        blockInternetBtn->setText(on ? Lang::get().t("Blocked", "Diblokir") : Lang::get().t("Enable", "Aktifkan"));
        blockDot->setStyleSheet(on ? "background:#EF4444; border-radius:5px; border:none;" : "background:#94A3B8; border-radius:5px; border:none;");
        blockStatusLbl->setText(on ? Lang::get().t("Status: Internet BLOCKED", "Status: Internet DIBLOKIR") : Lang::get().t("Status: Inactive", "Status: Tidak aktif"));
        blockStatusLbl->setStyleSheet(on
            ? "#InetStat { font-family:'Segoe UI'; font-size:9pt; color:#EF4444; font-weight:bold; background:transparent; border:none; }"
            : "#InetStat { font-family:'Segoe UI'; font-size:9pt; color:#94A3B8; background:transparent; border:none; }");
        blockRow->setStyleSheet(on
            ? "#InetRow { background:#FEF2F2; border:1px solid #FECACA; border-radius:10px; }"
            : "#InetRow { background:#FFFFFF; border:1px solid #E2E8F0; border-radius:10px; }");
        // Send command immediately
        QString cmd = on ? "INTERNET_BLOCK" : "INTERNET_UNBLOCK";
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) m_connManager->sendCommandToAll(cmd);
        else {
            QStringList ids;
            for (auto* t : selected) ids.append(t->studentId());
            m_connManager->sendCommand(ids, cmd);
        }
    });
    blockLay->addWidget(blockDot, 0, Qt::AlignVCenter);
    blockLay->addLayout(blockTextCol, 1);
    blockLay->addWidget(blockInternetBtn, 0, Qt::AlignVCenter);
    layout->addWidget(blockRow);
    
    // --- Whitelist Panel ---
    auto* wlPanel = new QWidget();
    wlPanel->setObjectName("InetWL");
    wlPanel->setStyleSheet("#InetWL { background:#FFFFFF; border:1px solid #E2E8F0; border-radius:10px; }");
    auto* wlLayout = new QVBoxLayout(wlPanel);
    wlLayout->setContentsMargins(24, 20, 24, 20);
    wlLayout->setSpacing(14);
    
    auto* wlTitleCol = new QVBoxLayout();
    wlTitleCol->setSpacing(2);
    auto* examTitle = new QLabel(Lang::get().t("Exam Mode — URL Whitelist", "Mode Ujian — Whitelist URL"));
    examTitle->setObjectName("WlTitle");
    examTitle->setStyleSheet("#WlTitle { font-family:'Segoe UI'; font-size:12pt; font-weight:bold; color:#1E293B; background:transparent; border:none; }");
    auto* examDesc = new QLabel(Lang::get().t("Students can only access the websites in this whitelist.", "Siswa hanya bisa mengakses situs yang ada di daftar ini."));
    examDesc->setObjectName("WlDesc");
    examDesc->setStyleSheet("#WlDesc { font-family:'Segoe UI'; font-size:9pt; color:#64748B; background:transparent; border:none; }");
    wlTitleCol->addWidget(examTitle);
    wlTitleCol->addWidget(examDesc);
    wlLayout->addLayout(wlTitleCol);
    
    // URL input
    auto* urlInputLay = new QHBoxLayout();
    urlInputLay->setSpacing(8);
    auto* urlInput = new QLineEdit();
    urlInput->setObjectName("WlInput");
    urlInput->setPlaceholderText(Lang::get().t("Enter URL, e.g. https://exam.school.edu", "Masukkan URL, contoh: https://ujian.sekolah.edu"));
    urlInput->setStyleSheet(
        "#WlInput { border:1px solid #CBD5E1; border-radius:8px; min-height:38px; padding:0 14px; font-size:10pt; background:#F8FAFC; color:#0F172A; }"
        "#WlInput:focus { border:2px solid #3B82F6; background:#FFFFFF; }"
    );
    auto* addUrlBtn = new QPushButton(Lang::get().t("+ Add", "+ Tambah"));
    addUrlBtn->setObjectName("WlAdd");
    addUrlBtn->setFixedSize(110, 38);
    addUrlBtn->setStyleSheet(
        "#WlAdd { background:#3B82F6; color:white; border-radius:8px; font-family:'Segoe UI'; font-weight:bold; font-size:10pt; border:none; }"
        "#WlAdd:hover { background:#2563EB; }"
    );
    addUrlBtn->setCursor(Qt::PointingHandCursor);
    urlInputLay->addWidget(urlInput, 1);
    urlInputLay->addWidget(addUrlBtn);
    wlLayout->addLayout(urlInputLay);
    
    // URL list area
    auto* urlListArea = new QWidget();
    urlListArea->setObjectName("UrlArea");
    urlListArea->setStyleSheet("#UrlArea { background:#F8FAFC; border:1px solid #E2E8F0; border-radius:8px; }");
    auto* urlAreaLayout = new QVBoxLayout(urlListArea);
    urlAreaLayout->setContentsMargins(0, 0, 0, 0);
    urlAreaLayout->setSpacing(0);
    
    auto* urlScroll = new QScrollArea();
    urlScroll->setWidgetResizable(true);
    urlScroll->setFrameShape(QFrame::NoFrame);
    urlScroll->setMinimumHeight(160);
    urlScroll->setStyleSheet("QScrollArea{background:transparent;border:none;} QScrollBar:vertical{width:6px;background:transparent;} QScrollBar::handle:vertical{background:rgba(0,0,0,0.12);border-radius:3px;} QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
    auto* urlListWidget = new QWidget();
    urlListWidget->setStyleSheet("background:transparent;");
    auto* urlItemsLay = new QVBoxLayout(urlListWidget);
    urlItemsLay->setContentsMargins(8, 8, 8, 8);
    urlItemsLay->setSpacing(4);
    
    auto* emptyLabel = new QLabel(Lang::get().t("No URLs added yet.\nClick '+ Add' to add a whitelist URL.", "Belum ada URL ditambahkan.\nKlik '+ Tambah' untuk menambahkan URL whitelist."));
    emptyLabel->setObjectName("WlEmpty");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("#WlEmpty { font-family:'Segoe UI'; font-size:10pt; color:#94A3B8; background:transparent; border:none; padding:28px; }");
    urlItemsLay->addWidget(emptyLabel);
    urlItemsLay->addStretch();
    urlScroll->setWidget(urlListWidget);
    urlAreaLayout->addWidget(urlScroll);
    
    auto* urlCountLbl = new QLabel(Lang::get().t("0 URLs in whitelist", "0 URL dalam whitelist"));
    urlCountLbl->setObjectName("WlCnt");
    urlCountLbl->setStyleSheet("#WlCnt { font-family:'Segoe UI'; font-size:9pt; color:#64748B; background:transparent; border:none; }");
    
    wlLayout->addWidget(urlListArea, 1);
    wlLayout->addWidget(urlCountLbl);
    layout->addWidget(wlPanel, 1);
    
    // Function to broadcast current whitelist to students
    auto syncWhitelist = [this, urlItemsLay]() {
        QStringList urls;
        for (int i = 0; i < urlItemsLay->count(); i++) {
            if (auto* w = urlItemsLay->itemAt(i)->widget()) {
                if (w->objectName() == "UrlItem") {
                    if (auto* lbl = w->findChild<QLabel*>("UrlTxt")) {
                        urls.append(lbl->text());
                    }
                }
            }
        }
        QString cmd = urls.isEmpty() ? "WHITELIST_CLEAR" : ("WHITELIST_SET:" + urls.join(","));
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) m_connManager->sendCommandToAll(cmd);
        else {
            QStringList ids;
            for (auto* t : selected) ids.append(t->studentId());
            m_connManager->sendCommand(ids, cmd);
        }
    };

    // Add URL item helper
    auto addUrlItem = [urlItemsLay, emptyLabel, urlCountLbl, syncWhitelist](const QString& url) {
        emptyLabel->hide();
        auto* item = new QWidget();
        item->setObjectName("UrlItem");
        item->setFixedHeight(40);
        item->setStyleSheet(
            "#UrlItem { background:#FFFFFF; border:1px solid #E2E8F0; border-radius:6px; }"
            "#UrlItem:hover { background:#F0F9FF; border:1px solid #BAE6FD; }"
        );
        auto* iLay = new QHBoxLayout(item);
        iLay->setContentsMargins(12, 0, 8, 0);
        iLay->setSpacing(8);
        auto* urlLbl = new QLabel(url);
        urlLbl->setObjectName("UrlTxt");
        urlLbl->setStyleSheet("#UrlTxt { font-family:'Segoe UI'; font-size:10pt; color:#1E293B; background:transparent; border:none; }");
        auto* delBtn = new QPushButton("X");
        delBtn->setObjectName("UrlDel");
        delBtn->setFixedSize(26, 26);
        delBtn->setStyleSheet(
            "#UrlDel { background:transparent; color:#94A3B8; border:none; border-radius:13px; font-size:9pt; font-weight:bold; }"
            "#UrlDel:hover { background:#FEE2E2; color:#EF4444; }"
        );
        delBtn->setCursor(Qt::PointingHandCursor);
        iLay->addWidget(urlLbl, 1);
        iLay->addWidget(delBtn);
        urlItemsLay->insertWidget(urlItemsLay->count() - 1, item);
        // Update count
        int c = 0;
        for (int i = 0; i < urlItemsLay->count(); i++)
            if (auto* w = urlItemsLay->itemAt(i)->widget())
                if (w->objectName() == "UrlItem") c++;
        urlCountLbl->setText(Lang::get().t(QStringLiteral("%1 URLs in whitelist").arg(c), QStringLiteral("%1 URL dalam whitelist").arg(c)));
        
        syncWhitelist();
        
        QObject::connect(delBtn, &QPushButton::clicked, item, [item, urlItemsLay, emptyLabel, urlCountLbl, syncWhitelist]() {
            item->deleteLater();
            QTimer::singleShot(100, [urlItemsLay, emptyLabel, urlCountLbl, syncWhitelist]() {
                int n = 0;
                for (int i = 0; i < urlItemsLay->count(); i++)
                    if (auto* w = urlItemsLay->itemAt(i)->widget())
                        if (w->objectName() == "UrlItem") n++;
                urlCountLbl->setText(Lang::get().t(QStringLiteral("%1 URLs in whitelist").arg(n), QStringLiteral("%1 URL dalam whitelist").arg(n)));
                if (n == 0) emptyLabel->show();
                syncWhitelist();
            });
        });
    };
    
    connect(addUrlBtn, &QPushButton::clicked, this, [urlInput, addUrlItem]() {
        QString t = urlInput->text().trimmed();
        if (!t.isEmpty()) { addUrlItem(t); urlInput->clear(); }
    });
    connect(urlInput, &QLineEdit::returnPressed, this, [urlInput, addUrlItem]() {
        QString t = urlInput->text().trimmed();
        if (!t.isEmpty()) { addUrlItem(t); urlInput->clear(); }
    });
    
    scrollArea->setWidget(scrollContent);
    auto* pageLayout = new QVBoxLayout(m_internetWidget);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scrollArea);
}

void TutorWindow::setupSecurityTab()
{
    m_securityWidget = new QWidget(this);
    m_securityWidget->setObjectName("SecurityPage");
    m_securityWidget->setStyleSheet("#SecurityPage { background:#F8FAFC; }");
    
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background:transparent; border:none; }");
    
    auto* scrollContent = new QWidget();
    scrollContent->setStyleSheet("background:transparent;");
    auto* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(16);
    
    auto* header = new QLabel(Lang::get().t("System & Class Security", "Keamanan Sistem & Kelas"));
    header->setStyleSheet("font-size:22pt; font-weight:bold; color:#0F172A; background:transparent;");
    auto* desc = new QLabel(Lang::get().t("Manage student computer security, restrict external devices and system apps.", "Atur keamanan komputer siswa, batasi perangkat eksternal dan aplikasi sistem."));
    desc->setStyleSheet("font-size:10pt; color:#64748B; background:transparent;");
    layout->addWidget(header);
    layout->addWidget(desc);
    
    // Summary label
    auto* summaryLabel = new QLabel("All security features disabled");
    summaryLabel->setObjectName("SecSum");
    summaryLabel->setStyleSheet("#SecSum { font-family:'Segoe UI'; font-size:10pt; color:#94A3B8; background:transparent; border:none; }");
    layout->addWidget(summaryLabel);
    
    // --- Helper lambda with status indicator ---
    auto createToggleRow = [this](const QString& title, const QString& subtitle) -> QPair<QWidget*, QPushButton*>
    {
        auto* rowWidget = new QWidget();
        rowWidget->setObjectName("SecRow");
        rowWidget->setMinimumHeight(72);
        rowWidget->setToolTip(subtitle);
        rowWidget->setStyleSheet("#SecRow { background:#FFFFFF; border:1px solid #E2E8F0; border-radius:10px; }");
        
        auto* hlay = new QHBoxLayout(rowWidget);
        hlay->setContentsMargins(20, 14, 20, 14);
        hlay->setSpacing(14);
        
        auto* dot = new QLabel();
        dot->setFixedSize(10, 10);
        dot->setStyleSheet("background:#94A3B8; border-radius:5px; border:none;");
        
        auto* leftCol = new QVBoxLayout();
        leftCol->setSpacing(2);
        auto* titleLbl = new QLabel(title);
        titleLbl->setObjectName("SecTitle");
        titleLbl->setStyleSheet("#SecTitle { font-family:'Segoe UI'; font-size:11pt; font-weight:bold; color:#1E293B; background:transparent; border:none; }");
        auto* statusLbl = new QLabel(Lang::get().t("Inactive", "Tidak aktif"));
        statusLbl->setObjectName("SecStat");
        statusLbl->setStyleSheet("#SecStat { font-family:'Segoe UI'; font-size:9pt; color:#94A3B8; background:transparent; border:none; }");
        leftCol->addWidget(titleLbl);
        leftCol->addWidget(statusLbl);
        
        auto* toggleBtn = new QPushButton(Lang::get().t("Disable", "Nonaktif"));
        toggleBtn->setObjectName("SecToggle");
        toggleBtn->setCheckable(true);
        toggleBtn->setFixedSize(120, 36);
        toggleBtn->setStyleSheet(
            "#SecToggle { background:#F1F5F9; color:#475569; border-radius:8px; font-family:'Segoe UI'; font-weight:bold; font-size:10pt; border:1px solid #CBD5E1; }"
            "#SecToggle:checked { background:#3B82F6; color:white; border:none; }"
            "#SecToggle:hover { background:#E2E8F0; }"
            "#SecToggle:checked:hover { background:#2563EB; }"
        );
        toggleBtn->setCursor(Qt::PointingHandCursor);
        connect(toggleBtn, &QPushButton::toggled, this, [this, toggleBtn, dot, statusLbl, rowWidget](bool on) {
            toggleBtn->setText(on ? Lang::get().t("Active", "Aktif") : Lang::get().t("Disable", "Nonaktif"));
            dot->setStyleSheet(on ? "background:#10B981; border-radius:5px; border:none;" : "background:#94A3B8; border-radius:5px; border:none;");
            statusLbl->setText(on ? Lang::get().t("Currently active", "Sedang aktif") : Lang::get().t("Inactive", "Tidak aktif"));
            statusLbl->setStyleSheet(on
                ? "#SecStat { font-family:'Segoe UI'; font-size:9pt; color:#10B981; font-weight:600; background:transparent; border:none; }"
                : "#SecStat { font-family:'Segoe UI'; font-size:9pt; color:#94A3B8; background:transparent; border:none; }");
            rowWidget->setStyleSheet(on
                ? "#SecRow { background:#F0FDF4; border:1px solid #86EFAC; border-radius:10px; }"
                : "#SecRow { background:#FFFFFF; border:1px solid #E2E8F0; border-radius:10px; }");
        });
        
        hlay->addWidget(dot, 0, Qt::AlignVCenter);
        hlay->addLayout(leftCol, 1);
        hlay->addWidget(toggleBtn, 0, Qt::AlignVCenter);
        
        return qMakePair(rowWidget, toggleBtn);
    };
    
    auto [w1, btnUsb]      = createToggleRow(Lang::get().t("Block USB Flash Drives", "Blokir USB Flashdisk"),
        Lang::get().t("Prevent students from reading/writing data to external USB devices", "Mencegah siswa membaca/menulis data ke perangkat USB eksternal"));
    auto [w2, btnTaskMgr]  = createToggleRow(Lang::get().t("Disable Task Manager", "Nonaktifkan Task Manager"),
        Lang::get().t("Prevent students from force-closing monitoring apps via Task Manager", "Mencegah siswa menutup paksa aplikasi pemantauan via Task Manager"));
    auto [w3, btnRegedit]  = createToggleRow(Lang::get().t("Disable Registry Editor", "Nonaktifkan Registry Editor"),
        Lang::get().t("Prevent harmful changes to the Windows Registry", "Mencegah perubahan berbahaya pada Windows Registry"));
    auto [w4, btnSettings] = createToggleRow(Lang::get().t("Lock Windows Settings", "Kunci Pengaturan Windows"),
        Lang::get().t("Prevent students from changing time, IP address, or other system settings", "Mencegah siswa mengubah jam, IP address, atau pengaturan sistem lainnya"));
    
    layout->addWidget(w1);
    layout->addWidget(w2);
    layout->addWidget(w3);
    layout->addWidget(w4);
    
    // Update summary when toggles change
    auto updateSummary = [summaryLabel, btnUsb, btnTaskMgr, btnRegedit, btnSettings]() {
        int n = 0;
        if (btnUsb->isChecked()) n++;
        if (btnTaskMgr->isChecked()) n++;
        if (btnRegedit->isChecked()) n++;
        if (btnSettings->isChecked()) n++;
        if (n == 0) {
            summaryLabel->setText(Lang::get().t("All security features disabled", "Semua fitur keamanan dinonaktifkan"));
            summaryLabel->setStyleSheet("#SecSum { font-family:'Segoe UI'; font-size:10pt; color:#94A3B8; background:transparent; border:none; }");
        } else {
            summaryLabel->setText(Lang::get().t(QStringLiteral("%1 of 4 security features active").arg(n), QStringLiteral("%1 dari 4 fitur keamanan aktif").arg(n)));
            summaryLabel->setStyleSheet("#SecSum { font-family:'Segoe UI'; font-size:10pt; color:#10B981; font-weight:bold; background:transparent; border:none; }");
        }
    };
    connect(btnUsb, &QPushButton::toggled, this, updateSummary);
    connect(btnTaskMgr, &QPushButton::toggled, this, updateSummary);
    connect(btnRegedit, &QPushButton::toggled, this, updateSummary);
    connect(btnSettings, &QPushButton::toggled, this, updateSummary);
    
    // Send commands immediately per toggle
    auto sendSecCmd = [this](const QString& onCmd, const QString& offCmd, bool on) {
        QString cmd = on ? onCmd : offCmd;
        auto selected = m_grid->selectedTiles();
        if (selected.isEmpty()) m_connManager->sendCommandToAll(cmd);
        else {
            QStringList ids;
            for (auto* tile : selected) ids.append(tile->studentId());
            m_connManager->sendCommand(ids, cmd);
        }
    };
    connect(btnUsb, &QPushButton::toggled, this, [this, sendSecCmd](bool on) {
        sendSecCmd("USB_BLOCK", "USB_UNBLOCK", on);
    });
    connect(btnTaskMgr, &QPushButton::toggled, this, [this, sendSecCmd](bool on) {
        sendSecCmd("TASKMGR_BLOCK", "TASKMGR_UNBLOCK", on);
    });
    connect(btnRegedit, &QPushButton::toggled, this, [this, sendSecCmd](bool on) {
        sendSecCmd("REGEDIT_BLOCK", "REGEDIT_UNBLOCK", on);
    });
    connect(btnSettings, &QPushButton::toggled, this, [this, sendSecCmd](bool on) {
        sendSecCmd("SETTINGS_BLOCK", "SETTINGS_UNBLOCK", on);
    });
    
    layout->addStretch();
    scrollArea->setWidget(scrollContent);
    auto* pageLayout = new QVBoxLayout(m_securityWidget);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scrollArea);
}

void TutorWindow::setupSettingsTab()
{
    m_settingsWidget = new QWidget();
    m_settingsWidget->setObjectName("SettingsPage");
    m_settingsWidget->setStyleSheet("#SettingsPage { background: #F8FAFC; }");
    
    auto* layout = new QVBoxLayout(m_settingsWidget);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(16);
    
    m_settingsHeader = new QLabel(Lang::get().t("Settings", "Pengaturan"));
    m_settingsHeader->setStyleSheet("font-size: 22pt; font-weight: bold; color: #0F172A; background: transparent;");
    m_settingsDesc = new QLabel(Lang::get().t("Application preferences and configurations.", "Preferensi dan konfigurasi aplikasi."));
    m_settingsDesc->setStyleSheet("font-size: 10pt; color: #64748B; background: transparent;");
    
    layout->addWidget(m_settingsHeader);
    layout->addWidget(m_settingsDesc);
    
    // Language Row
    auto* langRow = new QWidget();
    langRow->setStyleSheet("background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 10px;");
    langRow->setMinimumHeight(72);
    auto* langLay = new QHBoxLayout(langRow);
    langLay->setContentsMargins(20, 14, 20, 14);
    
    m_langLabel = new QLabel(Lang::get().t("Language", "Bahasa"));
    m_langLabel->setStyleSheet("font-size: 11pt; font-weight: bold; color: #1E293B; border: none;");
    
    m_langCombo = new QComboBox();
    m_langCombo->setFixedSize(140, 36);
    m_langCombo->setStyleSheet(
        "QComboBox { border: 1px solid #CBD5E1; border-radius: 8px; padding: 4px 10px; font-size: 10pt; color: #1E293B; background: #FFFFFF; }"
        "QComboBox QAbstractItemView { border: 1px solid #CBD5E1; selection-background-color: #F1F5F9; color: #1E293B; background: #FFFFFF; }"
        "QComboBox::drop-down { border: none; }"
    );
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("Indonesia", "id");
    
    if (Lang::get().currentLang == "id") {
        m_langCombo->setCurrentIndex(1);
    } else {
        m_langCombo->setCurrentIndex(0);
    }
    
    connect(m_langCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        QString newLang = m_langCombo->itemData(index).toString();
        if (newLang != Lang::get().currentLang) {
            Lang::get().setLanguage(newLang);
            updateTranslations();
            m_connManager->sendCommandToAll("SET_LANG:" + newLang);
        }
    });
    
    langLay->addWidget(m_langLabel);
    langLay->addStretch();
    langLay->addWidget(m_langCombo);
    
    layout->addWidget(langRow);
    layout->addStretch();
}

void TutorWindow::updateTranslations()
{
    // Update simple labels we kept pointers to
    if (m_settingsHeader) m_settingsHeader->setText(Lang::get().t("Settings", "Pengaturan"));
    if (m_settingsDesc) m_settingsDesc->setText(Lang::get().t("Application preferences and configurations.", "Preferensi dan konfigurasi aplikasi."));
    if (m_langLabel) m_langLabel->setText(Lang::get().t("Language", "Bahasa"));
    
    // Update Sidebar
    if (m_sidebar) m_sidebar->updateTranslations();
    
    // Update toolbar buttons
    if (m_toolbar) m_toolbar->updateTranslations();
    
    // Update student grid
    if (m_grid) m_grid->updateTranslations();
    
    // Update status bar labels
    if (m_updateSpeedLabel) m_updateSpeedLabel->setText(Lang::get().t("Update Speed:", "Kecepatan Pembaruan:"));
    if (m_thumbSizeLabel) m_thumbSizeLabel->setText(Lang::get().t("Thumbnail Size:", "Ukuran Thumbnail:"));
    updateStatusLabel();
    
    // Refresh the entire stacked widget contents to re-read language dynamically!
    if (m_internetWidget) {
        m_stackedWidget->removeWidget(m_internetWidget);
        m_internetWidget->deleteLater();
        setupInternetTab();
        m_stackedWidget->insertWidget(1, m_internetWidget);
    }
    
    if (m_securityWidget) {
        m_stackedWidget->removeWidget(m_securityWidget);
        m_securityWidget->deleteLater();
        setupSecurityTab();
        m_stackedWidget->insertWidget(2, m_securityWidget);
    }
}

} // namespace LabMonitor

