#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>

#include <QModbusRtuSerialSlave>
#include <QTcpSocket>

#include <vector>

//#include "simplemodbusdevice.h"
#include "device_table_item.h"
#include <plus/dai/database.h>

#include "Dai/project.h"
#include "Dai/type_managers.h"

class QProcess;
class QSettings;

void term_handler(int);

struct Config
{
    Config() :
        baud_rate_(QSerialPort::Baud115200),
        data_bits_(QSerialPort::Data8),
        parity_(QSerialPort::NoParity),
        stop_bits_(QSerialPort::OneStop),
        flow_control_(QSerialPort::NoFlowControl)
    {
    }

    QString name_;
    int baud_rate_;                           ///< Скорость. По умолчанию 115200
    QSerialPort::DataBits   data_bits_;       ///< Количество бит. По умолчанию 8
    QSerialPort::Parity     parity_;         ///< Паритет. По умолчанию нет
    QSerialPort::StopBits   stop_bits_;       ///< Стоп бит. По умолчанию 1
    QSerialPort::FlowControl flow_control_;   ///< Управление потоком. По умолчанию нет
};

namespace Ui
{
class Main_Window;
}

class DBusTTY: public QObject
{
    Q_OBJECT
    QString path_;
public:
    void setPath(const QString &path);
    Q_INVOKABLE QString ttyPath() const;
};

class Main_Window : public QMainWindow
{
    Q_OBJECT

public:
    explicit Main_Window(QWidget *parent = 0);
    ~Main_Window();
private slots:
    void handleDeviceError(QModbusDevice::Error newError);
    void onStateChanged(int state);

    void changeTemperature();
    void proccessData();
    void socketDataReady();
    void on_openBtn_toggled(bool open);
    void on_socatReset_clicked();

private:

    struct Socat_Info
    {
//        SocatInfo() : socat(nullptr) {}
//        ~SocatInfo() { delete socat; }
        QProcess* socat_process_;
        QString port_from_name_;
        QString port_to_name_;
    };    

    struct Modbus_Device_Item : Socat_Info
    {
        QModbusRtuSerialSlave* modbus_device_;
        QSerialPort* serial_port_;
        DeviceTableItem* device_table_item_;

        void operator =(const Socat_Info& info)
        {
            socat_process_ = info.socat_process_;
            port_from_name_ = info.port_from_name_;
            port_to_name_ = info.port_to_name_;
        }
    };

    Ui::Main_Window *ui_;

    Dai::Database::Helper db_manager_;
    Dai::Project dai_project_;

    QTimer temp_timer_;

    QSerialPort* serial_port_;
    QProcess* socat_process_;
    DBusTTY* dbus_ = nullptr;

    std::map<uchar, Modbus_Device_Item> modbus_list_;

    Config config_;

    void init() noexcept;
    void init_database() noexcept;
    void fill_data() noexcept;

    QString get_user() noexcept;

    Socat_Info create_socat();
    void init_client_connection();

    friend void term_handler(int);
};

#endif // MAINWINDOW_H
