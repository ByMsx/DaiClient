#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>

#include <QModbusRtuSerialSlave>
#include <QTcpSocket>

#include <vector>

//#include "simplemodbusdevice.h"
#include "Database/db_manager.h"
#include "Dai/project.h"
#include "Dai/typemanager/typemanager.h"

class QProcess;
class QSettings;

void term_handler(int);

struct Conf {
    Conf() :
        baudRate(QSerialPort::Baud115200),
        dataBits(QSerialPort::Data8),
        parity(QSerialPort::NoParity),
        stopBits(QSerialPort::OneStop),
        flowControl(QSerialPort::NoFlowControl)
    {
    }

    QString name;
    int baudRate;   ///< Скорость. По умолчанию 115200
    QSerialPort::DataBits   dataBits;       ///< Количество бит. По умолчанию 8
    QSerialPort::Parity     parity;         ///< Паритет. По умолчанию нет
    QSerialPort::StopBits   stopBits;       ///< Стоп бит. По умолчанию 1
    QSerialPort::FlowControl flowControl;   ///< Управление потоком. По умолчанию нет
};

namespace Ui {
class MainWindow;
}

class DBusTTY: public QObject
{
    Q_OBJECT
    QString m_path;
public:
    void setPath(const QString &path);
    Q_INVOKABLE QString ttyPath() const;
};

class MainBox;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private slots:
    void handleDeviceError(QModbusDevice::Error newError);
    void onStateChanged(int state);

    void changeTemperature();
    void proccessData();
    void socketDataReady();
    void on_openBtn_toggled(bool open);
    void on_socatReset_clicked();
private:
    void init_Database(QSettings *s);
    void init();


    struct SocatInfo {
//        SocatInfo() : socat(nullptr) {}
//        ~SocatInfo() { delete socat; }
        QProcess* socat;
        QString portFromName;
        QString portToName;
    };
    SocatInfo createSocat();

    Ui::MainWindow *ui;

    Dai::DBManager db_mng;
    Dai::Project prj;
//    Dai::TypeManagers mng;
//    Dai::Devices devs;

    std::map<uint, std::map<uchar, QByteArray>> tmp;

    QTimer timer;
    QSerialPort* m_serialPort;

    QProcess* m_socat;
    DBusTTY* dbus = nullptr;

    struct ModbusDeviceItem : SocatInfo {
        QModbusRtuSerialSlave* device;
        QSerialPort* serialPort;
        MainBox* box;

        void operator =(const SocatInfo& info) {
            socat = info.socat;
            portFromName = info.portFromName;
            portToName = info.portToName;
        }
    };

    std::map<uchar, ModbusDeviceItem> modbus_list;

    Conf conf;

    friend void term_handler(int);
};

#endif // MAINWINDOW_H
