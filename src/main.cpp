#include <QApplication>
#include <QDir>
#include "mainwindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    // Look for a bundled ffmpeg/ffprobe next to the program first.
    const QByteArray sep = QDir::listSeparator().toLatin1();
    qputenv("PATH", QDir::toNativeSeparators(app.applicationDirPath()).toLocal8Bit()
                    + sep + qgetenv("PATH"));
    MainWindow w;
    w.show();
    return app.exec();
}
