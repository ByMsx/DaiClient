#include <QDebug>
#include <QSettings>
#include <QFile>

#ifdef QT_DEBUG
#include <QtDBus>
#endif

#include <Helpz/settingshelper.h>
#include <Helpz/simplethread.h>

#include <Dai/db/item_type.h>
#include <Dai/deviceitem.h>
#include <Dai/device.h>

#include "modbus_file_writer.h"
#include "modbusplugin.h"

namespace Dai {
namespace Modbus {

Q_LOGGING_CATEGORY(ModbusLog, "modbus")

ModbusPlugin::ModbusPlugin() :
    QModbusRtuSerialMaster(),
    b_break(false)
{
}

ModbusPlugin::~ModbusPlugin()
{
}

const Config&ModbusPlugin::config() const
{
    return config_;
}

void ModbusPlugin::configure(QSettings *settings, Project *)
{
    using Helpz::Param;

    config_ = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Param<QString>,Param<QSerialPort::BaudRate>,Param<QSerialPort::DataBits>,
                            Param<QSerialPort::Parity>,Param<QSerialPort::StopBits>,Param<QSerialPort::FlowControl>,Param<int>,Param<int>,Param<int>>
        #endif
            (
                settings, "Modbus",
                Param<QString>{"Port", "ttyUSB0"},
                Param<QSerialPort::BaudRate>{"BaudRate", QSerialPort::Baud9600},
                Param<QSerialPort::DataBits>{"DataBits", QSerialPort::Data8},
                Param<QSerialPort::Parity>{"Parity", QSerialPort::NoParity},
                Param<QSerialPort::StopBits>{"StopBits", QSerialPort::OneStop},
                Param<QSerialPort::FlowControl>{"FlowControl", QSerialPort::NoFlowControl},
                Param<int>{"ModbusTimeout", timeout()},
                Param<int>{"ModbusNumberOfRetries", numberOfRetries()},
                Param<int>{"InterFrameDelay", interFrameDelay()}
    ).obj<Config>();

    auto firmware_tuple = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Param<int>,Param<int>>
        #endif
    {
        settings, "Modbus",
        Param<int>{"Firmware_Part_Size", 240},
        Param<int>{"Firmware_Part_Interval", 200},
    }();
    firmware_.part_size_ = std::get<0>(firmware_tuple);
    firmware_.part_interval_ = std::get<1>(firmware_tuple);

#if defined(QT_DEBUG) && defined(Q_OS_UNIX)
    if (QDBusConnection::sessionBus().isConnected())
    {
        QDBusInterface iface("ru.deviceaccess.Dai.Emulator", "/", "", QDBusConnection::sessionBus());
        if (iface.isValid())
        {
            QDBusReply<QString> tty_path = iface.call("ttyPath");
            if (tty_path.isValid())
                config_.name = tty_path.value(); // "/dev/ttyUSB0";
        }
        else
        {
            QString overTCP = "/home/kirill/vmodem0";
            config_.name = QFile::exists(overTCP) ? overTCP : "";//"/dev/pts/10";
        }
    }
#endif

    if (config_.name.isEmpty())
        config_.name = Config::getUSBSerial();

    qCDebug(ModbusLog) << "Used as serial port:" << config_.name;

    connect(this, &QModbusClient::errorOccurred, [this](Error e)
    {
        qCCritical(ModbusLog).noquote() << "Occurred:" << e << errorString();
        if (e == ConnectionError)
            disconnectDevice();
    });

    Config::set(config_, this);
}

bool ModbusPlugin::check(Device* dev)
{
    if (!checkConnect())
        return false;

    if (b_break)
        b_break = false;

    auto proccessRegister = [&](QModbusDataUnit::RegisterType registerType, const DevItems& itemMap)
    {
        if (itemMap.size())
        {
            int startAddress = 0;
            quint16 unitCount = 0;

            auto readValues = [&]() {
                if (unitCount)
                {
                    auto values = read(dev->address(), registerType, startAddress, unitCount, false);
                    for(int i = 0; i < values.size(); ++i)
                        QMetaObject::invokeMethod(itemMap.at(startAddress + i), "setRawValue", Qt::QueuedConnection,
                                                  Q_ARG(const QVariant&, values.at(i)));
                }
            };

            for(auto unit_it: itemMap)
            {
                if (startAddress + unitCount == unit_it.first)
                    ++unitCount;
                else
                {
                    readValues();

                    startAddress = unit_it.first;
                    unitCount = 1;
                }
            }

            readValues();
        }
    };

    std::map<QModbusDataUnit::RegisterType, DevItems> modbusInfoMap;
    DeviceItem* info_item = nullptr;

    int32_t unit_id;
    for (DeviceItem* item: dev->items())
//    for (int i = 0; i < dev->item_size(); ++i)
    {
//        DeviceItem* item = &dev->item(i);
        const auto rgsType = static_cast<QModbusDataUnit::RegisterType>(item->register_type());
        if (    rgsType > QModbusDataUnit::Invalid &&
                rgsType <= QModbusDataUnit::HoldingRegisters)
        {
            unit_id = unit(item);
            if (unit_id == -1)
                info_item = item;
            else
                modbusInfoMap[rgsType][unit_id] = item;
        }
    }

    if (info_item)
    {
        auto requestPair = std::make_pair(dev->address(), QModbusDataUnit::Invalid);
        if (devStatusCache.find(requestPair) == devStatusCache.cend()) {
            devStatusCache[requestPair] = QModbusDevice::NoError;

            QModbusReply *reply = sendRawRequest(QModbusRequest(QModbusPdu::ReportServerId), dev->address());
            if (reply)
            {
                auto setInfo = [this, reply, dev, info_item]() {
                    if (reply->error() == QModbusDevice::NoError)
                    {
                        quint16 slave_id = 0;
                        quint8 ver_major = 0, ver_minor = 0;

                        QByteArray info_data = reply->rawResult().data().right(4);
                        QDataStream ds(info_data);
//                        ds.setByteOrder(QDataStream::BigEndian);
                        ds >> slave_id >> ver_major >> ver_minor;

                        QString dev_info = QString("%1.%2 (Type: %3)").arg((int)ver_major).arg((int)ver_minor).arg(slave_id);
                        QMetaObject::invokeMethod(info_item, "setRawValue", Qt::QueuedConnection, Q_ARG(const QVariant&, dev_info));
                    }
                    else
                        qCWarning(ModbusLog).noquote() << tr("Failed to send info request: %1 Device address: %2 (%3)")
                                            .arg(reply->errorString()) .arg(dev->address()) .arg(reply->error() == QModbusDevice::ProtocolError ?
                                                                                                    tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                                                                                    tr("code: 0x%1").arg(reply->error(), -1, 16));
                    reply->deleteLater();
                };

                if (!reply->isFinished())
                    connect(reply, &QModbusReply::finished, setInfo);
                else
                    setInfo();
            }
            else
                qCCritical(ModbusLog) << "Failed to send info request:" << errorString() << "Device address:" << dev->address();
        }
    }
    
    for (auto modbusInfo: modbusInfoMap)
        if (!b_break)
            proccessRegister(modbusInfo.first, modbusInfo.second);

    return true;
}

void ModbusPlugin::stop() {
    b_break = true;

    if (wait_.isRunning())
        wait_.exit(1);
}

void ModbusPlugin::write(DeviceItem *item, const QVariant &raw_data, uint32_t user_id)
{
    if (!checkConnect())
        return;
    uint8_t item_register_type = item->register_type();
    if (item_register_type == Item_Type::rtFile)
    {
        disconnectDevice();

        using File_Writer_Thread = Helpz::ParamThread<File_Writer, Config, DeviceItem*, const QVariant&, uint32_t, int, int>;

        File_Writer_Thread file_writer_thread(config_, item, raw_data, user_id, firmware_.part_interval_, firmware_.part_size_);
        file_writer_thread.start(QThread::HighestPriority);
        file_writer_thread.wait();

        connectDevice();
        return;
    }

    auto regType = static_cast<QModbusDataUnit::RegisterType>( item_register_type );
    if (regType != QModbusDataUnit::Coils && regType != QModbusDataUnit::HoldingRegisters)
    {
        qCWarning(ModbusLog).noquote().nospace() << user_id << "|ERROR: Try to toggle not supported item.";
        return;
    }
    quint16 write_data;
    if (raw_data.type() == QVariant::Bool)
        write_data = raw_data.toBool() ? 1 : 0;
    else
        write_data = raw_data.toUInt();

    int32_t unit_id = unit(item);
    qCDebug(ModbusLog).noquote() << QString::number(user_id) + "|WRITE" << write_data << "TO" << item->toString() << "ADR"
                                 << item->device()->address() << "UNIT" << unit_id
                                 << (regType == QModbusDataUnit::Coils ? "Coils" : "HoldingRegisters");

    QModbusDataUnit writeUnit(regType, unit_id, 1);
    writeUnit.setValue(0, write_data);

    QEventLoop wait;

    if (auto *reply = sendWriteRequest(writeUnit, item->device()->address()))
    {

        if (!reply->isFinished())
        {
            connect(reply, &QModbusReply::finished, &wait, &QEventLoop::quit);
            wait.exec(QEventLoop::EventLoopExec);
        }

        if (!reply->isFinished())
            qCDebug(ModbusLog).noquote() << QString::number(user_id) + "|Write break";
        else if (reply->error() != NoError)
            qCWarning(ModbusLog).noquote() << tr("%1|Write response error: %2 Device address: %3 (%4) Function: %5 Unit: %6 Data:")
                          .arg(user_id)
                          .arg(reply->errorString())
                          .arg(item->device()->address())
                          .arg(reply->error() == ProtocolError ?
                                   tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                   tr("code: 0x%1").arg(reply->error(), -1, 16))
                          .arg(regType).arg(unit_id) << raw_data;

        reply->deleteLater();
    } else
        qCCritical(ModbusLog).noquote() << QString::number(user_id) + tr("|Write error: ") + this->errorString();
}

QVariantList ModbusPlugin::read(int serverAddress, uchar regType,
                                         int startAddress, quint16 unitCount, bool clearCache)
{
    QVariantList values;
    values.reserve(unitCount);
    for (quint16 i = 0; i < unitCount; ++i)
        values.push_back(QVariant());

    if (unitCount == 0)
        return values;

    auto registerType = static_cast<QModbusDataUnit::RegisterType>(regType);
    auto requestPair = std::make_pair(serverAddress, registerType);
    StatusCacheMap::iterator statusIt = devStatusCache.find(requestPair);

    if (clearCache && statusIt != devStatusCache.end())
    {
        devStatusCache.erase(statusIt);
        statusIt = devStatusCache.end();
    }

    struct ReadException {
        Error error;
        QString text;
    };

    try {
        if (state() != ConnectedState)
        {
            disconnectDevice();

            config_.name = Config::getUSBSerial();
            if (config_.name.isEmpty())
                throw ReadException{ ConnectionError, "USB Serial not found" };

            setConnectionParameter(SerialPortNameParameter, config_.name);

            if (!connectDevice())
                throw ReadException{ error(), tr("Connect to port %1 fail: %2").arg(config_.name).arg(errorString()) };
        }

        QModbusDataUnit request(registerType, startAddress, unitCount);
        std::unique_ptr<QModbusReply> reply(sendReadRequest(request, serverAddress));

        if (!reply)
            throw ReadException{ error(), errorString() };

        // broadcast replies return immediately
        if (!reply->isFinished())
        {
            wait_.connect(reply.get(), &QModbusReply::finished, &wait_, &QEventLoop::quit);
            wait_.exec(QEventLoop::EventLoopExec);
        }

        if (!reply->isFinished())
        {
            qCDebug(ModbusLog) << "Read break";
            return values;
        }

        if (reply->error() == QModbusDevice::NoError)
        {
//                    auto dbg = qDebug() << "Dev" << dev->address() << modbusInfo.first;
            const QModbusDataUnit unit = reply->result();
            for (uint i = 0; i < unit.valueCount() && i < unitCount; i++)
            {
                quint16 raw = unit.value(i);
//                        dbg << raw;
                // if (quint16(0xFFFF) != raw)
                {
                    if (registerType == QModbusDataUnit::Coils ||
                            registerType == QModbusDataUnit::DiscreteInputs)
                        values[i] = (bool)raw;
                    else
                        values[i] = (qint32)raw;
                }
//                    if (itemMap.at(startAddress + i)->id() == 44)
//                        qWarning() << "Modbus" << raw << (quint16(0xFFFF) != raw) << Prt::valueToVariant(value);
            }

            if (statusIt != devStatusCache.end())
            {
                qCDebug(ModbusLog) << "Modbus device " << statusIt->first.first << "recovered" << statusIt->second
                         << "Function:" << registerType << "Start:" << startAddress << "Value count:" << unitCount;
                devStatusCache.erase(statusIt);
            }
        }
        else
            throw ReadException{ reply->error(),
                    tr("%5 Device address: %1 (%6) Function: %2 Start: %3 Value count: %4")
                    .arg(serverAddress).arg(registerType).arg(startAddress).arg(unitCount)
                    .arg(reply->errorString())
                    .arg(reply->error() == QModbusDevice::ProtocolError ?
                           tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                           tr("code: 0x%1").arg(reply->error(), -1, 16)) };
    }
    catch(const ReadException& err)
    {
        if (statusIt == devStatusCache.end() || statusIt->second != err.error)
        {
            qCWarning(ModbusLog).noquote() << tr("Read response error:") << err.text;

            if (statusIt == devStatusCache.end())
                devStatusCache[requestPair] = err.error;
            else
                statusIt->second = err.error;
        }
    }
    return values;
}

bool ModbusPlugin::checkConnect()
{
    if (state() == ConnectedState || connectDevice())
        return true;
    qCCritical(ModbusLog).noquote() << "Connect failed." << this->errorString();
    return false;
}

int32_t ModbusPlugin::unit(DeviceItem *item) const
{
    return item->extra().value("unit").toInt();
}

} // namespace Modbus
} // namespace Dai
