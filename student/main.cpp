#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QTextStream>
#include <QMessageBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QDesktopServices>
#include <QUrl>
#include <QPointer>
#include <QScreen>
#include <QProcess>
#include <QSettings>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QPainter>
#include <csignal>

#include "student_agent.h"
#include "protocol.h"

static LabMonitor::StudentAgent* g_agent = nullptr;

// ── Shared dialog stylesheet ────────────────────────────────
static const char* DIALOG_STYLE = R"(
    QDialog {
        background-color: #FFFFFF;
        color: #2C3E35;
    }
    QLabel {
        color: #2C3E35;
        background: transparent;
    }
    QPushButton {
        color: #FFFFFF;
    }
    QTextEdit {
        color: #2C3E35;
        background-color: #F0F7F2;
    }
    QLineEdit {
        color: #2C3E35;
        background-color: #FFFFFF;
    }
)";

void signalHandler(int signal)
{
    Q_UNUSED(signal)
    QTextStream(stdout) << "\n[lab-student] Shutting down...\n";
    if (g_agent) {
        g_agent->stop();
    }
    QCoreApplication::quit();
}

// ── Lock Screen Overlay ─────────────────────────────────────
class LockOverlay : public QWidget
{
public:
    LockOverlay(QWidget* parent = nullptr) : QWidget(parent)
    {
        // Use Window flag (not Tool) so the WM treats it as a real window
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_ShowWithoutActivating, false);
        setStyleSheet("background-color: #1A4D2A;");
        setCursor(Qt::BlankCursor);
    }

    void activate()
    {
        // Cover the primary screen
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            setGeometry(screen->geometry());
        }
        showFullScreen();
        raise();
        activateWindow();
        setFocus();
    }

    void deactivate()
    {
        hide();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor("#1A4D2A"));

        // Draw lock icon
        QFont iconFont;
        iconFont.setPixelSize(80);
        p.setFont(iconFont);
        p.setPen(Qt::white);
        QRect iconRect(0, height()/2 - 120, width(), 100);
        p.drawText(iconRect, Qt::AlignCenter, "🔒");

        // Draw title
        QFont titleFont("sans-serif", 28, QFont::Bold);
        p.setFont(titleFont);
        QRect titleRect(0, height()/2 - 20, width(), 50);
        p.drawText(titleRect, Qt::AlignCenter, "Screen Locked");

        // Draw subtitle
        QFont subFont("sans-serif", 14);
        p.setFont(subFont);
        p.setPen(QColor(255, 255, 255, 180));
        QRect subRect(0, height()/2 + 40, width(), 60);
        p.drawText(subRect, Qt::AlignCenter, "Your screen has been locked by the teacher.\nPlease wait for instructions.");
    }

    void keyPressEvent(QKeyEvent* e) override {
        // Block all keys except letting the agent handle unlock
        e->accept();
    }

    void closeEvent(QCloseEvent* e) override {
        e->ignore(); // Prevent closing
    }

    void focusOutEvent(QFocusEvent*) override {
        // Re-grab focus
        if (isVisible()) {
            raise();
            activateWindow();
        }
    }
};

// ── Chat Window ─────────────────────────────────────────────
class ChatWindow : public QDialog
{
    Q_OBJECT
public:
    ChatWindow(LabMonitor::StudentAgent* agent, QWidget* parent = nullptr)
        : QDialog(parent), m_agent(agent)
    {
        setWindowTitle("Chat with Teacher");
        setMinimumSize(380, 450);
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

        // Explicit light theme
        setStyleSheet(
            "QDialog { background-color: #FFFFFF; color: #2C3E35; }"
            "QLabel { color: #2C3E35; background: transparent; }"
        );

        auto* layout = new QVBoxLayout(this);
        layout->setSpacing(8);
        layout->setContentsMargins(12, 12, 12, 12);

        // Header
        auto* header = new QLabel("💬 Chat with Teacher", this);
        header->setStyleSheet("font-size: 13pt; font-weight: bold; color: #1B5E28; padding: 4px;");

        // Chat log
        m_chatLog = new QTextEdit(this);
        m_chatLog->setReadOnly(true);
        m_chatLog->setStyleSheet(
            "QTextEdit { background: #F0F7F2; color: #2C3E35; border: 1px solid #BDC7C0;"
            " border-radius: 6px; padding: 8px; font-size: 10pt; }");

        // Input row
        auto* inputLayout = new QHBoxLayout();
        m_input = new QLineEdit(this);
        m_input->setPlaceholderText("Type a message...");
        m_input->setStyleSheet(
            "QLineEdit { padding: 8px; font-size: 10pt; color: #2C3E35;"
            " background: #FFFFFF; border: 1px solid #BDC7C0; border-radius: 4px; }");

        auto* sendBtn = new QPushButton("Send", this);
        sendBtn->setStyleSheet(
            "QPushButton { background: #27AE60; color: white; padding: 8px 16px;"
            " border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background: #2ECC71; }");
        sendBtn->setCursor(Qt::PointingHandCursor);

        inputLayout->addWidget(m_input, 1);
        inputLayout->addWidget(sendBtn);

        layout->addWidget(header);
        layout->addWidget(m_chatLog, 1);
        layout->addLayout(inputLayout);

        connect(sendBtn, &QPushButton::clicked, this, &ChatWindow::onSend);
        connect(m_input, &QLineEdit::returnPressed, this, &ChatWindow::onSend);
    }

    void addMessage(const QString& sender, const QString& message)
    {
        QString color = (sender == "Teacher") ? "#1B5E28" : "#2C3E35";
        m_chatLog->append(QStringLiteral(
            "<p><b style='color:%1'>%2:</b> <span style='color:#2C3E35'>%3</span></p>"
        ).arg(color, sender.toHtmlEscaped(), message.toHtmlEscaped()));

        // Auto-scroll
        auto* sb = m_chatLog->verticalScrollBar();
        sb->setValue(sb->maximum());
    }

signals:
    void messageSent(const QString& message);

private slots:
    void onSend()
    {
        QString text = m_input->text().trimmed();
        if (text.isEmpty()) return;

        if (m_agent) {
            m_agent->sendChat(text);
        }
        emit messageSent(text);
        addMessage("Me", text);
        m_input->clear();
    }

private:
    LabMonitor::StudentAgent* m_agent;
    QTextEdit* m_chatLog;
    QLineEdit* m_input;
};

// ── Helper: create styled message dialog ────────────────────
static QDialog* createStyledMessageDialog(const QString& title, const QString& body,
                                          const QString& sender)
{
    auto* dialog = new QDialog();
    dialog->setWindowTitle("Message from Teacher");
    dialog->setWindowFlags(dialog->windowFlags() | Qt::WindowStaysOnTopHint);
    dialog->setMinimumSize(420, 240);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    // Force light theme
    dialog->setStyleSheet(DIALOG_STYLE);

    auto* layout = new QVBoxLayout(dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 24, 24, 24);

    // Icon + Title
    auto* headerLabel = new QLabel(dialog);
    headerLabel->setText(QStringLiteral("📩 %1").arg(title));
    headerLabel->setStyleSheet("font-size: 15pt; font-weight: bold; color: #1B5E28; padding-bottom: 4px;");
    headerLabel->setWordWrap(true);

    // Sender info
    auto* senderLabel = new QLabel(QStringLiteral("From: %1").arg(sender), dialog);
    senderLabel->setStyleSheet("font-size: 9pt; color: #7F8D84; font-style: italic;");

    // Message body in a styled box
    auto* bodyLabel = new QLabel(dialog);
    bodyLabel->setText(body);
    bodyLabel->setWordWrap(true);
    bodyLabel->setStyleSheet(
        "font-size: 11pt; color: #2C3E35; background: #F0F7F2; border: 1px solid #C8D6CB;"
        " border-radius: 8px; padding: 14px;");
    bodyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    bodyLabel->setMinimumHeight(60);

    // OK button
    auto* okBtn = new QPushButton("OK", dialog);
    okBtn->setFixedSize(120, 36);
    okBtn->setStyleSheet(
        "QPushButton { background: #27AE60; color: white; padding: 8px 32px;"
        " border-radius: 6px; font-weight: bold; font-size: 10pt; border: none; }"
        "QPushButton:hover { background: #2ECC71; }");
    okBtn->setCursor(Qt::PointingHandCursor);

    layout->addWidget(headerLabel);
    layout->addWidget(senderLabel);
    layout->addWidget(bodyLabel, 1);
    layout->addSpacing(8);
    layout->addWidget(okBtn, 0, Qt::AlignCenter);

    QObject::connect(okBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    return dialog;
}

// ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("lab-student");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LabMonitor");

    // Command line parsing (still supported for overrides)
    QCommandLineParser parser;
    parser.setApplicationDescription("Lab Monitor Student Agent");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption teacherOpt(
        QStringList() << "t" << "teacher",
        "Teacher console IP address",
        "ip", ""
    );

    QCommandLineOption portOpt(
        QStringList() << "p" << "port",
        QString("Teacher console port (default: %1)").arg(LabMonitor::DEFAULT_PORT),
        "port", QString::number(LabMonitor::DEFAULT_PORT)
    );

    QCommandLineOption intervalOpt(
        QStringList() << "i" << "interval",
        QString("Capture interval in ms (default: %1)").arg(LabMonitor::DEFAULT_CAPTURE_MS),
        "ms", QString::number(LabMonitor::DEFAULT_CAPTURE_MS)
    );

    QCommandLineOption qualityOpt(
        QStringList() << "q" << "quality",
        QString("JPEG quality 1-100 (default: %1)").arg(LabMonitor::DEFAULT_QUALITY),
        "quality", QString::number(LabMonitor::DEFAULT_QUALITY)
    );

    QCommandLineOption scaleOpt(
        QStringList() << "s" << "scale",
        QString("Capture scale 0.1-1.0 (default: %1)").arg(LabMonitor::DEFAULT_SCALE),
        "scale", QString::number(LabMonitor::DEFAULT_SCALE)
    );

    parser.addOption(teacherOpt);
    parser.addOption(portOpt);
    parser.addOption(intervalOpt);
    parser.addOption(qualityOpt);
    parser.addOption(scaleOpt);

    parser.process(app);

    // ── Load config from file (config.ini next to executable) ──
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings config(configPath, QSettings::IniFormat);

    // Read settings: command line > config file > defaults
    QString teacherIp = parser.value(teacherOpt);
    if (teacherIp.isEmpty()) {
        teacherIp = config.value("teacher_ip", "127.0.0.1").toString();
    }
    uint16_t port = parser.isSet(portOpt)
        ? parser.value(portOpt).toUShort()
        : config.value("port", LabMonitor::DEFAULT_PORT).toUInt();
    int interval = parser.isSet(intervalOpt)
        ? parser.value(intervalOpt).toInt()
        : config.value("interval", LabMonitor::DEFAULT_CAPTURE_MS).toInt();
    int quality = parser.isSet(qualityOpt)
        ? parser.value(qualityOpt).toInt()
        : config.value("quality", LabMonitor::DEFAULT_QUALITY).toInt();
    double scale = parser.isSet(scaleOpt)
        ? parser.value(scaleOpt).toDouble()
        : config.value("scale", LabMonitor::DEFAULT_SCALE).toDouble();

    // Save config (auto-create config.ini on first run)
    config.setValue("teacher_ip", teacherIp);
    config.setValue("port", port);
    config.setValue("interval", interval);
    config.setValue("quality", quality);
    config.setValue("scale", scale);
    config.sync();

    // Check screen capture availability (Linux only — grim required)
#ifndef Q_OS_WIN
    if (!LabMonitor::ScreenCapturer::isGrimAvailable()) {
        QTextStream(stderr) << "ERROR: 'grim' is not installed. Please install grim for screen capture.\n";
        QTextStream(stderr) << "  Arch Linux: sudo pacman -S grim\n";
        QTextStream(stderr) << "  Ubuntu:     sudo apt install grim\n";
        return 1;
    }
#endif

    // Create and configure agent
    LabMonitor::StudentAgent agent;
    g_agent = &agent;

    agent.setTeacherHost(teacherIp);
    agent.setTeacherPort(port);
    agent.setCaptureInterval(interval);
    agent.setCaptureQuality(quality);
    agent.setCaptureScale(scale);

    // Signal handling for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Lock overlay (persistent, hidden until needed)
    LockOverlay lockOverlay;

    // Chat window (persistent)
    ChatWindow chatWindow(&agent);

    // ── Status logging ──
    QObject::connect(&agent, &LabMonitor::StudentAgent::connected, []() {
        QTextStream(stdout) << "[lab-student] ✓ Connected to teacher\n";
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::disconnected, [&lockOverlay]() {
        QTextStream(stdout) << "[lab-student] ✗ Disconnected from teacher\n";
        // Auto-unlock on disconnect
        if (lockOverlay.isVisible()) {
            lockOverlay.deactivate();
            QTextStream(stdout) << "[lab-student] 🔓 Auto-unlocked (disconnected)\n";
        }
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::frameSent, [](int bytes) {
        QTextStream(stdout) << "[lab-student] → Frame sent (" << bytes << " bytes)\n";
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::error, [](const QString& msg) {
        QTextStream(stderr) << "[lab-student] ERROR: " << msg << "\n";
    });

    // ── Message popup ──
    QObject::connect(&agent, &LabMonitor::StudentAgent::messageReceived,
                     [](const QString& title, const QString& body, const QString& sender) {
        QTextStream(stdout) << "[lab-student] 📩 Message from " << sender << ": " << title << "\n";
        auto* dialog = createStyledMessageDialog(title, body, sender);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });

    // ── Lock/Unlock ──
    QObject::connect(&agent, &LabMonitor::StudentAgent::lockScreenRequested,
                     [&lockOverlay]() {
        QTextStream(stdout) << "[lab-student] 🔒 Screen LOCKED\n";
        lockOverlay.activate();
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::unlockScreenRequested,
                     [&lockOverlay]() {
        QTextStream(stdout) << "[lab-student] 🔓 Screen UNLOCKED\n";
        lockOverlay.deactivate();
    });

    // ── Send URL (auto-open browser) ──
    QObject::connect(&agent, &LabMonitor::StudentAgent::urlReceived,
                     [](const QString& url) {
        QTextStream(stdout) << "[lab-student] 🌐 Opening URL: " << url << "\n";
        // QDesktopServices works cross-platform (Linux, Windows, macOS)
        if (!QDesktopServices::openUrl(QUrl(url))) {
            QTextStream(stderr) << "[lab-student] Failed to open URL\n";
        }
    });

    // ── Chat ──
    QObject::connect(&agent, &LabMonitor::StudentAgent::chatReceived,
                     [&chatWindow](const QString& sender, const QString& message) {
        chatWindow.addMessage(sender, message);
        if (!chatWindow.isVisible()) {
            chatWindow.show();
        }
        chatWindow.raise();
        chatWindow.activateWindow();
    });

    // Start
    QTextStream(stdout) << "╔══════════════════════════════════════════╗\n";
    QTextStream(stdout) << "║     Lab Monitor — Student Agent v1.0    ║\n";
    QTextStream(stdout) << "╚══════════════════════════════════════════╝\n";
    QTextStream(stdout) << "  Teacher: " << teacherIp
                        << ":" << port << "\n";
    QTextStream(stdout) << "  Config:  " << configPath << "\n";
    QTextStream(stdout) << "  Hostname: " << LabMonitor::getLocalHostname() << "\n";
    QTextStream(stdout) << "  User: " << LabMonitor::getLocalUsername() << "\n";
    QTextStream(stdout) << "\n";

    agent.start();

    return app.exec();
}

#include "main.moc"
