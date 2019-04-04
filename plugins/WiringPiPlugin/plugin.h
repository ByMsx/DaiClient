#ifndef DAI_WIRINGPIPLUGIN_H
#define DAI_WIRINGPIPLUGIN_H

#include <QLoggingCategory>

#include <memory>

#include "plugin_global.h"
#include <Dai/checkerinterface.h>

namespace Dai {
namespace WiringPi {

Q_DECLARE_LOGGING_CATEGORY(WiringPiLog)

class WIRINGPIPLUGINSHARED_EXPORT WiringPiPlugin : public QObject, public Checker_Interface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DaiCheckerInterface_iid FILE "checkerinfo.json")
    Q_INTERFACES(Dai::Checker_Interface)

public:
    WiringPiPlugin();
    ~WiringPiPlugin();

    // CheckerInterface interface
public:
    void configure(QSettings* settings, Project*) override;
    bool check(Device *dev) override;
    void stop() override;
    void write(std::vector<Write_Cache_Item>& items) override;
private:
    bool b_break;
};

} // namespace WiringPi
} // namespace Dai

#endif // DAI_WIRINGPIPLUGIN_H
