#include <QApplication>
#include <QDir>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("OpenClip");
    app.setWindowIcon(QIcon(":/icon.png"));
    // Look for a bundled ffmpeg/ffprobe next to the program first.
    const QString extra = QDir::toNativeSeparators(app.applicationDirPath());
    const QString path = extra + QDir::listSeparator() + qEnvironmentVariable("PATH");
    qputenv("PATH", path.toLocal8Bit());
    MainWindow w;
    w.show();
    return app.exec();
}
