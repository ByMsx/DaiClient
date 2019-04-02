#ifndef DAI_MODBUSPLUGIN_H
#define DAI_MODBUSPLUGIN_H

#include <QLoggingCategory>
#include <QModbusRtuSerialMaster>
//#include <QModbusDevice>
//#include <QModbusDataUnit>
//#include <QModbusRtuSerialMaster>
#include <QSerialPort>
#include <QEventLoop>

#include <memory>

#include <Dai/checkerinterface.h>

#include "modbusplugin_global.h"
#include "config.h"

namespace Dai {
namespace Modbus {

Q_DECLARE_LOGGING_CATEGORY(ModbusLog)

class MODBUSPLUGINSHARED_EXPORT ModbusPlugin : public QModbusRtuSerialMaster, public CheckerInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DaiCheckerInterface_iid FILE "checkerinfo.json")
    Q_INTERFACES(Dai::CheckerInterface)

public:
    ModbusPlugin();
    ~ModbusPlugin();

    // CheckerInterface interface
public:
    void configure(QSettings* settings, Project*) override;
    bool check(Device *dev) override;
    void stop() override;
    void write(DeviceItem* item, const QVariant& raw_data, uint32_t user_id) override;

    void writeFile(uint serverAddress, const QString& fileName);
public slots:
    QVariantList read(int serverAddress, uchar regType = QModbusDataUnit::InputRegisters,
                      int startAddress = 0, quint16 unitCount = 1, bool clearCache = true);
private:

    bool checkConnect();

    int32_t unit(DeviceItem* item) const;

    std::unique_ptr<Config> conf_;
    struct { int part_size_, part_interval_; } firmware_;

    typedef std::map<int, DeviceItem*> DevItems;
    typedef std::map<std::pair<int, QModbusDataUnit::RegisterType>, QModbusDevice::Error> StatusCacheMap;
    StatusCacheMap devStatusCache;

    QEventLoop wait_;
    bool b_break;
};

} // namespace Modbus
} // namespace Dai

#endif // DAI_MODBUSPLUGIN_H
