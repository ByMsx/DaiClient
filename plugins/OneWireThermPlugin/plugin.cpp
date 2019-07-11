#include <QDebug>
#include "plugin.h"

namespace Dai {
namespace OneWireTherm {

// ----------

OneWireThermPlugin::OneWireThermPlugin() :
    QObject(),
    therm_thread_(nullptr)
{
}

OneWireThermPlugin::~OneWireThermPlugin()
{
    therm_thread_->quit();
    therm_thread_->wait();
    delete therm_thread_;
}

void OneWireThermPlugin::configure(QSettings */*settings*/, Project */*proj*/)
{
    therm_thread_ = new Therm_Task_Thread();
    therm_thread_->start();
}

bool OneWireThermPlugin::check(Device* dev)
{
    if (therm_thread_ != nullptr && therm_thread_->isRunning())
    {
        QMetaObject::invokeMethod(therm_thread_->ptr(), "read_therm_data", Qt::QueuedConnection, Q_ARG(Device*, dev));
    }
    return true;
}

void OneWireThermPlugin::stop() {}
void OneWireThermPlugin::write(std::vector<Write_Cache_Item>& /*items*/) {}

} // namespace Modbus
} // namespace Dai
