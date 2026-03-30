#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("pfvc");
    app.setOrganizationName("pfvc");
    app.setWindowIcon(QIcon::fromTheme("pfvc", QIcon(":/pfvc_logo.svg")));

    MainWindow window;
    window.show();

    return app.exec();
}
