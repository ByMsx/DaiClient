#ifndef DAI_MODBUS_PLUGIN_H
#define DAI_MODBUS_PLUGIN_H

#include "modbus_plugin_base.h"

namespace Dai {
namespace Modbus {

class MODBUSPLUGINSHARED_EXPORT Modbus_Plugin : public Modbus_Plugin_Base
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DaiCheckerInterface_iid FILE "checkerinfo.json")
    Q_INTERFACES(Dai::Checker_Interface)
};

} // namespace Modbus
} // namespace Dai

#endif // DAI_MODBUSPLUGIN_H
