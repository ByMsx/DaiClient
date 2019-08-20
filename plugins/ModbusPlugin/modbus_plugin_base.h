#ifndef DAI_MODBUS_PLAGIN_BASE_H
#define DAI_MODBUS_PLAGIN_BASE_H

#include <QLoggingCategory>
#include <QModbusRtuSerialMaster>
#include <QSerialPort>

#include <memory>

#include <Dai/checkerinterface.h>

#include "modbusplugin_global.h"
#include "config.h"

namespace Dai {
namespace Modbus {

Q_DECLARE_LOGGING_CATEGORY(ModbusLog)

struct Modbus_Queue;

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

    static int32_t address(Device* dev, bool *ok = nullptr);
    static int32_t unit(DeviceItem* item, bool *ok = nullptr);

    // CheckerInterface interface
public:
    virtual void configure(QSettings* settings, Project*) override;
    virtual bool check(Device *dev) override;
    virtual void stop() override;
    virtual void write(std::vector<Write_Cache_Item>& items) override;
public slots:
    QStringList available_ports() const;

    void clear_status_cache();
private slots:
    void write_finished_slot();
    void read_finished_slot();
protected:
    void clear_queue();
    void print_cached(int server_address, QModbusDataUnit::RegisterType register_type, Error value, const QString& text);
    bool reconnect();
    void read(const QVector<DeviceItem *>& dev_items);
    void process_queue();
    QVector<quint16> cache_items_to_values(const std::vector<Write_Cache_Item>& items) const;
    void write_pack(int server_address, QModbusDataUnit::RegisterType register_type, int start_address, const std::vector<Write_Cache_Item>& items, QModbusReply** reply);
    void write_finished(QModbusReply* reply);
    void read_pack(int server_address, QModbusDataUnit::RegisterType register_type, int start_address, const std::vector<DeviceItem*>& items, QModbusReply** reply);
    void read_finished(QModbusReply* reply);

    typedef std::map<std::pair<int, QModbusDataUnit::RegisterType>, QModbusDevice::Error> StatusCacheMap;
    StatusCacheMap dev_status_cache_;
private:
    Config config_;

    Modbus_Queue* queue_;

    bool b_break, is_port_name_in_config_;
};

} // namespace Modbus
} // namespace Dai

#endif // DAI_MODBUS_PLAGIN_BASE_H
