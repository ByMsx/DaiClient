#ifndef DAI_MODBUS_FILE_WRITER_H
#define DAI_MODBUS_FILE_WRITER_H

#include <QFile>
#include <QEventLoop>
#include <QModbusRtuSerialMaster>

#include <Dai/deviceitem.h>

#include "config.h"

namespace Dai {
namespace Modbus {

class File_Writer : public QModbusRtuSerialMaster
{
    Q_OBJECT

    static const int header_size;
public:
    File_Writer(Config&& config, DeviceItem* item, const QVariant& raw_data, uint32_t user_id, int interval = 200, int part_size = 240);
private slots:
    void start();
    void modbus_error(QModbusDevice::Error e);
    void write_file_part();
private:
    void process_reply(QModbusReply* reply);
    void reply_finish(QModbusReply* reply);

    int32_t user_id_, address_, interval_, part_size_;
    DeviceItem* item_;

    QFile file_;
};

} // namespace Modbus
} // namespace Dai

#endif // DAI_MODBUS_FILE_WRITER_H
