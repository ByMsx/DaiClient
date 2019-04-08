#include "worker.h"
//#include "version.h"

namespace Dai {
#define STR(x) #x
    QString getVersionString() { return STR(VER_MJ) "." STR(VER_MN) "." STR(VER_B); }
}

int main(int argc, char *argv[])
{
    SET_DAI_META("DaiClient")

    QLocale loc = QLocale::system(); // current locale
    loc.setNumberOptions(QLocale::c().numberOptions()); // borrow number options from the "C" locale
    QLocale::setDefault(loc); // set as default

    return Dai::Service::instance(argc, argv).exec();
}
