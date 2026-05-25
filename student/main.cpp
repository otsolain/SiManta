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
#include <QSet>
#include <QAbstractNativeEventFilter>
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

// Set to true when the OS is shutting down / logging off / app is quitting
// for a legitimate reason. Used by LockOverlay and StudentPanel to allow
// themselves to be closed so Windows can finish its shutdown cleanly instead
// of showing "This app is preventing shutdown" for every window.
static bool g_shuttingDown = false;

// -- Local TCP "blackhole" proxy on 127.0.0.1:9999 --
// When the PAC file routes non-whitelisted traffic to "PROXY 127.0.0.1:9999",
// this server accepts the connection and immediately closes it, giving the
// browser a fast, deterministic "connection refused" instead of a long hang.
class BlackholeProxy : public QTcpServer {
public:
    explicit BlackholeProxy(QObject* parent = nullptr) : QTcpServer(parent) {}
protected:
    void incomingConnection(qintptr sd) override {
        QTcpSocket* s = new QTcpSocket(this);
        s->setSocketDescriptor(sd);
        // Write a minimal HTTP 403 so the user sees something if the browser
        // decided to try to read from this "proxy".
        const QByteArray msg =
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: 94\r\n"
            "Connection: close\r\n\r\n"
            "<html><body><h2>Blocked by Simanta</h2>"
            "<p>This site is not in the whitelist.</p></body></html>";
        s->write(msg);
        s->flush();
        s->disconnectFromHost();
        connect(s, &QTcpSocket::disconnected, s, &QTcpSocket::deleteLater);
    }
};
static BlackholeProxy* g_blackholeProxy = nullptr;

// -- Firewall rule helpers -------------------------------------------------
// We use Windows Advanced Firewall (netsh) to cut off QUIC/HTTP3 traffic that
// bypasses PAC/HTTP proxying, and optionally block raw TCP too when
// "Block All Internet" is active. Rules are tagged so we can remove them later.
static const char* FW_RULE_QUIC = "Simanta Block QUIC";
static const char* FW_RULE_TCP  = "Simanta Block TCP";

static void fw_addQuicBlock() {
    // Block outbound UDP 443 (HTTP/3 / QUIC) for everyone on the machine.
    // This forces browsers to fall back to TCP which the PAC can then route.
    QProcess::startDetached("netsh", {
        "advfirewall", "firewall", "add", "rule",
        QStringLiteral("name=%1").arg(FW_RULE_QUIC),
        "dir=out", "action=block", "protocol=UDP", "remoteport=443", "enable=yes"
    });
    // Also block UDP 80 for completeness (uncommon HTTP/3 fallback).
    QProcess::startDetached("netsh", {
        "advfirewall", "firewall", "add", "rule",
        QStringLiteral("name=%1").arg(FW_RULE_QUIC),
        "dir=out", "action=block", "protocol=UDP", "remoteport=80", "enable=yes"
    });
}

static void fw_removeQuicBlock() {
    QProcess::startDetached("netsh", {
        "advfirewall", "firewall", "delete", "rule",
        QStringLiteral("name=%1").arg(FW_RULE_QUIC)
    });
}

static void fw_addTcpBlock() {
    // Block outbound TCP 80/443 for a hard "no internet at all" stance.
    QProcess::startDetached("netsh", {
        "advfirewall", "firewall", "add", "rule",
        QStringLiteral("name=%1").arg(FW_RULE_TCP),
        "dir=out", "action=block", "protocol=TCP", "remoteport=80,443", "enable=yes"
    });
}

static void fw_removeTcpBlock() {
    QProcess::startDetached("netsh", {
        "advfirewall", "firewall", "delete", "rule",
        QStringLiteral("name=%1").arg(FW_RULE_TCP)
    });
}

// -- Browser policy enforcement ------------------------------------------
// Some students "escape" Internet Access Control because:
//   1) Chrome/Edge has DNS-over-HTTPS (DoH) ON, which uses UDP 443 to a
//      remote resolver (Cloudflare/Google) and bypasses the system PAC for
//      the DNS step. Once the IP is known, HTTP/3 over QUIC sneaks past as
//      well.
//   2) Firefox keeps its own proxy setting (Settings -> Network -> Proxy
//      "No proxy") and ignores Windows AutoConfigURL by default.
//   3) Brave / Vivaldi / Opera respect Chrome policy keys but only when the
//      relevant HKLM keys exist.
//   4) Some browsers cache "use system proxy" flag at startup -- they need
//      to be (re)started after policy is pushed.
//
// We push a small set of registry policies that:
//   - Disable DoH globally (Chrome, Edge, Firefox).
//   - Force "use system proxy" for Chrome family + Firefox.
//   - Disable QUIC in Chrome family (defense in depth on top of the firewall
//     rule that already blocks UDP 443).
//
// The policies survive browser reinstalls and apply on next browser start.
static void browserPolicy_apply() {
    QProcess::startDetached("powershell", {"-Command",
        // Chrome (and Brave/Vivaldi via Chromium policy)
        "$ck='HKLM:\\SOFTWARE\\Policies\\Google\\Chrome'; "
        "New-Item -Path $ck -Force | Out-Null; "
        "Set-ItemProperty -Path $ck -Name 'DnsOverHttpsMode' -Value 'off' -Type String -Force; "
        "Set-ItemProperty -Path $ck -Name 'QuicAllowed' -Value 0 -Type DWord -Force; "
        "Set-ItemProperty -Path $ck -Name 'ProxySettings' -Value '{\"ProxyMode\":\"system\"}' -Type String -Force; "
        // Edge
        "$ek='HKLM:\\SOFTWARE\\Policies\\Microsoft\\Edge'; "
        "New-Item -Path $ek -Force | Out-Null; "
        "Set-ItemProperty -Path $ek -Name 'DnsOverHttpsMode' -Value 'off' -Type String -Force; "
        "Set-ItemProperty -Path $ek -Name 'QuicAllowed' -Value 0 -Type DWord -Force; "
        "Set-ItemProperty -Path $ek -Name 'ProxySettings' -Value '{\"ProxyMode\":\"system\"}' -Type String -Force; "
        // Brave
        "$bk='HKLM:\\SOFTWARE\\Policies\\BraveSoftware\\Brave'; "
        "New-Item -Path $bk -Force | Out-Null; "
        "Set-ItemProperty -Path $bk -Name 'DnsOverHttpsMode' -Value 'off' -Type String -Force; "
        "Set-ItemProperty -Path $bk -Name 'QuicAllowed' -Value 0 -Type DWord -Force; "
        // Firefox: force system proxy + disable DoH (TRR)
        "$fk='HKLM:\\SOFTWARE\\Policies\\Mozilla\\Firefox'; "
        "New-Item -Path $fk -Force | Out-Null; "
        "Set-ItemProperty -Path $fk -Name 'DNSOverHTTPS' -Value '{\"Enabled\":false,\"Locked\":true}' -Type String -Force; "
        "$fp='HKLM:\\SOFTWARE\\Policies\\Mozilla\\Firefox\\Proxy'; "
        "New-Item -Path $fp -Force | Out-Null; "
        "Set-ItemProperty -Path $fp -Name 'Mode' -Value 'system' -Type String -Force; "
        "Set-ItemProperty -Path $fp -Name 'Locked' -Value 1 -Type DWord -Force"
    });
}

static void browserPolicy_remove() {
    // Remove ONLY the specific values Simanta sets, never the whole policy
    // key. Other software (corp Chrome policy, antivirus, parental controls)
    // may store unrelated values in the same keys -- a Recurse delete here
    // would silently break them.
    QProcess::startDetached("powershell", {"-Command",
        // Chrome
        "$ck='HKLM:\\SOFTWARE\\Policies\\Google\\Chrome'; "
        "Remove-ItemProperty -Path $ck -Name 'DnsOverHttpsMode' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path $ck -Name 'QuicAllowed'      -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path $ck -Name 'ProxySettings'    -ErrorAction SilentlyContinue; "
        // Edge
        "$ek='HKLM:\\SOFTWARE\\Policies\\Microsoft\\Edge'; "
        "Remove-ItemProperty -Path $ek -Name 'DnsOverHttpsMode' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path $ek -Name 'QuicAllowed'      -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path $ek -Name 'ProxySettings'    -ErrorAction SilentlyContinue; "
        // Brave
        "$bk='HKLM:\\SOFTWARE\\Policies\\BraveSoftware\\Brave'; "
        "Remove-ItemProperty -Path $bk -Name 'DnsOverHttpsMode' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path $bk -Name 'QuicAllowed'      -ErrorAction SilentlyContinue; "
        // Firefox: DNSOverHTTPS is a value on the parent key; the Proxy
        // subkey is fully ours so it's safe to drop the whole subkey.
        "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Mozilla\\Firefox' -Name 'DNSOverHTTPS' -ErrorAction SilentlyContinue; "
        "Remove-Item         -Path 'HKLM:\\SOFTWARE\\Policies\\Mozilla\\Firefox\\Proxy' -Recurse -ErrorAction SilentlyContinue"
    });
}

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

void resetAllRestrictions() {
    // Guard: don't spawn child processes while Windows is shutting down /
    // logging off. The session is already torn down at that point and
    // CreateProcess fails with "netsh.exe - Application Error: cannot start"
    // dialogs that block Windows from completing shutdown.
    if (g_shuttingDown) {
        QTextStream(stdout) << "[lab-student] System shutting down, skipping async restriction reset\n";
        return;
    }
    QTextStream(stdout) << "[lab-student] Auto-unblocking all restrictions...\n";
    if (g_pacServer) g_pacServer->currentPacContent = "";
    // Remove our firewall rules so QUIC/TCP traffic is free again.
    fw_removeQuicBlock();
    fw_removeTcpBlock();
    // Remove browser policy hardening so DoH / Firefox-proxy work normally.
    browserPolicy_remove();
    QProcess::startDetached("powershell", {"-Command", 
        "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxySettingsPerUser' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'EnableLegacyAutoProxyFeatures' -ErrorAction SilentlyContinue; "
        "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -Type DWord -Force; "
        "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -PropertyType DWord -Force; "
        "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue"
    });
    QTimer::singleShot(500, []() { refreshWindowsProxy(); });

    QProcess::startDetached("powershell", {"-Command", 
        "Set-ItemProperty -Path 'HKLM:\\System\\CurrentControlSet\\Services\\USBSTOR' -Name 'Start' -Value 3 -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableTaskMgr' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableTaskMgr' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableRegistryTools' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableRegistryTools' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer' -Name 'NoControlPanel' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer' -Name 'NoControlPanel' -ErrorAction SilentlyContinue"
    });
}

void resetAllRestrictionsSync() {
    if (g_shuttingDown) {
        QTextStream(stdout) << "[lab-student] System shutting down, skipping sync restriction reset\n";
        return;
    }
    QTextStream(stdout) << "[lab-student] Auto-unblocking all restrictions (sync)...\n";
    if (g_pacServer) g_pacServer->currentPacContent = "";
    // Remove firewall rules synchronously so uninstall/exit leaves no trace.
    {
        QProcess p;
        p.start("netsh", {
            "advfirewall", "firewall", "delete", "rule",
            QStringLiteral("name=%1").arg(FW_RULE_QUIC)
        });
        p.waitForFinished(3000);
    }
    {
        QProcess p;
        p.start("netsh", {
            "advfirewall", "firewall", "delete", "rule",
            QStringLiteral("name=%1").arg(FW_RULE_TCP)
        });
        p.waitForFinished(3000);
    }
    // Synchronously remove browser policies on shutdown / uninstall.
    // Use targeted value removal so we don't nuke other software's policy.
    {
        QProcess pp;
        pp.start("powershell", {"-Command",
            "$keys=@("
            "'HKLM:\\SOFTWARE\\Policies\\Google\\Chrome',"
            "'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Edge',"
            "'HKLM:\\SOFTWARE\\Policies\\BraveSoftware\\Brave'); "
            "foreach($k in $keys){ "
            "Remove-ItemProperty -Path $k -Name 'DnsOverHttpsMode' -ErrorAction SilentlyContinue; "
            "Remove-ItemProperty -Path $k -Name 'QuicAllowed' -ErrorAction SilentlyContinue; "
            "Remove-ItemProperty -Path $k -Name 'ProxySettings' -ErrorAction SilentlyContinue }; "
            "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Mozilla\\Firefox' -Name 'DNSOverHTTPS' -ErrorAction SilentlyContinue; "
            "Remove-Item -Path 'HKLM:\\SOFTWARE\\Policies\\Mozilla\\Firefox\\Proxy' -Recurse -ErrorAction SilentlyContinue"
        });
        pp.waitForFinished(3000);
    }
    QProcess p;
    p.start("powershell", {"-Command", 
        "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxySettingsPerUser' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'EnableLegacyAutoProxyFeatures' -ErrorAction SilentlyContinue; "
        "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -Type DWord -Force; "
        "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -PropertyType DWord -Force; "
        "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue"
    });
    p.waitForFinished();
    refreshWindowsProxy();

    QProcess p2;
    p2.start("powershell", {"-Command", 
        "Set-ItemProperty -Path 'HKLM:\\System\\CurrentControlSet\\Services\\USBSTOR' -Name 'Start' -Value 3 -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableTaskMgr' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableTaskMgr' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableRegistryTools' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' -Name 'DisableRegistryTools' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer' -Name 'NoControlPanel' -ErrorAction SilentlyContinue; "
        "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer' -Name 'NoControlPanel' -ErrorAction SilentlyContinue"
    });
    p2.waitForFinished();
}

void signalHandler(int signal)
{
    Q_UNUSED(signal)
    QTextStream(stdout) << "\n[lab-student] Shutting down...\n";
    if (g_agent) {
        g_agent->stop();
    }
    QCoreApplication::quit();
}
static HHOOK g_keyboardHook = nullptr;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        return 1; // Block all keyboard input system-wide when locked
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
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

    ~LockOverlay() {
        deactivate();
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
        if (!g_keyboardHook) {
            g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
        }
    }

    void deactivate()
    {
        if (g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
            g_keyboardHook = nullptr;
        }
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
        p.drawText(titleRect, Qt::AlignCenter,
            Lang::get().t("Screen Locked", "Layar Dikunci"));
        QFont subFont("Segoe UI", 13);
        p.setFont(subFont);
        p.setPen(QColor(230, 237, 243, 140));
        QRect subRect(0, height()/2 + 40, width(), 60);
        p.drawText(subRect, Qt::AlignCenter,
            Lang::get().t(
                "Your screen has been locked by the teacher.\nPlease wait for instructions.",
                "Layar Anda dikunci oleh guru.\nSilakan tunggu instruksi."));
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
        if (g_shuttingDown) { e->accept(); return; }
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

        // -- Header bar (Modern Clean) --
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

        // -- Chat area (QScrollArea) --
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

        // -- Input area --
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

        // -- Main container --
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

        // Header: clicking anywhere on it toggles the expanded state.
        // We lay it out as a plain QWidget and install an event filter that
        // forwards mouse release events to the same toggleExpanded() slot
        // the old "+/-" button used to call.
        auto* headerWidget = new QWidget(m_container);
        headerWidget->setObjectName("PanelHeader");
        headerWidget->setStyleSheet(
            "#PanelHeader { background: transparent; }"
            "#PanelHeader:hover { background: rgba(15,23,42,0.03); }");
        headerWidget->setCursor(Qt::PointingHandCursor);
        headerWidget->installEventFilter(this);
        m_headerWidget = headerWidget;

        auto* headerLayout = new QHBoxLayout(headerWidget);
        headerLayout->setContentsMargins(18, 12, 18, 12);
        headerLayout->setSpacing(10);

        m_monitorDot = new QLabel(headerWidget);
        m_monitorDot->setFixedSize(14, 14);
        m_monitorDot->setStyleSheet(
            "QLabel { background: #3FB950; border-radius: 7px; border: none; }");

        m_statusLabel = new QLabel(Lang::get().t("Connecting...", "Menghubungkan..."), headerWidget);
        m_statusLabel->setStyleSheet(
            "QLabel { color: #1E293B; font-size: 9pt; font-weight: 600;"
            " background: transparent; border: none; }");

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

        m_bodyWidget = new QWidget(m_container);
        m_bodyWidget->setStyleSheet("background: transparent;");
        auto* bodyLayout = new QVBoxLayout(m_bodyWidget);
        bodyLayout->setContentsMargins(14, 4, 14, 14);
        bodyLayout->setSpacing(8);

        // Monitor text strip removed as requested

        // -- Action Buttons --
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

        // -- Container layout in this widget --
        auto* wLayout = new QVBoxLayout(this);
        wLayout->setContentsMargins(0, 0, 0, 0); // No margins needed without shadow
        wLayout->addWidget(m_container);

        // -- Connections --
        // (Header is now the click target; no dedicated expand button.)

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
            // Send a language-neutral token so the teacher translates the
            // notification in the teacher's current UI language.
            sendQuickRequest("__REQ_NEED_HELP__");
        });

        connect(m_questionBtn, &QPushButton::clicked, this, [this]() {
            sendQuickRequest("__REQ_HAVE_QUESTION__");
        });

        connect(m_toiletBtn, &QPushButton::clicked, this, [this]() {
            sendQuickRequest("__REQ_TOILET__");
        });

        connect(m_doneBtn, &QPushButton::clicked, this, [this]() {
            sendQuickRequest("__REQ_FINISHED__");
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
        if (g_shuttingDown) { e->accept(); return; }
        e->ignore(); // Prevent closing -  panel must stay visible
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        // Header acts as a single click target to toggle expand/collapse,
        // matching the new design without a separate +/- button.
        if (watched == m_headerWidget) {
            if (event->type() == QEvent::MouseButtonPress) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    m_headerPressPos = me->globalPosition().toPoint();
                    m_headerPressed = true;
                    // Do not consume the event - let the parent drag handlers run.
                }
            } else if (event->type() == QEvent::MouseButtonRelease) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton && m_headerPressed) {
                    m_headerPressed = false;
                    QPoint delta = me->globalPosition().toPoint() - m_headerPressPos;
                    // Only treat as a click if the mouse barely moved - real
                    // drags produce big deltas and must NOT toggle collapse.
                    if (delta.manhattanLength() < 6
                        && m_headerWidget->rect().contains(me->pos())) {
                        toggleExpanded();
                        return true;
                    }
                }
            }
        }
        return QWidget::eventFilter(watched, event);
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
            // User dragged the panel, so any previously saved "before expand"
            // position is no longer relevant. Forget it so the next collapse
            // doesn't snap the panel back to the pre-drag location.
            m_hasPreExpandPos = false;
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
        updateLayout();

        // Keep the panel anchored wherever the user left it. The only times
        // we adjust position are:
        //   (a) expanding near the bottom would push the panel off-screen
        //       -> pin its bottom edge so it stays fully visible,
        //   (b) expanding near the top would push the header off-screen
        //       -> clamp to the top.
        // Collapsing restores the exact position the panel had before the
        // preceding expand, so dragging to the bottom + expand + collapse
        // returns the panel to the bottom where the user left it.
        QScreen* screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        QRect geo = screen->availableGeometry();

        bool expanding = m_expanded && height() > oldHeight;
        QPoint p = oldPos;

        if (expanding) {
            // Remember where we were before expansion so collapse can restore
            // it (e.g. user placed panel at bottom, expand pushed it up to
            // fit, collapse should put it back at the bottom).
            m_preExpandPos = oldPos;
            m_hasPreExpandPos = true;

            // Only re-pin if expansion makes the panel leave the screen.
            if (p.y() + height() > geo.bottom()) {
                p.setY(geo.bottom() - height());
            }
            if (p.y() < geo.top()) {
                p.setY(geo.top());
            }
        } else if (!m_expanded && m_hasPreExpandPos) {
            // Collapsing: restore the pre-expand position as long as it still
            // fits on screen (screen size or panel size might have changed).
            QPoint restored = m_preExpandPos;
            if (restored.y() + height() > geo.bottom()) {
                restored.setY(geo.bottom() - height());
            }
            if (restored.y() < geo.top()) {
                restored.setY(geo.top());
            }
            if (restored.x() + width() > geo.right()) {
                restored.setX(geo.right() - width());
            }
            if (restored.x() < geo.left()) {
                restored.setX(geo.left());
            }
            p = restored;
            m_hasPreExpandPos = false;
        }

        if (p != pos()) {
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
    QWidget* m_headerWidget;
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
    QPoint m_headerPressPos;
    bool m_headerPressed = false;
    bool m_manuallyPositioned;
    // Saved panel position BEFORE the most recent expand. Used by
    // toggleExpanded() to restore the panel to where the user left it when
    // the body collapses again.
    QPoint m_preExpandPos;
    bool m_hasPreExpandPos = false;
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
        iconLabel->setText(QString::fromUtf8("\u2193")); // down arrow ->

        m_titleLabel = new QLabel(Lang::get().t("Receiving file...", "Menerima file..."), container);
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
        m_titleLabel->setText(isFolder
            ? Lang::get().t("Receiving folder from Teacher...", "Menerima folder dari Guru...")
            : Lang::get().t("Receiving file from Teacher...",   "Menerima file dari Guru..."));
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
        m_titleLabel->setText(isFolder
            ? Lang::get().t("Folder received!", "Folder diterima!")
            : Lang::get().t("File received!",   "File diterima!"));
        m_fileNameLabel->setText(
            Lang::get().t(QStringLiteral("Saved to Downloads/Simanta/%1").arg(fileName),
                          QStringLiteral("Disimpan ke Downloads/Simanta/%1").arg(fileName)));
        m_progressBar->setValue(100);
        m_percentLabel->setText(QString::fromUtf8("\u2713")); // OK
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
    dialog->setWindowTitle(Lang::get().t("Message from Teacher", "Pesan dari Guru"));
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
    // Force light title bars -  ignore Windows 11 dark mode
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=0");

    QApplication app(argc, argv);
    app.setApplicationName("Simanta-student");
    app.setApplicationVersion("");
    app.setOrganizationName("Simanta");

    // -- Native event filter to detect Windows shutdown/logoff. --
    // When the user picks Shutdown/Restart/Sign-out, Windows sends
    // WM_QUERYENDSESSION and WM_ENDSESSION to every top-level window. By
    // flipping g_shuttingDown to true here, our LockOverlay / StudentPanel
    // stop refusing their close events so the OS can tear us down cleanly
    // instead of showing "This app is preventing shutdown" repeatedly.
    class ShutdownFilter : public QAbstractNativeEventFilter {
    public:
        bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) override {
            if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
                MSG* msg = static_cast<MSG*>(message);
                if (msg->message == WM_QUERYENDSESSION || msg->message == WM_ENDSESSION
                    || msg->message == WM_CLOSE) {
                    // WM_CLOSE by itself doesn't mean shutdown, but if Windows
                    // is asking us to close every window in rapid succession
                    // it's safest to allow them to close. We only set the
                    // flag on ENDSESSION-family messages.
                    if (msg->message != WM_CLOSE) {
                        g_shuttingDown = true;
                    }
                }
            }
            return false;
        }
    };
    static ShutdownFilter s_shutdownFilter;
    app.installNativeEventFilter(&s_shutdownFilter);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [](){
        g_shuttingDown = true;
    });
    
    // Set application icon using .ico for proper Windows taskbar scaling
    QString icoPath = QCoreApplication::applicationDirPath() + "/logo.ico";
    if (QFile::exists(icoPath)) {
        app.setWindowIcon(QIcon(icoPath));
    }

    // Single instance check using Windows Mutex (auto-released on crash/kill)
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\SimantaStudentMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        // Silently exit -  don't show message box (might block auto-start)
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

    // ========================================================
    // System Tray Icon -  student can initiate chat & help
    // ========================================================
    QSystemTrayIcon trayIcon;
    trayIcon.setToolTip(Lang::get().t("Simanta Student - Connecting...", "Simanta Siswa - Menyambung..."));

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
    auto* chatAction = trayMenu.addAction(Lang::get().t("Chat with Teacher", "Chat dengan Guru"));
    auto* helpAction = trayMenu.addAction(Lang::get().t("Request Help", "Minta Bantuan"));
    trayMenu.addSeparator();
    auto* panelAction = trayMenu.addAction(Lang::get().t("Show Panel", "Tampilkan Panel"));
    auto* statusAction = trayMenu.addAction(Lang::get().t("Status: Connecting...", "Status: Menyambung..."));
    statusAction->setEnabled(false);
    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    // Chat action -  open chat window
    QObject::connect(chatAction, &QAction::triggered, [&chatWindow]() {
        chatWindow.show();
        chatWindow.raise();
        chatWindow.activateWindow();
    });

    // Help request action -  popup dialog to type help message
    QObject::connect(helpAction, &QAction::triggered, [&agent]() {
        bool ok;
        QString msg = QInputDialog::getMultiLineText(
            nullptr,
            Lang::get().t("Request Help", "Minta Bantuan"),
            Lang::get().t("Describe your problem to the teacher:",
                          "Jelaskan masalahmu ke guru:"),
            "", &ok);
        if (ok && !msg.trimmed().isEmpty()) {
            agent.sendHelpRequest(msg.trimmed());
            QMessageBox::information(nullptr,
                Lang::get().t("Help Request Sent", "Permintaan Bantuan Terkirim"),
                Lang::get().t("Your help request has been sent to the teacher.",
                              "Permintaan bantuanmu sudah terkirim ke guru."));
        }
    });

    // Panel action -  show/raise the floating panel
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

    // ========================================================
    // Agent signal connections
    // ========================================================
    // Track last connect/disconnect time so we can suppress tray spam when the
    // teacher restarts the app or WiFi blips.
    auto* lastConnState = new bool(false);
    auto* lastStateChange = new qint64(0);

    QObject::connect(&agent, &LabMonitor::StudentAgent::connected,
                     [&trayIcon, &statusAction, &studentPanel, lastConnState, lastStateChange]() {
        QTextStream(stdout) << "[lab-student] Connected to teacher\n";
        trayIcon.setToolTip(Lang::get().t("Simanta Student - Connected", "Simanta Siswa - Terhubung"));
        statusAction->setText(Lang::get().t("Status: Connected", "Status: Terhubung"));
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool shouldNotify = !(*lastConnState) && (now - *lastStateChange > 5000);
        if (shouldNotify) {
            trayIcon.showMessage("Simanta",
                Lang::get().t("Connected to teacher", "Terhubung dengan guru"),
                QSystemTrayIcon::Information, 2000);
        }
        *lastConnState = true;
        *lastStateChange = now;
        studentPanel.setConnected(true);
    });

    // Teacher discovered via UDP beacon
    auto* announcedTeachers = new QSet<QString>();
    QObject::connect(&agent, &LabMonitor::StudentAgent::teacherDiscovered,
                     [&trayIcon, announcedTeachers](const QString& ip, uint16_t port, const QString& hostname) {
        Q_UNUSED(port)
        QTextStream(stdout) << "[lab-student] Teacher discovered: " << hostname
                            << " at " << ip << "\n";
        QString key = hostname + "@" + ip;
        if (announcedTeachers->contains(key)) return;
        announcedTeachers->insert(key);
        trayIcon.showMessage("Simanta",
            Lang::get().t(QStringLiteral("Teacher found: %1 (%2)").arg(hostname, ip),
                          QStringLiteral("Guru ditemukan: %1 (%2)").arg(hostname, ip)),
            QSystemTrayIcon::Information, 3000);
    });

    // Debounce restriction-reset: only unblock if the student stays
    // disconnected for more than 30 seconds (avoids thrashing PowerShell
    // on brief WiFi blips during monitoring).
    auto* pendingResetTimer = new QTimer(&app);
    pendingResetTimer->setSingleShot(true);
    pendingResetTimer->setInterval(30000);
    QObject::connect(pendingResetTimer, &QTimer::timeout, []() {
        QTextStream(stdout) << "[lab-student] Prolonged disconnect -> clearing restrictions\n";
        resetAllRestrictions();
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::disconnected,
                     [&lockOverlay, &trayIcon, &statusAction, &studentPanel, pendingResetTimer, lastConnState, lastStateChange]() {
        QTextStream(stdout) << "[lab-student] Disconnected from teacher\n";
        trayIcon.setToolTip(Lang::get().t("Simanta Student - Reconnecting...", "Simanta Siswa - Menyambung ulang..."));
        statusAction->setText(Lang::get().t("Status: Reconnecting...", "Status: Menyambung ulang..."));
        studentPanel.setConnected(false);
        *lastConnState = false;
        *lastStateChange = QDateTime::currentMSecsSinceEpoch();
        if (lockOverlay.isVisible()) {
            lockOverlay.deactivate();
            QTextStream(stdout) << "[lab-student] Auto-unlocked (disconnected)\n";
        }
        // Only clear proxy/USB/etc after 30 s of continuous disconnect.
        if (!pendingResetTimer->isActive()) pendingResetTimer->start();
    });

    QObject::connect(&agent, &LabMonitor::StudentAgent::connected,
                     [pendingResetTimer]() {
        // Reconnected before the grace period elapsed -> do not reset restrictions.
        if (pendingResetTimer->isActive()) pendingResetTimer->stop();
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
        trayIcon.showMessage(Lang::get().t("Chat from ", "Chat dari ") + sender, message,
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

    // Blackhole proxy so that "PROXY 127.0.0.1:9999" fails fast with a clear
    // "Blocked by Simanta" page instead of timing out.
    g_blackholeProxy = new BlackholeProxy(&app);
    if (!g_blackholeProxy->listen(QHostAddress::LocalHost, 9999)) {
        qWarning() << "Failed to start blackhole proxy on port 9999";
    }

    QObject::connect(&agent, &LabMonitor::StudentAgent::cmdReceived,
                     [&studentPanel, &lockOverlay](const QString& cmd) {
        QTextStream(stdout) << "[lab-student] Executing command: " << cmd << "\n";
        if (cmd.startsWith("SET_LANG:")) {
            QString lang = cmd.mid(9);
            Lang::get().setLanguage(lang);
            studentPanel.updateTranslations();
            if (lockOverlay.isVisible()) lockOverlay.update(); // repaint in new language
        } else if (cmd == "INTERNET_BLOCK") {
            // Hardest stance: block UDP 443/80 (QUIC) + TCP 80/443, and still
            // install a deny-all PAC so anything that tries gets a clear error.
            fw_addQuicBlock();
            fw_addTcpBlock();
            // Force browsers to use system proxy and disable DoH/QUIC so they
            // can't bypass the PAC via direct DNS-over-HTTPS or HTTP/3.
            browserPolicy_apply();
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
            // Lift firewall rules first so traffic can flow immediately.
            fw_removeQuicBlock();
            fw_removeTcpBlock();
            browserPolicy_remove();
            QProcess::startDetached("powershell", {"-Command", 
                "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxySettingsPerUser' -ErrorAction SilentlyContinue; "
                "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -Type DWord -Force; "
                "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'ProxyEnable' -Value 0 -PropertyType DWord -Force; "
                "Remove-ItemProperty -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue; "
                "Remove-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name 'AutoConfigURL' -ErrorAction SilentlyContinue"
            });
            QTimer::singleShot(500, []() { refreshWindowsProxy(); });
        } else if (cmd.startsWith("WHITELIST_SET:")) {
            // Whitelist mode: allow TCP (PAC decides whitelist vs blackhole),
            // but kill QUIC/HTTP3 so browsers cannot sneak past the PAC via UDP 443.
            fw_removeTcpBlock();
            fw_addQuicBlock();
            // Same browser hardening as INTERNET_BLOCK so DoH / Firefox-own-proxy
            // can't bypass the system PAC.
            browserPolicy_apply();
            QString domainsPart = cmd.mid(14);
            QStringList domains = domainsPart.split(",", Qt::SkipEmptyParts);

            // Extract a clean "host" from an arbitrary user-entered URL-like string.
            // Handles: "https://foo.com/path", "www.foo.com", "foo.com/@user",
            //          "foo.com:8080/path?x=1", "FOO.COM/", plain "foo.com".
            auto normalizeHost = [](const QString& raw) -> QString {
                QString d = raw.trimmed().toLower();
                if (d.isEmpty()) return d;
                // Strip scheme
                int schemeIdx = d.indexOf("://");
                if (schemeIdx >= 0) d = d.mid(schemeIdx + 3);
                // Strip any path / query / fragment (anything after first '/', '?' or '#')
                int cut = d.size();
                for (QChar ch : {QChar('/'), QChar('?'), QChar('#')}) {
                    int p = d.indexOf(ch);
                    if (p >= 0 && p < cut) cut = p;
                }
                d = d.left(cut);
                // Strip port
                int colon = d.indexOf(':');
                if (colon >= 0) d = d.left(colon);
                // Strip leading "www."
                if (d.startsWith("www.")) d = d.mid(4);
                // Strip trailing dots
                while (d.endsWith(".")) d.chop(1);
                return d;
            };

            // Known "sibling" domains that many big sites NEED to function.
            // Without these, things like YouTube channels won't load because the
            // player, thumbnails and video CDN live on different hosts.
            auto relatedDomains = [](const QString& host) -> QStringList {
                QStringList extra;
                if (host == "youtube.com" || host.endsWith(".youtube.com")) {
                    extra << "youtu.be"
                          << "youtube-nocookie.com"
                          << "googlevideo.com"
                          << "ytimg.com"
                          << "ggpht.com"
                          << "yt3.ggpht.com";
                } else if (host == "google.com" || host.endsWith(".google.com")) {
                    extra << "gstatic.com"
                          << "googleusercontent.com"
                          << "googleapis.com";
                } else if (host == "facebook.com" || host.endsWith(".facebook.com")) {
                    extra << "fbcdn.net" << "fbsbx.com";
                } else if (host == "instagram.com" || host.endsWith(".instagram.com")) {
                    extra << "cdninstagram.com" << "fbcdn.net";
                } else if (host == "twitter.com" || host == "x.com") {
                    extra << "twimg.com" << "t.co";
                } else if (host == "github.com" || host.endsWith(".github.com")) {
                    // GitHub splits CSS/JS/images/avatars/raw files/API across
                    // many hosts. Without these the site renders broken.
                    extra << "githubassets.com"
                          << "githubusercontent.com"
                          << "githubcopilot.com"
                          << "github.io"
                          << "github.dev"
                          << "githubstatus.com";
                } else if (host == "wikipedia.org" || host.endsWith(".wikipedia.org")
                           || host == "wikimedia.org" || host.endsWith(".wikimedia.org")) {
                    extra << "wikimedia.org"
                          << "wikipedia.org"
                          << "wikidata.org"
                          << "wiktionary.org"
                          << "mediawiki.org";
                } else if (host == "reddit.com" || host.endsWith(".reddit.com")) {
                    extra << "redd.it"
                          << "redditstatic.com"
                          << "redditmedia.com";
                } else if (host == "linkedin.com" || host.endsWith(".linkedin.com")) {
                    extra << "licdn.com";
                } else if (host == "discord.com" || host.endsWith(".discord.com")) {
                    extra << "discordapp.com"
                          << "discordapp.net"
                          << "discord.gg"
                          << "discord.media";
                } else if (host == "microsoft.com" || host.endsWith(".microsoft.com")) {
                    extra << "msecnd.net"
                          << "office.com"
                          << "office.net"
                          << "live.com";
                } else if (host == "stackoverflow.com" || host.endsWith(".stackoverflow.com")) {
                    extra << "sstatic.net"
                          << "stackexchange.com";
                }
                return extra;
            };

            // Common asset / font / CDN hosts that almost every modern site
            // pulls from. We always allow these in whitelist mode so that
            // whitelisted pages render with their CSS, JS and web fonts
            // instead of looking broken. These hosts don't host browsable
            // content, they only serve static assets.
            const QStringList commonAssetCdns = {
                // jsDelivr / unpkg / cdnjs – JS library CDNs
                "jsdelivr.net",
                "unpkg.com",
                "cdnjs.cloudflare.com",
                // Popular per-library CDNs
                "jquery.com",
                "bootstrapcdn.com",
                "fontawesome.com",
                "use.fontawesome.com",
                "kit.fontawesome.com",
                // Google Fonts (font CSS + WOFF files)
                "fonts.googleapis.com",
                "fonts.gstatic.com",
                // Typekit / Adobe Fonts
                "typekit.net",
                "use.typekit.net",
                // Generic static-asset providers
                "gstatic.com"
            };

            QSet<QString> uniqueHosts;
            for (const QString& raw : domains) {
                QString host = normalizeHost(raw);
                if (host.isEmpty()) continue;
                uniqueHosts.insert(host);
                for (const QString& rel : relatedDomains(host)) {
                    uniqueHosts.insert(rel);
                }
            }
            // Always include generic asset/font CDNs so whitelisted pages
            // render with their styles, scripts and fonts.
            for (const QString& cdn : commonAssetCdns) {
                uniqueHosts.insert(cdn);
            }

            QString pacContent = "function FindProxyForURL(url, host) {\n";
            pacContent += "  host = host.toLowerCase();\n";
            for (const QString& d : uniqueHosts) {
                // Match the domain itself and any subdomain of it
                pacContent += QString("  if (host == \"%1\" || dnsDomainIs(host, \".%1\")) return \"DIRECT\";\n").arg(d);
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
            QTextStream(stdout) << "[lab-student] Whitelist PAC HTTP Server applied with "
                                << uniqueHosts.size() << " hosts\n";
        } else if (cmd == "WHITELIST_CLEAR") {
            // Lift QUIC block as well.
            fw_removeQuicBlock();
            browserPolicy_remove();
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

    // -- KICK handling: teacher forcefully disconnected this student --
    QObject::connect(&agent, &LabMonitor::StudentAgent::kicked,
                     [&agent, &lockOverlay, &trayIcon, &statusAction, &studentPanel]() {
        QTextStream(stdout) << "[lab-student] KICKED by teacher\n";
        trayIcon.setToolTip(Lang::get().t("Simanta Student - Disconnected by Teacher",
                                          "Simanta Siswa - Diputus oleh Guru"));
        statusAction->setText(Lang::get().t("Status: Disconnected by Teacher",
                                            "Status: Diputus oleh Guru"));
        trayIcon.showMessage("Simanta",
            Lang::get().t("You have been disconnected by the teacher.\nReconnecting in 30 seconds...",
                          "Kamu telah diputus oleh guru.\nMenyambung ulang dalam 30 detik..."),
            QSystemTrayIcon::Warning, 5000);
        studentPanel.setConnected(false);
        if (lockOverlay.isVisible()) {
            lockOverlay.deactivate();
        }
        resetAllRestrictions();

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

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        // resetAllRestrictionsSync() internally checks g_shuttingDown and
        // becomes a no-op during Windows shutdown (avoids spawning netsh /
        // powershell while the session is already being torn down, which
        // surfaces as "netsh.exe Application Error" dialogs).
        resetAllRestrictionsSync();
    });

    // Force unblock everything on startup in case it was killed via Task Manager previously
    resetAllRestrictions();

    agent.start();

    return app.exec();
}

#include "main.moc"



