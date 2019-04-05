#ifndef DAI_MODBUS_CONFIG_H
#define DAI_MODBUS_CONFIG_H

#include <QSerialPort>

QT_FORWARD_DECLARE_CLASS(QModbusRtuSerialMaster)

namespace Dai {
namespace Modbus {

struct Config {
    Config& operator=(const Config&) = default;
    Config(const Config&) = default;
    Config(Config&&) = default;
    Config(const QString& portName = QString(),
         QSerialPort::BaudRate speed = QSerialPort::Baud115200,
         QSerialPort::DataBits bits_num = QSerialPort::Data8,
         QSerialPort::Parity parity = QSerialPort::NoParity,
         QSerialPort::StopBits stopBits = QSerialPort::OneStop,
         QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl,
         int modbusTimeout = 200, int modbusNumberOfRetries = 5, int frameDelayMicroseconds = 0);

    /*static QString firstPort() {
        return QSerialPortInfo::availablePorts().count() ? QSerialPortInfo::availablePorts().first().portName() : QString();
    }*/

    static QStringList available_ports();

    static void set(const Config& config, QModbusRtuSerialMaster* device);

    static QString getUSBSerial();

    QString name;
    int baudRate;   ///< Скорость. По умолчанию 115200
    QSerialPort::DataBits   dataBits;       ///< Количество бит. По умолчанию 8
    QSerialPort::Parity     parity;         ///< Паритет. По умолчанию нет
    QSerialPort::StopBits   stopBits;       ///< Стоп бит. По умолчанию 1
    QSerialPort::FlowControl flowControl;   ///< Управление потоком. По умолчанию нет

    int modbusTimeout;
    int modbusNumberOfRetries;
    int frameDelayMicroseconds;
};

} // namespace Modbus
} // namespace Dai

#endif // CONFIG_H
