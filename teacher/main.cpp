#include <QApplication>
#include <QIcon>
#include <QTextStream>
#include <QFile>
#include <QCoreApplication>
#include <QMessageBox>
#include <windows.h>
#undef DEFAULT_QUALITY

#include "tutor_window.h"
#include "protocol.h"

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=0");

    QApplication app(argc, argv);
    app.setApplicationName("Simanta");
    app.setApplicationVersion("");
    app.setOrganizationName("Simanta");

    // Single instance check using Windows Mutex (auto-released on crash/kill)
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\SimantaTeacherMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        QMessageBox::warning(nullptr, "Simanta Teacher", "Simanta Teacher Console sudah berjalan!");
        return 1;
    }
    QFont defaultFont("Segoe UI", 10);
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);

    app.setStyleSheet(
        "QMessageBox { background: #FFFFFF; }"
        "QMessageBox QLabel { color: #1A2233; }"
        "QMessageBox QPushButton { background: #F0F4F8; color: #1A2233; border: 1px solid #CBD5E1;"
        " border-radius: 6px; padding: 6px 20px; font-weight: 500; min-width: 80px; }"
        "QMessageBox QPushButton:hover { background: #E2E8F0; }"
        "QMessageBox QPushButton:default { background: #1A73E8; color: #FFFFFF; border: none; }"
        "QMessageBox QPushButton:default:hover { background: #1557B0; }"
        "QInputDialog { background: #FFFFFF; }"
        "QInputDialog QLabel { color: #1A2233; }"
        "QInputDialog QLineEdit { background: #FFFFFF; color: #1A2233; border: 1px solid #CBD5E1;"
        " border-radius: 6px; padding: 6px 10px; }"
        "QInputDialog QPushButton { background: #F0F4F8; color: #1A2233; border: 1px solid #CBD5E1;"
        " border-radius: 6px; padding: 6px 20px; font-weight: 500; min-width: 80px; }"
        "QInputDialog QPushButton:default { background: #1A73E8; color: #FFFFFF; border: none; }"
    );
    app.setWindowIcon(QIcon(":/icons/icons/logo.ico"));
    QTextStream(stdout) << "============================================\n";
    QTextStream(stdout) << "   Simanta - Teacher Console\n";
    QTextStream(stdout) << "============================================\n";
    QTextStream(stdout) << "   Listening on port " << LabMonitor::DEFAULT_PORT << "\n";
    QTextStream(stdout) << "   Ctrl+A: Select All | Esc: Deselect\n";
    QTextStream(stdout) << "   F5: Refresh\n";
    QTextStream(stdout) << "============================================\n";

    LabMonitor::TutorWindow window;
    window.show();

    return app.exec();
}

