#include <QModbusRtuSerialMaster>
#include <QSerialPortInfo>
#include <QVariant>
#include <QLoggingCategory>

#include "config.h"

Q_LOGGING_CATEGORY(ModbusLog, "modbus")

namespace Dai {
namespace Modbus {

Config::Config(const QString& portName, QSerialPort::BaudRate speed, QSerialPort::DataBits bits_num, QSerialPort::Parity parity, QSerialPort::StopBits stopBits, QSerialPort::FlowControl flowControl, int modbusTimeout, int modbusNumberOfRetries, int frameDelayMicroseconds) :
    name(portName),
    baudRate(speed),
    dataBits(bits_num),
    parity(parity),
    stopBits(stopBits),
    flowControl(flowControl),

    modbusTimeout(modbusTimeout),
    modbusNumberOfRetries(modbusNumberOfRetries),
    frameDelayMicroseconds(frameDelayMicroseconds)
{
    if (ModbusLog().isDebugEnabled())
    {
        auto dbg = QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, ModbusLog().categoryName()).debug()
                << "Available port list:";
        for (auto&& port: QSerialPortInfo::availablePorts())
            dbg << port.portName();
    }
}

void Config::set(const Config& config, QModbusRtuSerialMaster* device)
{
    device->setConnectionParameter(QModbusDevice::SerialPortNameParameter, config.name);
    device->setConnectionParameter(QModbusDevice::SerialParityParameter,   config.parity);
    device->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, config.baudRate);
    device->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, config.dataBits);
    device->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, config.stopBits);

    device->setTimeout(config.modbusTimeout);
    device->setNumberOfRetries(config.modbusNumberOfRetries);

    if (config.frameDelayMicroseconds > 0)
        device->setInterFrameDelay(config.frameDelayMicroseconds);
}

/*static*/ QString Config::getUSBSerial()
{
    for (auto& it: QSerialPortInfo::availablePorts())
        if (it.portName().indexOf("ttyUSB") != -1)
            return it.portName();
    return QString();
}

} // namespace Modbus
} // namespace Dai
