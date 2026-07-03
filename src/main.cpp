#include <QApplication>
#include <QIcon>
#include <QLocalSocket>
#include "app/Application.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("hopy");
    app.setOrganizationName("hopy");
    app.setWindowIcon(QIcon(QStringLiteral(":/logo.ico")));
    app.setQuitOnLastWindowClosed(false); // stay resident in the tray when the window hides

    // Single instance: if one is already running, ask it to show and exit.
    {
        QLocalSocket probe;
        probe.connectToServer(hopy::kSingleInstanceKey);
        if (probe.waitForConnected(200)) {
            probe.write("show");
            probe.flush();
            probe.waitForBytesWritten(200);
            probe.disconnectFromServer();
            return 0;
        }
    }

    hopy::Application hopyApp;
    hopyApp.start();
    return app.exec();
}
