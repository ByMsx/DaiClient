#include <QDebug>
#include <QSettings>
#include <QSerialPortInfo>
#include <QFile>

#ifdef QT_DEBUG
#include <QtDBus>
#endif

#include <Helpz/settingshelper.h>
#include <Dai/deviceitem.h>

#include "modbusplugin.h"

namespace Dai {
namespace Modbus {

Q_LOGGING_CATEGORY(ModbusLog, "modbus")

// ----------

/*static*/ QString Conf::getUSBSerial() {
    for (auto& it: QSerialPortInfo::availablePorts())
        if (it.portName().indexOf("ttyUSB") != -1)
            return it.portName();
    return QString();
}

// ----------

ModbusPlugin::ModbusPlugin() :
    QModbusRtuSerialMaster(),
    b_break(false)
{
    qDebug() << "ModbusPlugin" << this;
}

ModbusPlugin::~ModbusPlugin()
{
    qDebug() << "~ModbusPlugin" << this;
}

void ModbusPlugin::configure(QSettings *settings, SectionManager *)
{
    using Helpz::Param;
    conf = Helpz::SettingsHelper(
                settings, "Modbus",
                Param{"Port", "ttyUSB0"},
                Param{"BaudRate", QSerialPort::Baud9600},
                Param{"DataBits", QSerialPort::Data8},
                Param{"Parity", QSerialPort::NoParity},
                Param{"StopBits", QSerialPort::OneStop},
                Param{"FlowControl", QSerialPort::NoFlowControl},
                Param{"ModbusTimeout", 200},
                Param{"ModbusNumberOfRetries", 5},
                Param{"InterFrameDelay", 0}
    ).unique_ptr<Conf>();

//    conf = std::unique_ptr<Conf>{
//            Helpz::SettingsHelper<Conf,
//            QString, QSerialPort::BaudRate, QSerialPort::DataBits, QSerialPort::Parity, QSerialPort::StopBits, QSerialPort::FlowControl, int, int, int >
//            {settings, "Modbus"}
//            (   Param{"Port", "ttyUSB0"},
//                Param{"BaudRate", QSerialPort::Baud9600},
//                Param{"DataBits", QSerialPort::Data8},
//                Param{"Parity", QSerialPort::NoParity},
//                Param{"StopBits", QSerialPort::OneStop},
//                Param{"FlowControl", QSerialPort::NoFlowControl},
//                Param{"ModbusTimeout", 200},
//                Param{"ModbusNumberOfRetries", 5},
//                Param{"InterFrameDelay", 0}
//            )};

#ifdef QT_DEBUG
    if (QDBusConnection::sessionBus().isConnected())
    {
        QDBusInterface iface("ru.deviceaccess.Dai.Emulator", "/", "", QDBusConnection::sessionBus());
        if (iface.isValid())
        {
            QDBusReply<QString> tty_path = iface.call("ttyPath");
            if (tty_path.isValid())
                conf->name = tty_path.value(); // "/dev/ttyUSB0";
        }
        else
        {
            QString overTCP = "/home/kirill/vmodem0";
            conf->name = QFile::exists(overTCP) ? overTCP : "";//"/dev/pts/10";
        }
    }
#endif

    if (conf->name.isEmpty())
        conf->name = Conf::getUSBSerial();

    qCDebug(ModbusLog) << "Used as serial port:" << conf->name;

    if (ModbusLog().isDebugEnabled())
    {
        auto dbg = QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, ModbusLog().categoryName()).debug()
                << "Available port list:";
        for (auto&& port: QSerialPortInfo::availablePorts())
            dbg << port.portName();
    }

    connect(this, &QModbusClient::errorOccurred, [this](Error e) {
        qCCritical(ModbusLog).noquote() << "Occurred:" << e << errorString();
        if (e == ConnectionError)
            disconnectDevice();
    });

    setConnectionParameter(SerialPortNameParameter, conf->name);
    setConnectionParameter(SerialParityParameter,   conf->parity);
    setConnectionParameter(SerialBaudRateParameter, conf->baudRate);
    setConnectionParameter(SerialDataBitsParameter, conf->dataBits);
    setConnectionParameter(SerialStopBitsParameter, conf->stopBits);

    setTimeout(conf->modbusTimeout);
    setNumberOfRetries(conf->modbusNumberOfRetries);

    if (conf->frameDelayMicroseconds > 0)
        setInterFrameDelay(conf->frameDelayMicroseconds);
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

    for (DeviceItem* item: dev->items())
//    for (int i = 0; i < dev->item_size(); ++i)
    {
//        DeviceItem* item = &dev->item(i);
        const auto rgsType = static_cast<QModbusDataUnit::RegisterType>(item->registerType());
        if (    rgsType > QModbusDataUnit::Invalid &&
                rgsType <= QModbusDataUnit::HoldingRegisters)
            modbusInfoMap[rgsType][item->unit()] = item;
    }

    for (auto modbusInfo: modbusInfoMap)
        if (!b_break)
            proccessRegister(modbusInfo.first, modbusInfo.second);

    return true;
}

void ModbusPlugin::stop() {
    b_break = true;

    if (wait.isRunning())
        wait.exit(1);
}

void ModbusPlugin::write(DeviceItem *item, const QVariant &raw_data)
{
    if (!checkConnect())
        return;

    auto regType = static_cast<QModbusDataUnit::RegisterType>( item->registerType() );
    if (regType != QModbusDataUnit::Coils && regType != QModbusDataUnit::HoldingRegisters)
    {
        qCWarning(ModbusLog) << "ERROR: Try to toggle not supported item.";
        return;
    }
    quint16 write_data;
    if (raw_data.type() == QVariant::Bool)
        write_data = raw_data.toBool() ? 1 : 0;
    else
        write_data = raw_data.toUInt();

    qCDebug(ModbusLog) << "WRITE" << write_data << "TO" << item->toString() << "ADR" << item->device()->address() << "UNIT" << item->unit()
                       << (regType == QModbusDataUnit::Coils ? "Coils" : "HoldingRegisters");

    QModbusDataUnit writeUnit(regType, item->unit(), 1);
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
            qCDebug(ModbusLog) << "Write break";
        else if (reply->error() != NoError)
            qCWarning(ModbusLog).noquote() << tr("Write response error: %1 Device address: %2 (%3) Function: %4 Unit: %5 Data:")
                          .arg(reply->errorString())
                          .arg(item->device()->address())
                          .arg(reply->error() == ProtocolError ?
                                   tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                   tr("code: 0x%1").arg(reply->error(), -1, 16))
                          .arg(regType).arg(item->unit()) << raw_data;

        reply->deleteLater();
    } else
        qCCritical(ModbusLog).noquote() << tr("Write error: ") + this->errorString();
}

void ModbusPlugin::writeFile(uint serverAddress, const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        qCCritical(ModbusLog).noquote() << "Fail write file" << file.errorString();
        return;
    }

    char requestHeaders[] = {
        0x06,       // Reference Type
        0x00, 0x01, // File Number
        0x00, 0x00, // Record Number
        0x00, 0x00  // Record length
    };

    QEventLoop wait;

    std::function<void()> writeFilePart = [&]() {
        requestHeaders[3] = file.pos() & 0xFF;
        requestHeaders[4] = file.pos() >> 8;

        QByteArray data = file.read(253 - sizeof(requestHeaders));

        if (data.size() % 2 != 0)
            data.resize(data.size() + 1);

        quint16 recordLength = data.size() / 2;

        requestHeaders[5] = recordLength & 0xFF;
        requestHeaders[6] = recordLength >> 8;

        if (auto *reply = sendRawRequest(QModbusRequest(QModbusPdu::WriteFileRecord, QByteArray(requestHeaders, sizeof(requestHeaders)) + data), serverAddress))
        {
            if (!reply->isFinished())
            {
                connect(reply, &QModbusReply::finished, &wait, &QEventLoop::quit);
                wait.exec(QEventLoop::EventLoopExec);
            }

            if (reply->error() == NoError)
            {
                if (file.atEnd())
                {

                }
                else
                    writeFilePart();
            }
            else
                qCWarning(ModbusLog).noquote() << tr("Write file response error: %1 Device address: %2 (%3)")
                              .arg(reply->errorString()) .arg(serverAddress) .arg(reply->error() == ProtocolError ?
                                       tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                       tr("code: 0x%1").arg(reply->error(), -1, 16));

            reply->deleteLater();
        }
    };

    writeFilePart();
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

            conf->name = Conf::getUSBSerial();
            if (conf->name.isEmpty())
                throw ReadException{ ConnectionError, "USB Serial not found" };

            setConnectionParameter(SerialPortNameParameter, conf->name);

            if (!connectDevice())
                throw ReadException{ error(), tr("Connect to port %1 fail: %2").arg(conf->name).arg(errorString()) };
        }

        QModbusDataUnit request(registerType, startAddress, unitCount);
        std::unique_ptr<QModbusReply> reply(sendReadRequest(request, serverAddress));

        if (!reply)
            throw ReadException{ error(), errorString() };

        // broadcast replies return immediately
        if (!reply->isFinished())
        {
            wait.connect(reply.get(), &QModbusReply::finished, &wait, &QEventLoop::quit);
            wait.exec(QEventLoop::EventLoopExec);
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

void ModbusPlugin::writeFilePart()
{
//    sendRawRequest()
}

bool ModbusPlugin::checkConnect()
{
    if (state() == ConnectedState || connectDevice())
        return true;
    qCCritical(ModbusLog).noquote() << "Connect failed." << this->errorString();
    return false;
}

} // namespace Modbus
} // namespace Dai
