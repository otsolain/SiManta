#include <QApplication>
#include <QIcon>
#include <QTextStream>

#include "tutor_window.h"
#include "protocol.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SiManta");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("SiManta");
    QFont defaultFont("Segoe UI", 10);
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);
    app.setWindowIcon(QIcon(":/icons/icons/logo.ico"));

    QTextStream(stdout) << "============================================\n";
    QTextStream(stdout) << "   SiManta - Teacher Console v1.0\n";
    QTextStream(stdout) << "============================================\n";
    QTextStream(stdout) << "   Listening on port " << LabMonitor::DEFAULT_PORT << "\n";
    QTextStream(stdout) << "   Ctrl+A: Select All | Esc: Deselect\n";
    QTextStream(stdout) << "   F5: Refresh\n";
    QTextStream(stdout) << "============================================\n";

    LabMonitor::TutorWindow window;
    window.show();

    return app.exec();
}
