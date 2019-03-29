#ifndef DAI_MODBUSPLUGIN_H
#define DAI_MODBUSPLUGIN_H

#include <QLoggingCategory>
#include <QModbusRtuSerialMaster>
//#include <QModbusDevice>
//#include <QModbusDataUnit>
//#include <QModbusRtuSerialMaster>
#include <QSerialPort>
#include <QEventLoop>

#include <memory>

#include <Dai/checkerinterface.h>

#include "modbusplugin_global.h"

namespace Dai {
namespace Modbus {

Q_DECLARE_LOGGING_CATEGORY(ModbusLog)

struct Conf {
    Conf& operator=(const Conf&) = default;
    Conf(const Conf&) = default;
    Conf(Conf&&) = default;
    Conf(const QString& portName = QString(),
         QSerialPort::BaudRate speed = QSerialPort::Baud115200,
         QSerialPort::DataBits bits_num = QSerialPort::Data8,
         QSerialPort::Parity parity = QSerialPort::NoParity,
         QSerialPort::StopBits stopBits = QSerialPort::OneStop,
         QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl,
         int modbusTimeout = 200, int modbusNumberOfRetries = 5, int frameDelayMicroseconds = 0) :
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
    }

    /*static QString firstPort() {
        return QSerialPortInfo::availablePorts().count() ? QSerialPortInfo::availablePorts().first().portName() : QString();
    }*/

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

class MODBUSPLUGINSHARED_EXPORT ModbusPlugin : public QModbusRtuSerialMaster, public CheckerInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DaiCheckerInterface_iid FILE "checkerinfo.json")
    Q_INTERFACES(Dai::CheckerInterface)

public:
    ModbusPlugin();
    ~ModbusPlugin();

    // CheckerInterface interface
public:
    void configure(QSettings* settings, Project*) override;
    bool check(Device *dev) override;
    void stop() override;
    void write(DeviceItem* item, const QVariant& raw_data, uint32_t user_id) override;

    void writeFile(uint serverAddress, const QString& fileName);
public slots:
    QVariantList read(int serverAddress, uchar regType = QModbusDataUnit::InputRegisters,
                      int startAddress = 0, quint16 unitCount = 1, bool clearCache = true);
private:

    bool checkConnect();

    int32_t unit(DeviceItem* item) const;

    std::unique_ptr<Conf> conf;

    typedef std::map<int, DeviceItem*> DevItems;
    typedef std::map<std::pair<int, QModbusDataUnit::RegisterType>, QModbusDevice::Error> StatusCacheMap;
    StatusCacheMap devStatusCache;

    QEventLoop wait_;
    bool b_break;
};

} // namespace Modbus
} // namespace Dai

#endif // DAI_MODBUSPLUGIN_H
