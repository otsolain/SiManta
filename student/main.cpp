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
#include <QSettings>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QProgressBar>
#include <QTimer>
#include <csignal>

#include "student_agent.h"
#include "protocol.h"

static LabMonitor::StudentAgent* g_agent = nullptr;
static const char* DIALOG_STYLE = R"(
    QDialog {
        background-color: #0D1117;
        color: #E6EDF3;
        border: 1px solid rgba(255,255,255,0.08);
    }
    QLabel {
        color: #E6EDF3;
        background: transparent;
    }
    QPushButton {
        color: #FFFFFF;
    }
    QTextEdit {
        color: #E6EDF3;
        background-color: #161B22;
        border: 1px solid rgba(255,255,255,0.1);
        border-radius: 8px;
        selection-background-color: #1F6FEB;
    }
    QLineEdit {
        color: #E6EDF3;
        background-color: #161B22;
        border: 1px solid rgba(255,255,255,0.1);
        border-radius: 8px;
        selection-background-color: #1F6FEB;
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
class LockOverlay : public QWidget
{
public:
    LockOverlay(QWidget* parent = nullptr) : QWidget(parent)
    {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_ShowWithoutActivating, false);
        setCursor(Qt::BlankCursor);
    }

    void activate()
    {
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
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient bgGrad(0, 0, width(), height());
        bgGrad.setColorAt(0.0, QColor("#0D1117"));
        bgGrad.setColorAt(0.5, QColor("#161B22"));
        bgGrad.setColorAt(1.0, QColor("#0D1117"));
        p.fillRect(rect(), bgGrad);
        QRadialGradient glow(width()/2, height()/2, qMin(width(), height())/2);
        glow.setColorAt(0.0, QColor(31, 111, 235, 25));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(rect(), glow);
        int circleSize = 120;
        QRect circleRect(width()/2 - circleSize/2, height()/2 - 160, circleSize, circleSize);
        QLinearGradient circleGrad(circleRect.topLeft(), circleRect.bottomRight());
        circleGrad.setColorAt(0, QColor("#1F6FEB"));
        circleGrad.setColorAt(1, QColor("#1158C7"));
        p.setBrush(circleGrad);
        p.setPen(Qt::NoPen);
        p.drawEllipse(circleRect);
        p.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        int cx = circleRect.center().x();
        int cy = circleRect.center().y();
        QRect lockBody(cx - 18, cy - 4, 36, 28);
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(lockBody, 4, 4);
        p.setPen(QPen(Qt::white, 5, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        QRect shackleRect(cx - 14, cy - 26, 28, 28);
        p.drawArc(shackleRect, 0 * 16, 180 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#1F6FEB"));
        p.drawEllipse(QPoint(cx, cy + 6), 5, 5);
        p.drawRect(cx - 2, cy + 10, 4, 8);
        QFont titleFont("Segoe UI", 28, QFont::Bold);
        p.setFont(titleFont);
        p.setPen(QColor("#E6EDF3"));
        QRect titleRect(0, height()/2 - 20, width(), 50);
        p.drawText(titleRect, Qt::AlignCenter, "Screen Locked");
        QFont subFont("Segoe UI", 13);
        p.setFont(subFont);
        p.setPen(QColor(230, 237, 243, 140));
        QRect subRect(0, height()/2 + 40, width(), 60);
        p.drawText(subRect, Qt::AlignCenter, "Your screen has been locked by the teacher.\nPlease wait for instructions.");
        QFont brandFont("Segoe UI", 9);
        p.setFont(brandFont);
        p.setPen(QColor(255, 255, 255, 60));
        QRect brandRect(0, height() - 50, width(), 30);
        p.drawText(brandRect, Qt::AlignCenter, "SiManta - Classroom Management");
    }

    void keyPressEvent(QKeyEvent* e) override {
        e->accept();
    }

    void closeEvent(QCloseEvent* e) override {
        e->ignore();
    }

    void focusOutEvent(QFocusEvent*) override {
        if (isVisible()) {
            raise();
            activateWindow();
        }
    }
};
class ChatWindow : public QDialog
{
    Q_OBJECT
public:
    ChatWindow(LabMonitor::StudentAgent* agent, QWidget* parent = nullptr)
        : QDialog(parent), m_agent(agent)
    {
        setWindowTitle("Chat with Teacher");
        setMinimumSize(420, 500);
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

        setStyleSheet(
            "QDialog { background-color: #0D1117; color: #E6EDF3; }"
            "QLabel { color: #E6EDF3; background: transparent; }"
        );

        auto* layout = new QVBoxLayout(this);
        layout->setSpacing(10);
        layout->setContentsMargins(16, 16, 16, 16);
        auto* header = new QLabel("Chat with Teacher", this);
        header->setStyleSheet(
            "font-size: 14pt; font-weight: bold; color: #58A6FF; padding: 4px;");
        m_chatLog = new QTextEdit(this);
        m_chatLog->setReadOnly(true);
        m_chatLog->setStyleSheet(
            "QTextEdit { background: #161B22; color: #E6EDF3; border: 1px solid rgba(255,255,255,0.08);"
            " border-radius: 10px; padding: 12px; font-size: 10pt; }");
        auto* inputLayout = new QHBoxLayout();
        m_input = new QLineEdit(this);
        m_input->setPlaceholderText("Type a message...");
        m_input->setStyleSheet(
            "QLineEdit { padding: 10px 14px; font-size: 10pt; color: #E6EDF3;"
            " background: #161B22; border: 1px solid rgba(255,255,255,0.1);"
            " border-radius: 8px; }"
            "QLineEdit:focus { border: 1px solid #1F6FEB; }");

        auto* sendBtn = new QPushButton("Send", this);
        sendBtn->setStyleSheet(
            "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 #1F6FEB, stop:1 #1158C7); color: white; padding: 10px 20px;"
            " border-radius: 8px; font-weight: bold; font-size: 10pt; border: none; }"
            "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 #388BFD, stop:1 #1F6FEB); }"
            "QPushButton:pressed { background: #1158C7; }");
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
        QString color = (sender == "Teacher") ? "#58A6FF" : "#8B949E";
        m_chatLog->append(QStringLiteral(
            "<p><b style='color:%1'>%2:</b> <span style='color:#E6EDF3'>%3</span></p>"
        ).arg(color, sender.toHtmlEscaped(), message.toHtmlEscaped()));

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
class FileTransferPopup : public QWidget
{
    Q_OBJECT
public:
    FileTransferPopup(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                       | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFixedSize(380, 120);
        auto* container = new QFrame(this);
        container->setGeometry(0, 0, 380, 120);
        container->setStyleSheet(
            "QFrame { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "stop:0 #161B22, stop:1 #0D1117);"
            " border: 1px solid rgba(88,166,255,0.25);"
            " border-radius: 14px; }"
        );

        auto* shadow = new QGraphicsDropShadowEffect(container);
        shadow->setBlurRadius(30);
        shadow->setColor(QColor(31, 111, 235, 80));
        shadow->setOffset(0, 4);
        container->setGraphicsEffect(shadow);

        auto* layout = new QVBoxLayout(container);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(8);
        auto* headerLayout = new QHBoxLayout();
        headerLayout->setSpacing(10);
        auto* iconLabel = new QLabel(container);
        iconLabel->setFixedSize(32, 32);
        iconLabel->setStyleSheet(
            "QLabel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "stop:0 #1F6FEB, stop:1 #388BFD);"
            " border-radius: 16px; color: white; font-size: 14pt;"
            " font-weight: bold; border: none; }"
        );
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setText(QString::fromUtf8("\u2193"));

        m_titleLabel = new QLabel("Receiving file...", container);
        m_titleLabel->setStyleSheet(
            "QLabel { color: #E6EDF3; font-size: 11pt; font-weight: bold; border: none; background: transparent; }");

        m_fileNameLabel = new QLabel("", container);
        m_fileNameLabel->setStyleSheet(
            "QLabel { color: #8B949E; font-size: 8pt; border: none; background: transparent; }");

        auto* titleCol = new QVBoxLayout();
        titleCol->setSpacing(2);
        titleCol->addWidget(m_titleLabel);
        titleCol->addWidget(m_fileNameLabel);

        headerLayout->addWidget(iconLabel);
        headerLayout->addLayout(titleCol, 1);

        m_percentLabel = new QLabel("0%", container);
        m_percentLabel->setStyleSheet(
            "QLabel { color: #58A6FF; font-size: 12pt; font-weight: bold;"
            " border: none; background: transparent; }");
        headerLayout->addWidget(m_percentLabel);

        layout->addLayout(headerLayout);
        m_progressBar = new QProgressBar(container);
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_progressBar->setFixedHeight(6);
        m_progressBar->setTextVisible(false);
        m_progressBar->setStyleSheet(
            "QProgressBar { background: #21262D; border: none; border-radius: 3px; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #1F6FEB, stop:1 #58A6FF); border-radius: 3px; }"
        );

        layout->addWidget(m_progressBar);
    }

    void showTransfer(const QString& fileName, qint64 fileSize, bool isFolder)
    {
        m_titleLabel->setText(isFolder ? "Receiving folder from Teacher..." : "Receiving file from Teacher...");
        QString sizeStr;
        if (fileSize < 1024) sizeStr = QString::number(fileSize) + " B";
        else if (fileSize < 1048576) sizeStr = QString::number(fileSize / 1024) + " KB";
        else sizeStr = QString::number(fileSize / 1048576) + " MB";
        m_fileNameLabel->setText(QStringLiteral("%1  (%2)").arg(fileName, sizeStr));
        m_progressBar->setValue(0);
        m_percentLabel->setText("0%");
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect geo = screen->availableGeometry();
            int targetX = geo.right() - width() - 20;
            int targetY = geo.bottom() - height() - 20;
            move(targetX, geo.bottom() + 10);
            show();

            auto* anim = new QPropertyAnimation(this, "pos", this);
            anim->setDuration(400);
            anim->setStartValue(pos());
            anim->setEndValue(QPoint(targetX, targetY));
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            show();
        }
    }

    void updateProgress(int percent)
    {
        m_progressBar->setValue(percent);
        m_percentLabel->setText(QStringLiteral("%1%").arg(percent));
    }

    void showComplete(const QString& fileName, bool isFolder)
    {
        m_titleLabel->setText(isFolder ? "Folder received!" : "File received!");
        m_fileNameLabel->setText(QStringLiteral("Saved to Downloads/SiManta/%1").arg(fileName));
        m_progressBar->setValue(100);
        m_percentLabel->setText(QString::fromUtf8("\u2713"));
        m_progressBar->setStyleSheet(
            "QProgressBar { background: #21262D; border: none; border-radius: 3px; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #238636, stop:1 #2EA043); border-radius: 3px; }"
        );
        QTimer::singleShot(5000, this, [this]() {
            QScreen* screen = QGuiApplication::primaryScreen();
            if (screen) {
                auto* anim = new QPropertyAnimation(this, "pos", this);
                anim->setDuration(300);
                anim->setStartValue(pos());
                anim->setEndValue(QPoint(pos().x(), screen->availableGeometry().bottom() + 10));
                anim->setEasingCurve(QEasingCurve::InCubic);
                connect(anim, &QPropertyAnimation::finished, this, &QWidget::hide);
                anim->start(QAbstractAnimation::DeleteWhenStopped);
            } else {
                hide();
            }
        });
    }

private:
    QLabel* m_titleLabel;
    QLabel* m_fileNameLabel;
    QLabel* m_percentLabel;
    QProgressBar* m_progressBar;
};
static QDialog* createStyledMessageDialog(const QString& title, const QString& body,
                                          const QString& sender)
{
    auto* dialog = new QDialog();
    dialog->setWindowTitle("Message from Teacher");
    dialog->setWindowFlags(dialog->windowFlags() | Qt::WindowStaysOnTopHint);
    dialog->setMinimumSize(460, 280);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    dialog->setStyleSheet(DIALOG_STYLE);

    auto* layout = new QVBoxLayout(dialog);
    layout->setSpacing(14);
    layout->setContentsMargins(28, 28, 28, 28);
    auto* headerLabel = new QLabel(dialog);
    headerLabel->setText(title);
    headerLabel->setStyleSheet(
        "font-size: 16pt; font-weight: bold; color: #58A6FF; padding-bottom: 4px;");
    headerLabel->setWordWrap(true);
    auto* senderLabel = new QLabel(QStringLiteral("From: %1").arg(sender), dialog);
    senderLabel->setStyleSheet("font-size: 9pt; color: #8B949E; font-style: italic;");
    auto* bodyLabel = new QLabel(dialog);
    bodyLabel->setText(body);
    bodyLabel->setWordWrap(true);
    bodyLabel->setStyleSheet(
        "font-size: 11pt; color: #E6EDF3; background: #161B22;"
        " border: 1px solid rgba(255,255,255,0.08);"
        " border-radius: 10px; padding: 16px;");
    bodyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    bodyLabel->setMinimumHeight(70);
    auto* okBtn = new QPushButton("OK", dialog);
    okBtn->setFixedSize(130, 40);
    okBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #1F6FEB, stop:1 #1158C7); color: white; padding: 8px 32px;"
        " border-radius: 8px; font-weight: bold; font-size: 10pt; border: none; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #388BFD, stop:1 #1F6FEB); }");
    okBtn->setCursor(Qt::PointingHandCursor);

    layout->addWidget(headerLabel);
    layout->addWidget(senderLabel);
    layout->addWidget(bodyLabel, 1);
    layout->addSpacing(8);
    layout->addWidget(okBtn, 0, Qt::AlignCenter);

    QObject::connect(okBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    return dialog;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("simanta-student");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("SiManta");
    QFont defaultFont("Segoe UI", 10);
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);
    QCommandLineParser parser;
    parser.setApplicationDescription("SiManta Student Agent");
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
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings config(configPath, QSettings::IniFormat);
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
    config.setValue("teacher_ip", teacherIp);
    config.setValue("port", port);
    config.setValue("interval", interval);
    config.setValue("quality", quality);
    config.setValue("scale", scale);
    config.sync();
    LabMonitor::StudentAgent agent;
    g_agent = &agent;

    agent.setTeacherHost(teacherIp);
    agent.setTeacherPort(port);
    agent.setCaptureInterval(interval);
    agent.setCaptureQuality(quality);
    agent.setCaptureScale(scale);
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    LockOverlay lockOverlay;
    ChatWindow chatWindow(&agent);
    QObject::connect(&agent, &LabMonitor::StudentAgent::connected, []() {
        QTextStream(stdout) << "[lab-student] Connected to teacher\n";
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::disconnected, [&lockOverlay]() {
        QTextStream(stdout) << "[lab-student] Disconnected from teacher\n";
        if (lockOverlay.isVisible()) {
            lockOverlay.deactivate();
            QTextStream(stdout) << "[lab-student] Auto-unlocked (disconnected)\n";
        }
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::frameSent, [](int bytes) {
        QTextStream(stdout) << "[lab-student] Frame sent (" << bytes << " bytes)\n";
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::error, [](const QString& msg) {
        QTextStream(stderr) << "[lab-student] ERROR: " << msg << "\n";
    });
    QObject::connect(&agent, &LabMonitor::StudentAgent::messageReceived,
                     [](const QString& title, const QString& body, const QString& sender) {
        QTextStream(stdout) << "[lab-student] Message from " << sender << ": " << title << "\n";
        auto* dialog = createStyledMessageDialog(title, body, sender);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
    QObject::connect(&agent, &LabMonitor::StudentAgent::lockScreenRequested,
                     [&lockOverlay]() {
        QTextStream(stdout) << "[lab-student] Screen LOCKED\n";
        lockOverlay.activate();
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::unlockScreenRequested,
                     [&lockOverlay]() {
        QTextStream(stdout) << "[lab-student] Screen UNLOCKED\n";
        lockOverlay.deactivate();
    });
    QObject::connect(&agent, &LabMonitor::StudentAgent::urlReceived,
                     [](const QString& url) {
        QTextStream(stdout) << "[lab-student] Opening URL: " << url << "\n";
        QUrl qurl(url);
        if (!qurl.isValid()) {
            QTextStream(stderr) << "[lab-student] Invalid URL\n";
            return;
        }
        if (qurl.scheme().isEmpty()) {
            qurl = QUrl("https://" + url);
        }
        if (!QDesktopServices::openUrl(qurl)) {
            QTextStream(stderr) << "[lab-student] Failed to open URL\n";
        }
    });
    QObject::connect(&agent, &LabMonitor::StudentAgent::chatReceived,
                     [&chatWindow](const QString& sender, const QString& message) {
        chatWindow.addMessage(sender, message);
        if (!chatWindow.isVisible()) {
            chatWindow.show();
        }
        chatWindow.raise();
        chatWindow.activateWindow();
    });
    FileTransferPopup filePopup;

    QObject::connect(&agent, &LabMonitor::StudentAgent::fileReceiveStarted,
                     &filePopup, [&filePopup](const QString& fileName, qint64 fileSize, bool isFolder) {
        QTextStream(stdout) << "[lab-student] Receiving file: " << fileName << "\n";
        filePopup.showTransfer(fileName, fileSize, isFolder);
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::fileReceiveProgress,
                     &filePopup, [&filePopup](const QString& fileName, int percent) {
        Q_UNUSED(fileName)
        filePopup.updateProgress(percent);
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::fileReceiveCompleted,
                     &filePopup, [&filePopup](const QString& fileName, const QString& savePath, bool isFolder) {
        Q_UNUSED(savePath)
        QTextStream(stdout) << "[lab-student] File saved: " << savePath << "\n";
        filePopup.showComplete(fileName, isFolder);
    });
    QTextStream(stdout) << "============================================\n";
    QTextStream(stdout) << "     SiManta - Student Agent v1.0\n";
    QTextStream(stdout) << "============================================\n";
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
