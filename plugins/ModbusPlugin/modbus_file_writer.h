#ifndef DAI_MODBUS_FILE_WRITER_H
#define DAI_MODBUS_FILE_WRITER_H

#include <QFile>
#include <QEventLoop>

#include <Dai/deviceitem.h>

QT_FORWARD_DECLARE_CLASS(QModbusRtuSerialMaster)
QT_FORWARD_DECLARE_CLASS(QModbusReply)

namespace Dai {
namespace Modbus {

struct FlashInfo {
};

class File_Writer
{
    static const int header_size;
public:
    File_Writer(QModbusRtuSerialMaster* modbus, DeviceItem* item, const QVariant& raw_data, uint32_t user_id, int interval = 200, int part_size = 240);
private:
    void write_file_part();
    void process_reply(QModbusReply* reply);

    int32_t user_id_, address_, interval_, part_size_;
    QModbusRtuSerialMaster* modbus_;
    DeviceItem* item_;

    QFile file_;

    QEventLoop wait_;
};

} // namespace Modbus
} // namespace Dai

#endif // DAI_MODBUS_FILE_WRITER_H
