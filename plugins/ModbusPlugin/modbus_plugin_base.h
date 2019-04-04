#ifndef DAI_MODBUS_PLAGIN_BASE_H
#define DAI_MODBUS_PLAGIN_BASE_H

#include <QLoggingCategory>
#include <QModbusRtuSerialMaster>
#include <QSerialPort>
#include <QEventLoop>

#include <memory>

#include <Dai/checkerinterface.h>

#include "modbusplugin_global.h"
#include "config.h"

namespace Dai {
namespace Modbus {

Q_DECLARE_LOGGING_CATEGORY(ModbusLog)

class MODBUSPLUGINSHARED_EXPORT Modbus_Plugin_Base : public QModbusRtuSerialMaster, public Checker_Interface
{
    Q_OBJECT
public:
    Modbus_Plugin_Base();
    ~Modbus_Plugin_Base();

    bool check_break_flag() const;
    void clear_break_flag();

    const Config& config() const;

    bool checkConnect();

    // CheckerInterface interface
public:
    virtual void configure(QSettings* settings, Project*) override;
    virtual bool check(Device *dev) override;
    virtual void stop() override;
    virtual void write(std::vector<Write_Cache_Item>& items) override;
public slots:
    QVariantList read(int serverAddress, uchar regType = QModbusDataUnit::InputRegisters,
                      int startAddress = 0, quint16 unitCount = 1, bool clearCache = true);
protected:
    typedef std::map<int, DeviceItem*> DevItems;
    void proccessRegister(Device* dev, QModbusDataUnit::RegisterType registerType, const DevItems& itemMap);
    void write_item_pack(Device* dev, QModbusDataUnit::RegisterType reg_type, std::vector<Write_Cache_Item*>& items);
    void write_multi_item(int server_address, QModbusDataUnit::RegisterType reg_type, int start_address, const QVector<quint16>& values);

    int32_t address(Device* dev, bool *ok = nullptr) const;
    int32_t unit(DeviceItem* item, bool *ok = nullptr) const;

    typedef std::map<std::pair<int, QModbusDataUnit::RegisterType>, QModbusDevice::Error> StatusCacheMap;
    StatusCacheMap dev_status_cache_;
private:
    Config config_;

    QEventLoop wait_;
    bool b_break;
};

} // namespace Modbus
} // namespace Dai

#endif // DAI_MODBUS_PLAGIN_BASE_H
