#include <QCoreApplication>

#include "worker.h"
#include "version.h"

namespace Dai {
    QString getVersionString() { return DaiClient::Version::getVersionString(); }
}

int main(int argc, char *argv[])
{
    SET_DAI_META("DaiClient")

    QLocale loc = QLocale::system(); // current locale
    loc.setNumberOptions(QLocale::c().numberOptions()); // borrow number options from the "C" locale
    QLocale::setDefault(loc); // set as default

    return Dai::Service::instance(argc, argv).exec();
}
