#ifndef DAI_ONEWIRETHERMPLUGIN_H
#define DAI_ONEWIRETHERMPLUGIN_H

#include <memory>

#include <QFile>

#include "plugin_global.h"
#include <Dai/checkerinterface.h>
#include <Helpz/simplethread.h>
#include "one_wire_therm_task.h"

namespace Dai {
namespace OneWireTherm {

class ONEWIRETHERMPLUGINSHARED_EXPORT OneWireThermPlugin : public QObject, public Checker_Interface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DaiCheckerInterface_iid FILE "checkerinfo.json")
    Q_INTERFACES(Dai::Checker_Interface)

public:
    OneWireThermPlugin();
    ~OneWireThermPlugin();

    // CheckerInterface interface
public:
    void configure(QSettings* settings, Project* proj) override;
    bool check(Device *dev) override;
    void stop() override;
    void write(std::vector<Write_Cache_Item>& items) override;
private:
    using Therm_Task_Thread = Helpz::ParamThread<One_Wire_Therm_Task>;
    Therm_Task_Thread* therm_thread_ = nullptr;
};

} // namespace OneWireTherm
} // namespace Dai

#endif // DAI_ONEWIRETHERMPLUGIN_H
