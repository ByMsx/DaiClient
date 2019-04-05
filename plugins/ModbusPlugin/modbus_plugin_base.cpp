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

#include "modbus_plugin_base.h"

namespace Dai {
namespace Modbus {

Q_LOGGING_CATEGORY(ModbusLog, "modbus")

Modbus_Plugin_Base::Modbus_Plugin_Base() :
    QModbusRtuSerialMaster(),
    b_break(false)
{
}

Modbus_Plugin_Base::~Modbus_Plugin_Base()
{
}

bool Modbus_Plugin_Base::check_break_flag() const
{
    return b_break;
}

void Modbus_Plugin_Base::clear_break_flag()
{
    if (b_break)
        b_break = false;
}

const Config&Modbus_Plugin_Base::config() const
{
    return config_;
}

bool Modbus_Plugin_Base::checkConnect()
{
    if (state() == ConnectedState || connectDevice())
        return true;
    qCCritical(ModbusLog).noquote() << "Connect failed." << this->errorString();
    return false;
}

void Modbus_Plugin_Base::configure(QSettings *settings, Project *)
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

bool Modbus_Plugin_Base::check(Device* dev)
{
    if (!checkConnect())
        return false;

    clear_break_flag();

    std::map<QModbusDataUnit::RegisterType, DevItems> modbusInfoMap;

    for (DeviceItem* item: dev->items())
    {
        const auto rgsType = static_cast<QModbusDataUnit::RegisterType>(item->register_type());
        if (    rgsType > QModbusDataUnit::Invalid &&
                rgsType <= QModbusDataUnit::HoldingRegisters)
        {
            modbusInfoMap[rgsType][unit(item)] = item;
        }
    }

    for (auto modbusInfo: modbusInfoMap)
        if (!b_break)
            proccessRegister(dev, modbusInfo.first, modbusInfo.second);

    return true;
}

void Modbus_Plugin_Base::stop()
{
    b_break = true;

    if (wait_.isRunning())
        wait_.exit(1);
}

void Modbus_Plugin_Base::write(std::vector<Write_Cache_Item>& items)
{
    if (!checkConnect() || !items.size())
        return;

    std::map<Device*, std::map<QModbusDataUnit::RegisterType, std::vector<Write_Cache_Item*>>> write_map;
    for (Write_Cache_Item& item: items)
    {
        auto reg_type = static_cast<QModbusDataUnit::RegisterType>( item.dev_item_->register_type() );
        if (reg_type != QModbusDataUnit::Coils && reg_type != QModbusDataUnit::HoldingRegisters)
            continue;

        write_map[item.dev_item_->device()][reg_type].push_back(&item);
    }

    for (auto& it: write_map)
    {
        for (auto& r_it: it.second)
        {
            write_item_pack(it.first, r_it.first, r_it.second);
        }
    }
}

void Modbus_Plugin_Base::write_item_pack(Device* dev, QModbusDataUnit::RegisterType reg_type, std::vector<Write_Cache_Item*>& items)
{
    if (reg_type != QModbusDataUnit::Coils && reg_type != QModbusDataUnit::HoldingRegisters)
    {
        qCWarning(ModbusLog) << "ERROR: Try to toggle not supported item.";
        return;
    }

    int dev_address = address(dev);
    if (dev_address < 0)
        return;

    std::sort(items.begin(), items.end(), [this](Write_Cache_Item* item, Write_Cache_Item* item_b)
    {
        return unit(item->dev_item_) < unit(item_b->dev_item_);
    });

    quint16 write_data;
    int32_t unit_id;

    std::map<uint32_t, std::pair<int32_t, QVector<quint16>>> item_units;
    decltype(item_units)::iterator it;

    for (Write_Cache_Item* item: items)
    {
        unit_id = unit(item->dev_item_);
        if (unit_id >= 0)
        {
            if (item->raw_value_.type() == QVariant::Bool)
                write_data = item->raw_value_.toBool() ? 1 : 0;
            else
                write_data = item->raw_value_.toUInt();

            it = item_units.find(unit_id);
            if (it != item_units.end())
            {
                it->second.second.push_back(write_data);
            }
            else
            {
                item_units.emplace(unit_id + 1, std::make_pair(unit_id, QVector<quint16>{write_data}));
            }

            qCDebug(ModbusLog).noquote() << QString::number(item->user_id_) + "|WRITE" << write_data << "TO" << item->dev_item_->toString() << "ADR"
                                         << dev_address << "UNIT" << unit_id
                                         << (reg_type == QModbusDataUnit::Coils ? "Coils" : "HoldingRegisters");
        }
    }

    for (auto& iter: item_units)
    {
        write_multi_item(dev_address, reg_type, iter.second.first, iter.second.second);
    }
}

void Modbus_Plugin_Base::write_multi_item(int server_address, QModbusDataUnit::RegisterType reg_type, int start_address, const QVector<quint16>& values)
{
    if (!values.size())
        return;

    QModbusDataUnit write_unit(reg_type, start_address, values);

    QEventLoop wait;

    if (auto *reply = sendWriteRequest(write_unit, server_address))
    {
        if (!reply->isFinished())
        {
            connect(reply, &QModbusReply::finished, &wait, &QEventLoop::quit);
            wait.exec(QEventLoop::EventLoopExec);
        }

        if (!reply->isFinished())
            qCDebug(ModbusLog) << "Write break";
        else if (reply->error() != NoError)
            qCWarning(ModbusLog).noquote() << tr("Write response error: %1 Device address: %2 (%3) Function: %4 Start unit: %5 Data:")
                          .arg(reply->errorString())
                          .arg(server_address)
                          .arg(reply->error() == ProtocolError ?
                                   tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                   tr("code: 0x%1").arg(reply->error(), -1, 16))
                          .arg(reg_type).arg(start_address) << values;

        reply->deleteLater();
    }
    else
        qCCritical(ModbusLog).noquote() << tr("Write error: ") + this->errorString();
}

QVariantList Modbus_Plugin_Base::read(int serverAddress, uchar regType,
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
    StatusCacheMap::iterator statusIt = dev_status_cache_.find(requestPair);

    if (clearCache && statusIt != dev_status_cache_.end())
    {
        dev_status_cache_.erase(statusIt);
        statusIt = dev_status_cache_.end();
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

            if (statusIt != dev_status_cache_.end())
            {
                qCDebug(ModbusLog) << "Modbus device " << statusIt->first.first << "recovered" << statusIt->second
                         << "Function:" << registerType << "Start:" << startAddress << "Value count:" << unitCount;
                dev_status_cache_.erase(statusIt);
            }
        }
        else
            throw ReadException{ reply->error(),
                    tr("%5 Device address: %1 (%6) registerType: %2 Start: %3 Value count: %4")
                    .arg(serverAddress).arg(registerType).arg(startAddress).arg(unitCount)
                    .arg(reply->errorString())
                    .arg(reply->error() == QModbusDevice::ProtocolError ?
                           tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                           tr("code: 0x%1").arg(reply->error(), -1, 16)) };
    }
    catch(const ReadException& err)
    {
        if (statusIt == dev_status_cache_.end() || statusIt->second != err.error)
        {
            qCWarning(ModbusLog).noquote() << tr("Read response error:") << err.text;

            if (statusIt == dev_status_cache_.end())
                dev_status_cache_[requestPair] = err.error;
            else
                statusIt->second = err.error;
        }
    }
    return values;
}

QStringList Modbus_Plugin_Base::available_ports() const
{
    return Config::available_ports();
}

void Modbus_Plugin_Base::proccessRegister(Device *dev, QModbusDataUnit::RegisterType registerType, const DevItems& itemMap)
{
    int32_t dev_address = address(dev);

    if (dev_address >= 0 && itemMap.size())
    {
        int startAddress = 0;
        quint16 unitCount = 0;

        auto readValues = [&]()
        {
            if (unitCount)
            {
                auto values = read(dev_address, registerType, startAddress, unitCount, false);
                for (int i = 0; i < values.size(); ++i)
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
}

int32_t Modbus_Plugin_Base::address(Device *dev, bool* ok) const
{
    QVariant v = dev->param("address");
    return v.isValid() ? v.toInt(ok) : -2;
}

int32_t Modbus_Plugin_Base::unit(DeviceItem *item, bool* ok) const
{
    QVariant v = item->param("unit");
    return v.isValid() ? v.toInt(ok) : -2;
}

} // namespace Modbus
} // namespace Dai
