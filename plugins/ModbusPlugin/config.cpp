#include <QModbusRtuSerialMaster>
#include <QSerialPortInfo>
#include <QVariant>

#include "config.h"

namespace Dai {
namespace Modbus {

Config::Config(const QString& portName, QSerialPort::BaudRate speed, QSerialPort::DataBits bits_num, QSerialPort::Parity parity,
               QSerialPort::StopBits stopBits, QSerialPort::FlowControl flowControl, int modbusTimeout, int modbusNumberOfRetries,
               int frameDelayMicroseconds, int line_use_timeout) :
    name(portName),
    baudRate(speed),
    dataBits(bits_num),
    parity(parity),
    stopBits(stopBits),
    flowControl(flowControl),

    modbusTimeout(modbusTimeout),
    modbusNumberOfRetries(modbusNumberOfRetries),
    frameDelayMicroseconds(frameDelayMicroseconds),
    line_use_timeout_(line_use_timeout)
{
}

QStringList Config::available_ports()
{
    QStringList ports;
    for (auto&& port: QSerialPortInfo::availablePorts())
        ports << port.portName();
    return ports;
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
