#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QCryptographicHash>

#include <Dai/device.h>

#include "modbusplugin.h"
#include "modbus_file_writer.h"

namespace Dai {
namespace Modbus {

/*static*/ const int File_Writer::header_size = 10;

quint16 crc_reflect(quint16 data, qint32 len)
{
    // Generated by pycrc v0.8.3, https://pycrc.org
    // Width = 16, Poly = 0x8005, XorIn = 0xffff, ReflectIn = True,
    // XorOut = 0x0000, ReflectOut = True, Algorithm = bit-by-bit-fast

    quint16 ret = data & 0x01;
    for (qint32 i = 1; i < len; i++) {
        data >>= 1;
        ret = (ret << 1) | (data & 0x01);
    }
    return ret;
}

quint16 calculateCRC(const char *data, qint32 len)
{
    // Generated by pycrc v0.8.3, https://pycrc.org
    // Width = 16, Poly = 0x8005, XorIn = 0xffff, ReflectIn = True,
    // XorOut = 0x0000, ReflectOut = True, Algorithm = bit-by-bit-fast

    quint16 crc = 0xFFFF;
    while (len--) {
        const quint8 c = *data++;
        for (qint32 i = 0x01; i & 0xFF; i <<= 1) {
            bool bit = crc & 0x8000;
            if (c & i)
                bit = !bit;
            crc <<= 1;
            if (bit)
                crc ^= 0x8005;
        }
        crc &= 0xFFFF;
    }
    crc = crc_reflect(crc & 0xFFFF, 16) ^ 0x0000;
    return (crc >> 8) | (crc << 8); // swap bytes
}

File_Writer::File_Writer(Config&& config, DeviceItem* item, const QVariant& raw_data, uint32_t user_id, int interval, int part_size) :
    user_id_(user_id), address_(item->device() ? item->device()->address() : 0),
    interval_(std::max(interval, 0)), part_size_(part_size), item_(item)
{
    if (raw_data.type() == QVariant::String)
    {
        QString file_path = raw_data.toString();
        file_.setFileName(file_path);

        if (file_.open(QIODevice::ReadOnly))
        {
            QCryptographicHash hash(QCryptographicHash::Sha1);
            if (hash.addData(&file_))
            {
                if (hash.result() == item->raw_value().toByteArray())
                {
                    config.modbusTimeout *= 4;
                    config.modbusNumberOfRetries = 3;
                    Config::set(config, this);
                    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
                    return;
                }
                else
                    qCCritical(ModbusLog).nospace().noquote() << user_id << "| " << item->toString() << ' ' << QObject::tr("diffrent hash for file:") << ' '
                                                             << file_path << ' ' << hash.result().toHex() << ' ' << item->raw_value().toByteArray().toHex();
            }
            else
                qCCritical(ModbusLog).nospace().noquote() << user_id << "| " << QObject::tr("filed check hash for file:") << ' ' << file_path;
        }
        else
            qCCritical(ModbusLog).nospace().noquote() << user_id << "| " << QObject::tr("filed write file:") << ' ' << file_path << ' ' << file_.errorString();
    }
    else
        qCCritical(ModbusLog).nospace().noquote() << user_id << "| " << QObject::tr("filed file path:") << ' ' << raw_data;

    thread()->quit();
}

File_Writer::~File_Writer()
{
    if (file_.isOpen())
        file_.close();
    if (file_.exists())
        file_.remove();
}

void File_Writer::start()
{
    connect(this, &QModbusClient::errorOccurred, this, &File_Writer::modbus_error);
    connectDevice();

    file_.seek(0);
    write_file_part();
}

void File_Writer::modbus_error(QModbusDevice::Error e)
{
    if (e != QModbusDevice::NoError)
    {
        if (e == QModbusDevice::ConnectionError)
            thread()->quit();
    }
}

void File_Writer::write_file_part()
{
    qCDebug(ModbusLog).nospace() << "Flash firmware " << (file_.pos() / (file_.size() / 100)) << "% (" << file_.pos() << ", " << file_.size() << ')';

    QByteArray data(header_size, Qt::Uninitialized);
    int firmware_size = file_.size() + 2; // +CRC
    data[1] = (firmware_size >> 24) & 0xFF;
    data[2] = (firmware_size >> 16) & 0xFF;
    data[3] = (firmware_size >> 8) & 0xFF;
    data[4] =  firmware_size & 0xFF;

    data[5] = (file_.pos() >> 24) & 0xFF;
    data[6] = (file_.pos() >> 16) & 0xFF;
    data[7] = (file_.pos() >> 8) & 0xFF;
    data[8] = file_.pos() & 0xFF;

    data += file_.read(part_size_);
    uint8_t firmware_part_size = data.size() - header_size;

    if (file_.atEnd())
    {
        if ((firmware_part_size + 2) > part_size_)
        {
            firmware_part_size -= 2;
            file_.seek(file_.pos() - 2);
            data.resize(data.size() - 2);
        }
        else
        {
            file_.seek(0);
            QByteArray file_data = file_.readAll();
            quint16 checksum = calculateCRC(file_data.constData(), file_data.size());
            int pos = data.size();
            data.resize(pos + 2);
            data[pos] = (checksum >> 8) & 0xFF;
            data[pos+1] = checksum & 0xFF;
            firmware_part_size += 2;
        }
    }

    data[9] = firmware_part_size;
    data[0] = data.size() - 1;

//    qCDebug(ModbusLog).nospace() << "Header: 0x" << data.left(header_size).toHex().toUpper();
//    qCDebug(ModbusLog).nospace() << "Firmware: 0x" << data.right(firmware_part_size).toHex().toUpper();
//    qCDebug(ModbusLog).nospace() << "Application-level packet total " << data.size() << " bytes";

    auto *reply = sendRawRequest(QModbusRequest(QModbusPdu::WriteFileRecord, data), address_);
    process_reply(reply);
}

void File_Writer::process_reply(QModbusReply* reply)
{
    if (reply)
    {
        if (reply->isFinished())
        {
            reply_finish(reply);
        }
        else
        {
            connect(reply, &QModbusReply::finished, [this, reply]()
            {
                reply_finish(reply);
            });
        }
    }
    else
    {
        qCCritical(ModbusLog).nospace() << user_id_ << "| " << QObject::tr("Failed to write file request") << ' ' << errorString();
        thread()->quit();
    }
}

void File_Writer::reply_finish(QModbusReply* reply)
{
    if (reply->error() != QModbusDevice::NoError)
    {
        qCDebug(ModbusLog).noquote().nospace()
                << user_id_ << "| " << QObject::tr("Write file response error: %1 Device address: %2 (%3)")
                   .arg(reply->errorString())
                   .arg(address_)
                   .arg(reply->error() == QModbusDevice::ProtocolError ?
                        QObject::tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                        QObject::tr("code: 0x%1").arg(reply->error(), -1, 16));
        thread()->quit();
    }
    else
    {
        if (file_.atEnd())
        {
            qCInfo(ModbusLog).nospace().noquote() << user_id_ << "| " << QObject::tr("File sucessful sended.");
            thread()->quit();
        }
        else
        {
            QTimer::singleShot(interval_, this, SLOT(write_file_part()));
        }
    }

    reply->deleteLater();
}

} // namespace Modbus
} // namespace Dai
