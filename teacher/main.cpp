#include <QApplication>
#include <QIcon>
#include <QTextStream>

#include "tutor_window.h"
#include "protocol.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Lab Monitor");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LabMonitor");

    // Set application-wide font
    QFont defaultFont("Segoe UI", 10);
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);

    QTextStream(stdout) << "╔══════════════════════════════════════════╗\n";
    QTextStream(stdout) << "║   Lab Monitor — Teacher Console v1.0    ║\n";
    QTextStream(stdout) << "╠══════════════════════════════════════════╣\n";
    QTextStream(stdout) << "║   Listening on port " << LabMonitor::DEFAULT_PORT << "                ║\n";
    QTextStream(stdout) << "║   Ctrl+A: Select All | Esc: Deselect    ║\n";
    QTextStream(stdout) << "║   F5: Refresh                           ║\n";
    QTextStream(stdout) << "╚══════════════════════════════════════════╝\n";

    LabMonitor::TutorWindow window;
    window.show();

    return app.exec();
}
