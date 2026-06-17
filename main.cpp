#include <QApplication>
#include <QIcon>
#include "Sources/Headers/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/app_icon.jpg"));

    MainWindow window;
    window.show();
    return app.exec();
}
