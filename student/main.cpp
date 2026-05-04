#include <QProcess>
#include <QStandardPaths>
#include <QApplication>
#include <QIcon>
#include <QFile>
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
#include <QFrame>
#include <QScrollArea>
#include <QPropertyAnimation>
#include <QProgressBar>
#include <QTimer>
#include <QTime>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <csignal>
#include "../common/lang.h"

#include "student_agent.h"
#include "protocol.h"
#include <windows.h>
#include <QTcpServer>
#include <QTcpSocket>

#undef DEFAULT_QUALITY

void refreshWindowsProxy() {
    HMODULE hWinInet = LoadLibraryA("wininet.dll");
    if (hWinInet) {
        typedef BOOL(WINAPI *InternetSetOptionW_t)(LPVOID, DWORD, LPVOID, DWORD);
        InternetSetOptionW_t pInternetSetOptionW = (InternetSetOptionW_t)GetProcAddress(hWinInet, "InternetSetOptionW");
        if (pInternetSetOptionW) {
            pInternetSetOptionW(NULL, 39, NULL, 0); // INTERNET_OPTION_SETTINGS_CHANGED
            pInternetSetOptionW(NULL, 37, NULL, 0); // INTERNET_OPTION_REFRESH
        }
        FreeLibrary(hWinInet);
    }
}

class PacServer : public QTcpServer {
    Q_OBJECT
public:
    QString currentPacContent;
    PacServer(QObject* parent = nullptr) : QTcpServer(parent) {}
protected:
    void incomingConnection(qintptr socketDescriptor) override {
        QTcpSocket* socket = new QTcpSocket(this);
        socket->setSocketDescriptor(socketDescriptor);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QByteArray request = socket->readAll();
            if (request.startsWith("GET ")) {
                QByteArray response = "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: application/x-ns-proxy-autoconfig\r\n"
                                      "Connection: close\r\n\r\n";
                response += currentPacContent.toUtf8();
                socket->write(response);
                socket->flush();
            }
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
};

static PacServer* g_pacServer = nullptr;
static LabMonitor::StudentAgent* g_agent = nullptr;
static const char* DIALOG_STYLE = R"(
    QDialog {
        background-color: #F0F4F8;
        color: #1A2233;
    }
    QLabel {
        color: #1A2233;
        background: transparent;
    }
    QPushButton {
        color: #FFFFFF;
    }
    QTextEdit {
        color: #1A2233;
        background-color: #FFFFFF;
        border: 1px solid rgba(0,60,120,0.12);
        border-radius: 10px;
        selection-background-color: #1A73E8;
    }
    QLineEdit {
        color: #1A2233;
        background-color: #FFFFFF;
        border: 1px solid rgba(0,60,120,0.12);
        border-radius: 10px;
        selection-background-color: #1A73E8;
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
        p.drawText(brandRect, Qt::AlignCenter, "Simanta - Classroom Management");
    }

    void keyPressEvent(QKeyEvent* e) override {
        e->accept(); // Block all keys
    }

    void closeEvent(QCloseEvent* e) override {
        e->ignore(); // Prevent closing
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
        setWindowTitle("Chat - Teacher");
        setMinimumSize(460, 520);
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        setStyleSheet("QDialog { background: #F8FAFC; }");

        auto* layout = new QVBoxLayout(this);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);

        // ── Header bar (Modern Clean) ──
        auto* headerBar = new QWidget(this);
        headerBar->setFixedHeight(68);
        headerBar->setStyleSheet("background: #FFFFFF; border-bottom: 1px solid #E2E8F0;");
        auto* hdrLayout = new QHBoxLayout(headerBar);
        hdrLayout->setContentsMargins(20, 0, 20, 0);
        hdrLayout->setSpacing(14);

        auto* avatar = new QLabel(headerBar);
        avatar->setFixedSize(42, 42);
        avatar->setText("G");
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #10B981, stop:1 #059669);"
            " color: white; font-weight: bold; font-size: 14pt; border-radius: 21px; border: none;");

        auto* nameCol = new QVBoxLayout();
        nameCol->setSpacing(2);
        nameCol->setAlignment(Qt::AlignVCenter);
        auto* nameLabel = new QLabel("Teacher", headerBar);
        nameLabel->setStyleSheet("color: #0F172A; font-size: 13pt; font-weight: 600; background: transparent; border: none;");
        auto* statusLbl = new QLabel("Online", headerBar);
        statusLbl->setStyleSheet("color: #10B981; font-size: 9pt; font-weight: 500; background: transparent; border: none;");
        nameCol->addWidget(nameLabel);
        nameCol->addWidget(statusLbl);

        hdrLayout->addWidget(avatar);
        hdrLayout->addLayout(nameCol, 1);

        // ── Chat area (QScrollArea) ──
        m_chatScroll = new QScrollArea(this);
        m_chatScroll->setWidgetResizable(true);
        m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_chatScroll->setStyleSheet(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollBar:vertical { width: 6px; background: transparent; }"
            "QScrollBar::handle:vertical { background: rgba(15,23,42,0.15); border-radius: 3px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
        m_chatContainer = new QWidget();
        m_chatContainer->setStyleSheet("background: transparent;");
        m_chatLayout = new QVBoxLayout(m_chatContainer);
        m_chatLayout->setContentsMargins(16, 16, 16, 16);
        m_chatLayout->setSpacing(12);
        m_chatLayout->addStretch();
        m_chatScroll->setWidget(m_chatContainer);

        // ── Input area ──
        auto* inputBar = new QWidget(this);
        inputBar->setFixedHeight(72);
        inputBar->setStyleSheet("background: #FFFFFF; border-top: 1px solid #E2E8F0;");
        auto* inputLayout = new QHBoxLayout(inputBar);
        inputLayout->setContentsMargins(16, 12, 16, 12);
        inputLayout->setSpacing(12);

        m_input = new QLineEdit(inputBar);
        m_input->setPlaceholderText("Ketik pesan...");
        m_input->setStyleSheet(
            "QLineEdit { padding: 10px 16px; font-size: 10pt; color: #0F172A;"
            " background: #F1F5F9; border: 1px solid transparent;"
            " border-radius: 20px; }"
            "QLineEdit:focus { border: 1px solid #3B82F6; background: #FFFFFF; }");

        auto* sendBtn = new QPushButton("Send", inputBar);
        sendBtn->setFixedSize(76, 40);
        sendBtn->setStyleSheet(
            "QPushButton { background: #3B82F6; color: white; border-radius: 20px;"
            " font-weight: 600; font-size: 10pt; border: none; }"
            "QPushButton:hover { background: #2563EB; }");
        sendBtn->setCursor(Qt::PointingHandCursor);

        inputLayout->addWidget(m_input, 1);
        inputLayout->addWidget(sendBtn);

        layout->addWidget(headerBar);
        layout->addWidget(m_chatScroll, 1);
        layout->addWidget(inputBar);

        connect(sendBtn, &QPushButton::clicked, this, &ChatWindow::onSend);
        connect(m_input, &QLineEdit::returnPressed, this, &ChatWindow::onSend);
    }

    void addMessage(const QString& sender, const QString& message)
    {
        bool isMe = (sender == "Me");
        QString time = QTime::currentTime().toString("HH:mm");
        QString displayName = isMe ? "You" : sender;

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

        auto* msgLbl = new QLabel(message, bubble);
        msgLbl->setWordWrap(true);
        msgLbl->setStyleSheet(QStringLiteral(
            "font-size: 10pt; color: %1; background: transparent; border: none;")
            .arg(isMe ? "#FFFFFF" : "#1E293B"));

        auto* timeLbl = new QLabel(time, bubble);
        timeLbl->setAlignment(Qt::AlignRight);
        timeLbl->setStyleSheet(QStringLiteral(
            "font-size: 8pt; color: %1; background: transparent; border: none;")
            .arg(isMe ? "rgba(255,255,255,0.7)" : "#94A3B8"));

        bLayout->addWidget(msgLbl);
        bLayout->addWidget(timeLbl);

        if (isMe) {
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

        m_chatLayout->insertLayout(m_chatLayout->count() - 1, row);

        QTimer::singleShot(20, m_chatScroll, [this]() {
            m_chatScroll->verticalScrollBar()->setValue(
                m_chatScroll->verticalScrollBar()->maximum());
        });
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
    QScrollArea* m_chatScroll;
    QWidget* m_chatContainer;
    QVBoxLayout* m_chatLayout;
    QLineEdit* m_input;
};

class StudentPanel : public QWidget
{
    Q_OBJECT
public:
    StudentPanel(LabMonitor::StudentAgent* agent, ChatWindow* chatWindow, QWidget* parent = nullptr)
        : QWidget(parent), m_agent(agent), m_chatWindow(chatWindow), m_expanded(false), m_unreadCount(0),
          m_dragging(false), m_manuallyPositioned(false)
    {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setCursor(Qt::OpenHandCursor);
        setFixedWidth(300); // Reset width since shadow margins are removed
        setStyleSheet("StudentPanel { background: rgba(0,0,0,0); border: 0px solid transparent; margin: 0px; padding: 0px; }");

        // ── Main container ──
        m_container = new QFrame(this);
        m_container->setStyleSheet(
            "QFrame#PanelContainer {"
            "  background: #FFFFFF;"
            "  border: 1px solid #CBD5E1;"
            "  margin: 0px;"
            "  padding: 0px;"
            "  border-radius: 0px;"
            "}"
        );
        m_container->setObjectName("PanelContainer");

        auto* mainLayout = new QVBoxLayout(m_container);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        auto* headerWidget = new QWidget(m_container);
        headerWidget->setStyleSheet("background: transparent;");
        headerWidget->setCursor(Qt::PointingHandCursor);
        auto* headerLayout = new QHBoxLayout(headerWidget);
        headerLayout->setContentsMargins(18, 12, 14, 12); // Pushed back to the left with safe padding
        headerLayout->setSpacing(10);

        m_monitorDot = new QLabel(headerWidget);
        m_monitorDot->setFixedSize(14, 14); // Slightly larger
        m_monitorDot->setStyleSheet(
            "QLabel { background: #3FB950; border-radius: 7px; border: none; }");

        m_statusLabel = new QLabel(Lang::get().t("Connecting...", "Menghubungkan..."), headerWidget);
        m_statusLabel->setStyleSheet(
            "QLabel { color: #1E293B; font-size: 9pt; font-weight: 600;"
            " background: transparent; border: none; }");

        m_expandBtn = new QPushButton(headerWidget);
        m_expandBtn->setFixedSize(28, 28);
        m_expandBtn->setText("-");
        m_expandBtn->setStyleSheet(
            "QPushButton { background: #F1F5F9; color: #64748B;"
            " border-radius: 14px; font-size: 14pt; border: none; font-weight: bold; padding-bottom: 2px; }"
            "QPushButton:hover { background: #E2E8F0; color: #0F172A; }");
        m_expandBtn->setCursor(Qt::PointingHandCursor);

        // Unread badge on header
        m_unreadBadge = new QLabel(headerWidget);
        m_unreadBadge->setFixedSize(22, 22);
        m_unreadBadge->setAlignment(Qt::AlignCenter);
        m_unreadBadge->setStyleSheet(
            "QLabel { background: #EA4335; color: white; font-size: 8pt;"
            " font-weight: bold; border-radius: 11px; border: none; }");
        m_unreadBadge->hide();

        headerLayout->addWidget(m_monitorDot);
        headerLayout->addWidget(m_statusLabel, 1);
        headerLayout->addWidget(m_unreadBadge);
        headerLayout->addWidget(m_expandBtn);

        m_bodyWidget = new QWidget(m_container);
        m_bodyWidget->setStyleSheet("background: transparent;");
        auto* bodyLayout = new QVBoxLayout(m_bodyWidget);
        bodyLayout->setContentsMargins(14, 4, 14, 14);
        bodyLayout->setSpacing(8);

        // Monitor text strip removed as requested

        // ── Action Buttons ──
        m_actionsLabel = new QLabel(Lang::get().t("Quick Actions", "Aksi Cepat"), m_bodyWidget);
        m_actionsLabel->setStyleSheet(
            "QLabel { color: #64748B; font-size: 8pt; font-weight: 600;"
            " letter-spacing: 1px; background: transparent; border: none; }");
        bodyLayout->addWidget(m_actionsLabel);

        // Chat button (main action)
        m_chatBtn = new QPushButton(m_bodyWidget);
        m_chatBtn->setText(Lang::get().t("Chat with Teacher", "Chat dengan Guru"));
        m_chatBtn->setFixedHeight(40);
        m_chatBtn->setStyleSheet(
            "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "  stop:0 #1A73E8, stop:1 #4DA3FF);"
            " color: white; border-radius: 10px; font-weight: bold;"
            " font-size: 10pt; border: none; text-align: center; }"
            "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "  stop:0 #4DA3FF, stop:1 #79BFFF); }"
            "QPushButton:disabled { background: #CBD5E1; color: #94A3B8; cursor: not-allowed; }");
        m_chatBtn->setCursor(Qt::PointingHandCursor);
        bodyLayout->addWidget(m_chatBtn);

        // Quick request buttons row
        auto* reqRow1 = new QHBoxLayout();
        reqRow1->setSpacing(6);

        auto createReqBtn = [this](const QString& label) -> QPushButton* {
            auto* btn = new QPushButton(m_bodyWidget);
            btn->setText(label);
            btn->setFixedHeight(36);
            btn->setStyleSheet(
                "QPushButton { background: #F8FAFC;"
                " color: #334155; border-radius: 8px; font-size: 9pt;"
                " border: 1px solid #E2E8F0; }"
                "QPushButton:hover { background: #F1F5F9;"
                " color: #0F172A; border: 1px solid #CBD5E1; }"
                "QPushButton:disabled { background: #F1F5F9; color: #CBD5E1; border: 1px solid #E2E8F0; cursor: not-allowed; }");
            btn->setCursor(Qt::PointingHandCursor);
            return btn;
        };

        m_helpBtn = createReqBtn(Lang::get().t("Need Help", "Butuh Bantuan"));
        m_questionBtn = createReqBtn(Lang::get().t("Question", "Ada Pertanyaan"));
        reqRow1->addWidget(m_helpBtn);
        reqRow1->addWidget(m_questionBtn);
        bodyLayout->addLayout(reqRow1);

        auto* reqRow2 = new QHBoxLayout();
        reqRow2->setSpacing(6);
        m_toiletBtn = createReqBtn(Lang::get().t("Toilet Rest", "Izin ke Toilet"));
        m_doneBtn = createReqBtn(Lang::get().t("Finished", "Sudah Selesai"));
        reqRow2->addWidget(m_toiletBtn);
        reqRow2->addWidget(m_doneBtn);
        bodyLayout->addLayout(reqRow2);

        // Connection info footer
        m_connInfoLabel = new QLabel("", m_bodyWidget);
        m_connInfoLabel->setAlignment(Qt::AlignCenter);
        m_connInfoLabel->setStyleSheet(
            "QLabel { color: #94A3B8; font-size: 7pt; background: transparent;"
            " border: none; padding-top: 4px; }");
        bodyLayout->addWidget(m_connInfoLabel);

        // Initially hide body (collapsed)
        m_bodyWidget->hide();

        mainLayout->addWidget(headerWidget);
        mainLayout->addWidget(m_bodyWidget);

        // ── Container layout in this widget ──
        auto* wLayout = new QVBoxLayout(this);
        wLayout->setContentsMargins(0, 0, 0, 0); // No margins needed without shadow
        wLayout->addWidget(m_container);

        // ── Connections ──
        connect(m_expandBtn, &QPushButton::clicked, this, &StudentPanel::toggleExpanded);

        connect(m_chatBtn, &QPushButton::clicked, this, [this]() {
            if (m_chatWindow) {
                m_chatWindow->show();
                m_chatWindow->raise();
                m_chatWindow->activateWindow();
            }
            m_unreadCount = 0;
            m_unreadBadge->hide();
        });

        connect(m_helpBtn, &QPushButton::clicked, this, [this]() {
            sendQuickRequest("Need help");
        });

        connect(m_questionBtn, &QPushButton::clicked, this, [this]() {
            sendQuickRequest("Have a question");
        });

        connect(m_toiletBtn, &QPushButton::clicked, this, [this]() {
            sendQuickRequest("Going to toilet");
        });

        connect(m_doneBtn, &QPushButton::clicked, this, [this]() {
            sendQuickRequest("Finished the assignment");
        });

        updateLayout();

        // Periodic check to make sure panel stays visible
        m_visibilityTimer = new QTimer(this);
        m_visibilityTimer->setInterval(3000);
        connect(m_visibilityTimer, &QTimer::timeout, this, [this]() {
            if (!isVisible()) {
                show();
                positionOnScreen();
            }
        });
        m_visibilityTimer->start();
    }

    void updateTranslations() {
        m_actionsLabel->setText(Lang::get().t("Quick Actions", "Aksi Cepat"));
        m_chatBtn->setText(Lang::get().t("Chat with Teacher", "Chat dengan Guru"));
        m_helpBtn->setText(Lang::get().t("Need Help", "Butuh Bantuan"));
        m_questionBtn->setText(Lang::get().t("Question", "Ada Pertanyaan"));
        m_toiletBtn->setText(Lang::get().t("Toilet Rest", "Izin ke Toilet"));
        m_doneBtn->setText(Lang::get().t("Finished", "Sudah Selesai"));
        // Re-apply connection texts
        setConnected(m_isConnected);
    }

    void setConnected(bool connected)
    {
        m_isConnected = connected;
        if (m_chatBtn) m_chatBtn->setEnabled(connected);
        if (m_helpBtn) m_helpBtn->setEnabled(connected);
        if (m_questionBtn) m_questionBtn->setEnabled(connected);
        if (m_toiletBtn) m_toiletBtn->setEnabled(connected);
        if (m_doneBtn) m_doneBtn->setEnabled(connected);

        if (connected) {
            m_statusLabel->setText(Lang::get().t("Being Monitored", "Sedang Diawasi"));
            m_statusLabel->setStyleSheet(
                "QLabel { color: #10B981; font-size: 9pt; font-weight: 600;"
                " background: transparent; border: none; }");
            m_monitorDot->setStyleSheet(
                "QLabel { background: #10B981; border-radius: 7px; border: none; }");
            m_connInfoLabel->setText(Lang::get().t("Connected to Teacher", "Terhubung dengan Guru"));
        } else {
            m_statusLabel->setText(Lang::get().t("Disconnected - Reconnecting...", "Terputus - Menghubungkan ulang..."));
            m_statusLabel->setStyleSheet(
                "QLabel { color: #F59E0B; font-size: 9pt; font-weight: 600;"
                " background: transparent; border: none; }");
            m_monitorDot->setStyleSheet(
                "QLabel { background: #F59E0B; border-radius: 7px; border: none; }");
            m_connInfoLabel->setText(Lang::get().t("Not connected", "Tidak terhubung"));
        }
    }

    void onChatReceived()
    {
        if (m_chatWindow && !m_chatWindow->isVisible()) {
            m_unreadCount++;
            m_unreadBadge->setText(QString::number(m_unreadCount));
            m_unreadBadge->show();
        }
    }

    void positionOnScreen()
    {
        if (m_manuallyPositioned) return; // Don't override user's drag position
        QScreen* screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        QRect geo = screen->availableGeometry();
        int x = geo.right() - width() - 12;
        int y = geo.bottom() - height() - 12;
        move(x, y);
    }

    void resetPosition()
    {
        m_manuallyPositioned = false;
        positionOnScreen();
    }

protected:
    void closeEvent(QCloseEvent* e) override {
        e->ignore(); // Prevent closing — panel must stay visible
    }

    void paintEvent(QPaintEvent*) override {
        // Do nothing! This prevents the parent widget from drawing a default
        // background, which fixes the gray corner artifacts caused by the
        // child container's border-radius.
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragStartPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
            setCursor(Qt::ClosedHandCursor);
            e->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (m_dragging && (e->buttons() & Qt::LeftButton)) {
            move(e->globalPosition().toPoint() - m_dragStartPos);
            m_manuallyPositioned = true;
            e->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = false;
            setCursor(Qt::OpenHandCursor);
            e->accept();
        }
    }

private slots:
    void toggleExpanded()
    {
        QPoint oldPos = pos();
        int oldHeight = height();

        m_expanded = !m_expanded;
        m_bodyWidget->setVisible(m_expanded);
        m_expandBtn->setText(m_expanded ? "-" : "+");
        updateLayout();

        // After resizing, ensure the panel stays within screen bounds
        // and pin to bottom if it was in the lower half of the screen
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect geo = screen->availableGeometry();
            
            if (oldPos.y() + oldHeight > geo.center().y()) {
                // Pin bottom
                int newY = oldPos.y() + oldHeight - height();
                move(oldPos.x(), newY);
            }
            
            QPoint p = pos();
            // Clamp: don't go below the bottom of the screen
            if (p.y() + height() > geo.bottom())
                p.setY(geo.bottom() - height());
            // Clamp: don't go above the top
            if (p.y() < geo.top())
                p.setY(geo.top());
            if (p != pos())
                move(p);
        }
    }

private:
    void updateLayout()
    {
        // Release fixed height constraint so sizeHint() can recalculate freely
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        m_container->adjustSize();
        adjustSize();
        setFixedHeight(sizeHint().height());
    }

    void sendQuickRequest(const QString& message)
    {
        if (!m_agent) return;
        m_agent->sendHelpRequest(message);

        // Show confirmation feedback
        auto* feedback = new QLabel(Lang::get().t("Request sent!", "Permintaan terkirim!"), this);
        feedback->setStyleSheet(
            "QLabel { color: #3FB950; font-size: 9pt; font-weight: bold;"
            " background: rgba(63,185,80,0.15); border: 1px solid rgba(63,185,80,0.3);"
            " border-radius: 8px; padding: 6px 12px; }");
        feedback->setAlignment(Qt::AlignCenter);
        feedback->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        feedback->setAttribute(Qt::WA_TranslucentBackground, false);
        feedback->setAttribute(Qt::WA_DeleteOnClose);
        feedback->adjustSize();

        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect geo = screen->availableGeometry();
            feedback->move(geo.right() - feedback->width() - 24,
                          pos().y() - feedback->height() - 8);
        }
        feedback->show();

        QTimer::singleShot(2500, feedback, &QLabel::close);
    }

    LabMonitor::StudentAgent* m_agent;
    ChatWindow* m_chatWindow;
    QFrame* m_container;
    QLabel* m_monitorDot;
    QLabel* m_statusLabel;
    QLabel* m_unreadBadge;
    QLabel* m_connInfoLabel;
    QPushButton* m_expandBtn;
    QWidget* m_bodyWidget;
    QPushButton* m_chatBtn;
    QLabel* m_actionsLabel;
    QPushButton* m_helpBtn;
    QPushButton* m_questionBtn;
    QPushButton* m_toiletBtn;
    QPushButton* m_doneBtn;
    bool m_expanded;
    bool m_isConnected = false;
    int m_unreadCount;
    QTimer* m_visibilityTimer;
    bool m_dragging;
    QPoint m_dragStartPos;
    bool m_manuallyPositioned;
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
        iconLabel->setText(QString::fromUtf8("\u2193")); // down arrow ↓

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
        m_percentLabel->setText(QString::fromUtf8("\u2713")); // ✓
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
    dialog->setMinimumSize(480, 300);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setStyleSheet(DIALOG_STYLE);

    auto* layout = new QVBoxLayout(dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    // Blue header bar
    auto* headerBar = new QWidget(dialog);
    headerBar->setStyleSheet(
        "background: #1A56A0; border-radius: 10px;");
    auto* hdrLayout = new QVBoxLayout(headerBar);
    hdrLayout->setContentsMargins(18, 14, 18, 14);
    auto* headerLabel = new QLabel(title, headerBar);
    headerLabel->setStyleSheet(
        "font-size: 15pt; font-weight: bold; color: #FFFFFF; background: transparent;");
    headerLabel->setWordWrap(true);
    auto* senderLabel = new QLabel(QStringLiteral("From: %1").arg(sender), headerBar);
    senderLabel->setStyleSheet(
        "font-size: 9pt; color: rgba(255,255,255,0.7); font-style: italic; background: transparent;");
    hdrLayout->addWidget(headerLabel);
    hdrLayout->addWidget(senderLabel);

    // Message body card
    auto* bodyCard = new QWidget(dialog);
    bodyCard->setStyleSheet(
        "background: #FFFFFF; border: 1px solid rgba(0,60,120,0.1); border-radius: 10px;");
    auto* bodyLayout = new QVBoxLayout(bodyCard);
    bodyLayout->setContentsMargins(18, 16, 18, 16);
    auto* bodyLabel = new QLabel(body, bodyCard);
    bodyLabel->setWordWrap(true);
    bodyLabel->setStyleSheet(
        "font-size: 11pt; color: #1A2233; background: transparent; border: none;");
    bodyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    bodyLayout->addWidget(bodyLabel);

    auto* okBtn = new QPushButton("OK", dialog);
    okBtn->setFixedSize(140, 42);
    okBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #1A73E8, stop:1 #1558B8); color: white; padding: 8px 32px;"
        " border-radius: 10px; font-weight: bold; font-size: 10pt; border: none; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #4DA3FF, stop:1 #1A73E8); }");
    okBtn->setCursor(Qt::PointingHandCursor);

    layout->addWidget(headerBar);
    layout->addWidget(bodyCard, 1);
    layout->addSpacing(4);
    layout->addWidget(okBtn, 0, Qt::AlignCenter);

    QObject::connect(okBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    return dialog;
}

int main(int argc, char* argv[])
{
    // Force light title bars — ignore Windows 11 dark mode
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=0");

    QApplication app(argc, argv);
    app.setApplicationName("simanta-student");
    app.setApplicationVersion("");
    app.setOrganizationName("Simanta");
    
    // Set application icon using .ico for proper Windows taskbar scaling
    QString icoPath = QCoreApplication::applicationDirPath() + "/logo.ico";
    if (QFile::exists(icoPath)) {
        app.setWindowIcon(QIcon(icoPath));
    }

    // Single instance check using Windows Mutex (auto-released on crash/kill)
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\SimantaStudentMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        // Silently exit — don't show message box (might block auto-start)
        return 1;
    }

    QFont defaultFont("Segoe UI", 10);
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);
    QCommandLineParser parser;
    parser.setApplicationDescription("Simanta Student Agent");
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

    // Determine if we should use auto-discovery
    // Auto-discovery is enabled when no explicit teacher IP is configured
    bool useAutoDiscovery = (teacherIp.isEmpty() || teacherIp == "127.0.0.1"
                             || teacherIp == "auto");

    LabMonitor::StudentAgent agent;
    g_agent = &agent;

    agent.setTeacherHost(teacherIp);
    agent.setTeacherPort(port);
    agent.setCaptureInterval(interval);
    agent.setCaptureQuality(quality);
    agent.setCaptureScale(scale);
    agent.setAutoDiscovery(useAutoDiscovery);
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    LockOverlay lockOverlay;
    ChatWindow chatWindow(&agent);
    FileTransferPopup filePopup;
    StudentPanel studentPanel(&agent, &chatWindow);

    // Update panel status to show discovery mode
    if (useAutoDiscovery) {
        studentPanel.setConnected(false);  // will show "Menghubungkan..."
    }

    // ════════════════════════════════════════════════════════
    // System Tray Icon — student can initiate chat & help
    // ════════════════════════════════════════════════════════
    QSystemTrayIcon trayIcon;
    trayIcon.setToolTip("Simanta Student - Connecting...");

    // Use app icon if available, otherwise create a simple colored icon
    QPixmap trayPx(32, 32);
    trayPx.fill(Qt::transparent);
    {
        QPainter p(&trayPx);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#1A73E8"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(2, 2, 28, 28);
        p.setPen(Qt::white);
        p.setFont(QFont("Segoe UI", 14, QFont::Bold));
        p.drawText(QRect(0, 0, 32, 32), Qt::AlignCenter, "S");
    }
    trayIcon.setIcon(QIcon(trayPx));

    QMenu trayMenu;
    auto* chatAction = trayMenu.addAction("Chat with Teacher");
    auto* helpAction = trayMenu.addAction("Request Help");
    trayMenu.addSeparator();
    auto* panelAction = trayMenu.addAction("Show Panel");
    auto* statusAction = trayMenu.addAction("Status: Connecting...");
    statusAction->setEnabled(false);
    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    // Chat action — open chat window
    QObject::connect(chatAction, &QAction::triggered, [&chatWindow]() {
        chatWindow.show();
        chatWindow.raise();
        chatWindow.activateWindow();
    });

    // Help request action — popup dialog to type help message
    QObject::connect(helpAction, &QAction::triggered, [&agent]() {
        bool ok;
        QString msg = QInputDialog::getMultiLineText(
            nullptr, "Request Help",
            "Describe your problem to the teacher:",
            "", &ok);
        if (ok && !msg.trimmed().isEmpty()) {
            agent.sendHelpRequest(msg.trimmed());
            QMessageBox::information(nullptr, "Help Request Sent",
                "Your help request has been sent to the teacher.");
        }
    });

    // Panel action — show/raise the floating panel
    QObject::connect(panelAction, &QAction::triggered, [&studentPanel]() {
        studentPanel.show();
        studentPanel.raise();
        studentPanel.positionOnScreen();
    });

    // Tray icon double-click opens chat
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated,
                     [&chatWindow](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            chatWindow.show();
            chatWindow.raise();
            chatWindow.activateWindow();
        }
    });

    // ════════════════════════════════════════════════════════
    // Agent signal connections
    // ════════════════════════════════════════════════════════
    QObject::connect(&agent, &LabMonitor::StudentAgent::connected,
                     [&trayIcon, &statusAction, &studentPanel]() {
        QTextStream(stdout) << "[lab-student] Connected to teacher\n";
        trayIcon.setToolTip("Simanta Student - Connected");
        statusAction->setText("Status: Connected");
        trayIcon.showMessage("Simanta", "Connected to teacher",
                             QSystemTrayIcon::Information, 2000);
        studentPanel.setConnected(true);
    });

    // Teacher discovered via UDP beacon
    QObject::connect(&agent, &LabMonitor::StudentAgent::teacherDiscovered,
                     [&trayIcon](const QString& ip, uint16_t port, const QString& hostname) {
        Q_UNUSED(port)
        QTextStream(stdout) << "[lab-student] Teacher discovered: " << hostname
                            << " at " << ip << "\n";
        trayIcon.showMessage("Simanta", QStringLiteral("Teacher found: %1 (%2)").arg(hostname, ip),
                             QSystemTrayIcon::Information, 3000);
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::disconnected,
                     [&lockOverlay, &trayIcon, &statusAction, &studentPanel]() {
        QTextStream(stdout) << "[lab-student] Disconnected from teacher\n";
        trayIcon.setToolTip("Simanta Student - Reconnecting...");
        statusAction->setText("Status: Reconnecting...");
        studentPanel.setConnected(false);
        if (lockOverlay.isVisible()) {
            lockOverlay.deactivate();
            QTextStream(stdout) << "[lab-student] Auto-unlocked (disconnected)\n";
        }
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::error, [](const QString& msg) {
        QTextStream(stderr) << "[lab-student] ERROR: " << msg << "\n";
    });
    QObject::connect(&agent, &LabMonitor::StudentAgent::messageReceived,
                     [&trayIcon](const QString& title, const QString& body, const QString& sender) {
        QTextStream(stdout) << "[lab-student] Message from " << sender << ": " << title << "\n";
        trayIcon.showMessage(title, body, QSystemTrayIcon::Information, 5000);
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
                     [&chatWindow, &trayIcon, &studentPanel](const QString& sender, const QString& message) {
        chatWindow.addMessage(sender, message);
        studentPanel.onChatReceived();
        trayIcon.showMessage("Chat from " + sender, message,
                             QSystemTrayIcon::Information, 3000);
    });

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

    // Initialize PAC Server
    g_pacServer = new PacServer(&app);
    if (!g_pacServer->listen(QHostAddress::LocalHost, 29999)) {
        qWarning() << "Failed to start PAC server on port 29999";
    }

    QObject::connect(&agent, &LabMonitor::StudentAgent::cmdReceived,
                     [&studentPanel](const QString& cmd) {
        QTextStream(stdout) << "[lab-student] Executing command: " << cmd << "\n";
        if (cmd.startsWith("SET_LANG:")) {
            QString lang = cmd.mid(9);
            Lang::get().setLanguage(lang);
            studentPanel.updateTranslations();
        } else if (cmd == "USB_BLOCK") {
            QProcess::startDetached("powershell", {"-Command", "Set-ItemProperty -Path 'HKLM:\\System\\CurrentControlSet\\Services\\USBSTOR' -Name 'Start' -Value 4"});
        } else if (cmd == "USB_UNBLOCK") {
            QProcess::startDetached("powershell", {"-Command", "Set-ItemProperty -Path 'HKLM:\\System\\CurrentControlSet\\Services\\USBSTOR' -Name 'Start' -Value 3"});
        } else if (cmd == "TASKMGR_BLOCK") {
            QProcess::startDetached("powershell", {"-Command", "New-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableTaskMgr' -Value 1 -PropertyType DWord -Force; New-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableTaskMgr' -Value 1 -PropertyType DWord -Force"});
        } else if (cmd == "TASKMGR_UNBLOCK") {
            QProcess::startDetached("powershell", {"-Command", "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableTaskMgr' -ErrorAction SilentlyContinue; Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableTaskMgr' -ErrorAction SilentlyContinue"});
        } else if (cmd == "REGEDIT_BLOCK") {
            QProcess::startDetached("powershell", {"-Command", "New-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableRegistryTools' -Value 1 -PropertyType DWord -Force; New-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableRegistryTools' -Value 1 -PropertyType DWord -Force"});
        } else if (cmd == "REGEDIT_UNBLOCK") {
            QProcess::startDetached("powershell", {"-Command", "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableRegistryTools' -ErrorAction SilentlyContinue; Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableRegistryTools' -ErrorAction SilentlyContinue"});
        } else if (cmd == "SETTINGS_BLOCK") {
            QProcess::startDetached("powershell", {"-Command", "New-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer' -Name 'NoControlPanel' -Value 1 -PropertyType DWord -Force; New-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer' -Name 'NoControlPanel' -Value 1 -PropertyType DWord -Force"});
        } else if (cmd == "SETTINGS_UNBLOCK") {
            QProcess::startDetached("powershell", {"-Command", "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer' -Name 'NoControlPanel' -ErrorAction SilentlyContinue; Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer' -Name 'NoControlPanel' -ErrorAction SilentlyContinue"});
        } else if (cmd == "INTERNET_BLOCK") {
            QString pacContent = "function FindProxyForURL(url, host) {\n"
                                 "  if (host == \"localhost\" || host == \"127.0.0.1\") return \"DIRECT\";\n"
                                 "  return \"PROXY 127.0.0.1:9999\";\n"
                                 "}\n";
            if (g_pacServer) g_pacServer->currentPacContent = pacContent;
            
            QString fileUrl = "http://127.0.0.1:29999/proxy.pac";
            QString psCmd = "New-Item -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Force -ErrorAction SilentlyContinue | Out-Null; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'EnableLegacyAutoProxyFeatures' -Value 1 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxySettingsPerUser' -Value 0 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -Value '" + fileUrl + "' -Force; "
                "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -Value '" + fileUrl + "' -Force; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -PropertyType DWord -Force";
            QProcess::startDetached("powershell", {"-Command", psCmd});
            QTimer::singleShot(500, []() { refreshWindowsProxy(); });
        } else if (cmd == "INTERNET_UNBLOCK") {
            QProcess::startDetached("powershell", {"-Command", 
                "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxySettingsPerUser' -ErrorAction SilentlyContinue; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -PropertyType DWord -Force; "
                "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue; "
                "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue"
            });
            QTimer::singleShot(500, []() { refreshWindowsProxy(); });
        } else if (cmd.startsWith("WHITELIST_SET:")) {
            QString domainsPart = cmd.mid(14);
            QStringList domains = domainsPart.split(",", Qt::SkipEmptyParts);
            
            QString pacContent = "function FindProxyForURL(url, host) {\n";
            for (const QString& domain : domains) {
                QString d = domain.trimmed();
                if (d.startsWith("https://")) d = d.mid(8);
                if (d.startsWith("http://")) d = d.mid(7);
                while (d.endsWith("/")) d.chop(1);
                pacContent += QString("  if (dnsDomainIs(host, \"%1\") || host == \"%1\") return \"DIRECT\";\n").arg(d);
                if (!d.startsWith("*.")) pacContent += QString("  if (dnsDomainIs(host, \".%1\")) return \"DIRECT\";\n").arg(d);
            }
            pacContent += "  if (host == \"localhost\" || host == \"127.0.0.1\") return \"DIRECT\";\n";
            pacContent += "  return \"PROXY 127.0.0.1:9999\";\n}\n";
            
            if (g_pacServer) g_pacServer->currentPacContent = pacContent;
            
            QString fileUrl = "http://127.0.0.1:29999/proxy.pac";
            QString psCmd = "New-Item -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Force -ErrorAction SilentlyContinue | Out-Null; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'EnableLegacyAutoProxyFeatures' -Value 1 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxySettingsPerUser' -Value 0 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -Value '" + fileUrl + "' -Force; "
                "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -Value '" + fileUrl + "' -Force; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -PropertyType DWord -Force";
            QProcess::startDetached("powershell", {"-Command", psCmd});
            QTimer::singleShot(500, []() { refreshWindowsProxy(); });
            QTextStream(stdout) << "[lab-student] Whitelist PAC HTTP Server applied with " << domains.size() << " domains\n";
        } else if (cmd == "WHITELIST_CLEAR") {
            if (g_pacServer) g_pacServer->currentPacContent = "";
            QProcess::startDetached("powershell", {"-Command", 
                "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'EnableLegacyAutoProxyFeatures' -ErrorAction SilentlyContinue; "
                "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue; "
                "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -PropertyType DWord -Force"
            });
            QTimer::singleShot(500, []() { refreshWindowsProxy(); });
            QTextStream(stdout) << "[lab-student] Whitelist cleared\n";
        }
    });

    // ── KICK handling: teacher forcefully disconnected this student ──
    QObject::connect(&agent, &LabMonitor::StudentAgent::kicked,
                     [&agent, &lockOverlay, &trayIcon, &statusAction, &studentPanel]() {
        QTextStream(stdout) << "[lab-student] KICKED by teacher\n";
        trayIcon.setToolTip("Simanta Student - Disconnected by Teacher");
        statusAction->setText("Status: Disconnected by Teacher");
        trayIcon.showMessage("Simanta", "You have been disconnected by the teacher.\nReconnecting in 30 seconds...",
                             QSystemTrayIcon::Warning, 5000);
        studentPanel.setConnected(false);
        if (lockOverlay.isVisible()) {
            lockOverlay.deactivate();
        }

        // Restart the agent after 30 seconds so student can reconnect later
        QTimer::singleShot(30000, &agent, [&agent]() {
            QTextStream(stdout) << "[lab-student] Attempting to reconnect after kick...\n";
            agent.start();
        });
    });
    QTextStream(stdout) << "============================================\n";
    QTextStream(stdout) << "     Simanta - Student Agent\n";
    QTextStream(stdout) << "============================================\n";
    if (useAutoDiscovery) {
        QTextStream(stdout) << "  Mode: Auto-Discovery (UDP beacon)\n";
        QTextStream(stdout) << "  Discovery Port: " << LabMonitor::DISCOVERY_PORT << "\n";
    } else {
        QTextStream(stdout) << "  Teacher: " << teacherIp
                            << ":" << port << "\n";
    }
    QTextStream(stdout) << "  Config:  " << configPath << "\n";
    QTextStream(stdout) << "  Hostname: " << LabMonitor::getLocalHostname() << "\n";
    QTextStream(stdout) << "  User: " << LabMonitor::getLocalUsername() << "\n";
    QTextStream(stdout) << "\n";

    // Show the floating student panel
    studentPanel.show();
    studentPanel.positionOnScreen();

    agent.start();

    return app.exec();
}

#include "main.moc"


