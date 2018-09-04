#include <QApplication>

#include "mainwindow.h"
#include "version.h"

namespace Dai {
    QString getVersionString() { return DaiEmulator::Version::getVersionString(); }
}

int main(int argc, char *argv[])
{
    SET_DAI_META("DaiEmulator")

    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    return a.exec();
}
