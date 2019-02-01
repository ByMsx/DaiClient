#include <QSettings>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QtDBus>

#include <QHostAddress>

#include <csignal>
#include <iostream>

#include "mainbox.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QGroupBox>

#include "device_item_view.h"

namespace GH = ::Dai;

void DBusTTY::setPath(const QString &path) { path_ = path; }
QString DBusTTY::ttyPath() const { return path_; }

Main_Window* obj = nullptr;

void term_handler(int)
{
    if (obj)
    {
        obj->socat_process_->terminate();
        obj->socat_process_->waitForFinished();
        delete obj->socat_process_;

        for (auto&& item: obj->modbus_list_)
        {
            item.second.serial_port_->close();
            item.second.socat_process_->terminate();
            item.second.socat_process_->waitForFinished();
            delete item.second.socat_process_;
        }
    }
    std::cerr << "Termination.\n";
}

Main_Window::Main_Window(QWidget *parent) : QMainWindow(parent), ui_(new Ui::Main_Window),
    db_manager_(), dai_project_(&db_manager_),
    socat_process_(nullptr)
{
    obj = this;
    ui_->setupUi(this);

    init();              
}



void Main_Window::init() noexcept
{
    std::signal(SIGTERM, term_handler);
    std::signal(SIGINT, term_handler);
    std::signal(SIGKILL, term_handler);

    config_.baud_rate_ = QSerialPort::Baud9600;
    qsrand(QDateTime::currentDateTime().toTime_t());

    init_database();
    db_manager_.initProject(&dai_project_);

    fill_data();

    init_client_connection();

    connect(&temp_timer_, &QTimer::timeout, this, &Main_Window::changeTemperature);
    temp_timer_.setInterval(3000);
    temp_timer_.start();



    return; // -----------------------------------------

    auto box = static_cast<QGridLayout*>(ui_->content->layout());
    for (GH::Device* dev: dai_project_.devices())
    {
        if (dev->items().size() == 0 ||
                dev->items().first()->type() == GH::Prt::itProcessor)
            continue;

        Modbus_Device_Item item;
        item = create_socat();
        item.modbus_device_ = new QModbusRtuSerialSlave(this);
        item.modbus_device_->setConnectionParameter(QModbusDevice::SerialPortNameParameter, item.port_from_name_ );
        item.modbus_device_->setConnectionParameter(QModbusDevice::SerialParityParameter,   config_.parity_);
        item.modbus_device_->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, config_.baud_rate_);
        item.modbus_device_->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, config_.data_bits_);
        item.modbus_device_->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, config_.stop_bits_);
        item.modbus_device_->setServerAddress(dev->address());

//        connect(modbusDevice, &QModbusServer::stateChanged,
//                this, &MainWindow::onStateChanged);
        connect(item.modbus_device_, &QModbusServer::errorOccurred, this, &Main_Window::handleDeviceError);

        item.serial_port_ = new QSerialPort(this);
        item.serial_port_->setPortName(item.port_to_name_);
        item.serial_port_->setBaudRate(   config_.baud_rate_);
        item.serial_port_->setDataBits(   config_.data_bits_);
        item.serial_port_->setParity(     config_.parity_);
        item.serial_port_->setStopBits(   config_.stop_bits_);
        item.serial_port_->setFlowControl(config_.flow_control_);

        if (!item.serial_port_->open(QIODevice::ReadWrite))
            qCritical() << item.serial_port_->errorString();

        connect(item.serial_port_, &QSerialPort::readyRead, this, &Main_Window::socketDataReady);

        item.box_ = new MainBox(&dai_project_.ItemTypeMng, dev, item.modbus_device_, ui_->content);
        modbus_list_.emplace(dev->address(), item);

        box->addWidget( item.box_, box->rowCount(), 0, 1, 2 );
    }
    box->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding), box->rowCount(), 0);
}

void Main_Window::init_database() noexcept
{
    QSettings dai_settings("DaiClient.conf", QSettings::NativeFormat);
    dai_settings.beginGroup("Database");
    qDebug() << "Open SQL is" << db_manager_.createConnection({
                    dai_settings.value("Name", "dai_main").toString(),
                    dai_settings.value("User", "DaiUser").toString(),
                    dai_settings.value("Password", "?").toString() });
    dai_settings.endGroup();
}

void Main_Window::fill_data() noexcept
{
    auto box = static_cast<QGridLayout*>(ui_->content->layout());

    for (GH::Device* dev: dai_project_.devices())
    {
        if (dev->items().size() > 0 && dev->items().first()->type() != GH::Prt::itProcessor)
        {
            Modbus_Device_Item modbus_device_item;
            modbus_device_item = create_socat();
            modbus_device_item.modbus_device_ = new QModbusRtuSerialSlave(this);
            modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialPortNameParameter, modbus_device_item.port_from_name_);
            modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialParityParameter, config_.parity_);
            modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, config_.baud_rate_);
            modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, config_.data_bits_);
            modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, config_.stop_bits_);
            modbus_device_item.modbus_device_->setServerAddress(dev->address());

    //        connect(modbusDevice, &QModbusServer::stateChanged,
    //                this, &MainWindow::onStateChanged);
            connect(modbus_device_item.modbus_device_, &QModbusServer::errorOccurred, this, &Main_Window::handleDeviceError);

            modbus_device_item.serial_port_ = new QSerialPort(this);
            modbus_device_item.serial_port_->setPortName(modbus_device_item.port_to_name_);
            modbus_device_item.serial_port_->setBaudRate(config_.baud_rate_);
            modbus_device_item.serial_port_->setDataBits(config_.data_bits_);
            modbus_device_item.serial_port_->setParity(config_.parity_);
            modbus_device_item.serial_port_->setStopBits(config_.stop_bits_);
            modbus_device_item.serial_port_->setFlowControl(config_.flow_control_);

            if (!modbus_device_item.serial_port_->open(QIODevice::ReadWrite))
                qCritical() << modbus_device_item.serial_port_->errorString();

            connect(modbus_device_item.serial_port_, &QSerialPort::readyRead, this, &Main_Window::socketDataReady);

            modbus_device_item.device_item_view_ = new Device_Item_View(&dai_project_.ItemTypeMng, dev, ui_->content);
            box->addWidget(modbus_device_item.device_item_view_);

            modbus_list_.emplace(dev->address(), modbus_device_item);
        }
    }
}

void Main_Window::init_client_connection()
{
    serial_port_ = new QSerialPort(this);
    connect(serial_port_, &QSerialPort::readyRead, this, &Main_Window::proccessData);

    QSettings s;
    ui_->portName->setText(s.value("portName", "/dev/pts/").toString());

    if (QDBusConnection::sessionBus().isConnected())
    {
        if (QDBusConnection::sessionBus().registerService("ru.deviceaccess.Dai.Emulator"))
        {
            dbus_ = new DBusTTY;
            QDBusConnection::sessionBus().registerObject("/", dbus_, QDBusConnection::ExportAllInvokables);
        }
        else
        {
            qCritical() << "Уже зарегистрирован";
        }
    }

    on_socatReset_clicked();
}

Main_Window::Socat_Info Main_Window::create_socat()
{
    Socat_Info info;

    info.socat_process_ = new QProcess;
    info.socat_process_->setProcessChannelMode(QProcess::MergedChannels);
    info.socat_process_->setReadChannel(QProcess::StandardOutput);
    info.socat_process_->setProgram("/usr/bin/socat");
    // TODO need take the username from GUI
    info.socat_process_->setArguments(QStringList() << "-d" << "-d" << "pty,raw,echo=0,user=andrey" << "pty,raw,echo=0,user=andrey");

    auto dbg = qDebug() << info.socat_process_->program() << info.socat_process_->arguments();
    info.socat_process_->start(QIODevice::ReadOnly);
    if (info.socat_process_->waitForStarted() && info.socat_process_->waitForReadyRead() && info.socat_process_->waitForReadyRead())
    {
        auto data = info.socat_process_->readAllStandardOutput();
        QRegularExpression re("/dev/pts/\\d+");
        auto it = re.globalMatch(data);

        QStringList args;
        while (it.hasNext())
            args << it.next().captured(0);

        if (args.count() == 2)
        {
            info.port_from_name_ = args.at(0);
            info.port_to_name_ = args.at(1);
            dbg << info.port_from_name_ << info.port_to_name_;
        }
        else
            qCritical() << data << info.socat_process_->readAllStandardError();
    }
    else
        qCritical() << info.socat_process_->errorString();
    return info;
}

void Main_Window::handleDeviceError(QModbusDevice::Error newError)
{
    auto modbusDevice = qobject_cast<QModbusServer*>(sender());
    if (newError == QModbusDevice::NoError || !modbusDevice)
        return;

    statusBar()->showMessage(modbusDevice->errorString(), 5000);
}

void Main_Window::onStateChanged(int state)
{
    Q_UNUSED(state)
//    auto modbusDevice = static_cast<QModbusRtuSerialSlave*>(sender());
//    qDebug() << "onStateChanged" << modbusDevice->serverAddress() <<  state;
}


void Main_Window::changeTemperature()
{
    for (GH::Device* dev: dai_project_.devices())
        for (GH::DeviceItem* item: dev->items())
        {
            if (!item->isControl() && item->isConnected())
            {
                auto val = item->needNormalize() ? (qrand() % 100) + 240 : (qrand() % 3000) + 50;
                item->setData(val, val);
            }
        }

    temp_timer_.stop();
}

QElapsedTimer t;
QByteArray buff;

void Main_Window::proccessData()
{
    if (!t.isValid())
        t.start();

    if (t.elapsed() >= 50)
        buff.clear();

    buff += serial_port_->readAll();

    auto it = modbus_list_.find( (uchar)buff.at(0) );
    if (it != modbus_list_.cend())
    {
        if (it->second.device_item_view_->is_use())
        {
            it->second.serial_port_->write(buff);
        }
//        if (it->second.box_->isUsed())
//            it->second.serial_port_->write(buff);
    }
    else
        qDebug() << "Device not found";

    t.restart();
//    qDebug() << buff.toHex().toUpper();

    return;
/*
    if (buff.size() == 32)
    {
        uint type = buff.at(0);
        uchar adr = buff.at(1);

        for (GH::DevicePtr& dev: prj.devices())
            if (dev->type() == type && dev->address() == adr)
            {
                if (!dev->item(0).isauto())
                    break;

                if (GH::DeviceItem::isControl(type))
                {
                    uchar value = dev->item(0).value().int32v();
// TODO: Fix it
#pragma GCC warning "Отправляет на включение два раза подряд"
                    if (type != GH::Prt::itWindow || value & GH::Prt::wExecuted)
                    {
                        if (buff.at(2) == 1)
                        {
                            value |= GH::Prt::wOpened;
                            value &= ~GH::Prt::wClosed;
                        }
                        else if (buff.at(2) == 2)
                        {
                            value |= GH::Prt::wClosed;
                            value &= ~GH::Prt::wOpened;
                        }

                        if (dev->item(0).value().int32v() != value)
                        {
                            if (dev->type() == GH::Prt::itWindow)
                            {
                                qDebug() << "WND" << bool((value & GH::Prt::wExecuted) != 0) << value << dev->item(0).value().int32v() << dev->id() << adr;
                                QTimer::singleShot(3000, std::bind(&GH::DeviceItem::setValue, unconstPtr<GH::DeviceItem>(dev->item(0)), value));
                                value = GH::Prt::wCalibrated;
                            }
                            qDebug() << "toggle" << dev->item(0).value().int32v() << value;
                            unconstPtr<GH::DeviceItem>(dev->item(0))->setValue(value);
                        }
                    }

                    buff[ 3 ] = value & 0xFF;
                }
                else
                {
                    for (uchar i = 0; i < dev->item_size(); i++)
                    {
                        qint16 val;

                        if (GH::DeviceItem::isConnected(dev->item(i).value()))
                            val = dev->item(i).value().int32v();
                        else
                            val = qint16(0xFFFF);

                        buff[ 3 + (i * 2) ] = val >> 8;
                        buff[ 4 + (i * 2) ] = val & 0xFF;
                    }
                }

                auto data = buff.right(buff.size() - 3);
                if (tmp[type][adr] != data)
                {
                    auto shorter = [](const QByteArray& buff) {
                        auto hex = buff;
                        int i = hex.size();
                        for (; i; --i)
                            if (hex.at(i - 1) != 0)
                                break;
                        hex.remove(i, hex.size() - i);
                        return hex.toHex().toUpper();
                    };

                    qDebug() << type << adr << shorter(tmp[type][adr]) << shorter(data);
                    tmp[type][adr] = data;
                }
                m_serialPort->write(buff);
                break;
            }
    }
    else
        qDebug("READ SIZE NOT 32 bytes");
        */
}

void Main_Window::socketDataReady()
{
    QThread::msleep(50);
    serial_port_->write(static_cast<QSerialPort*>(sender())->readAll());
}

void Main_Window::on_openBtn_toggled(bool open)
{
    return;
    if (serial_port_->isOpen())
    {
        for (auto&& item: modbus_list_)
        {
            item.second.modbus_device_->disconnectDevice();
        }
        serial_port_->close();
    }

    if (open)
    {
        serial_port_->setPortName( ui_->portName->text() );
        serial_port_->setBaudRate(   config_.baud_rate_);
        serial_port_->setDataBits(   config_.data_bits_);
        serial_port_->setParity(     config_.parity_);
        serial_port_->setStopBits(   config_.stop_bits_);
        serial_port_->setFlowControl(config_.flow_control_);

        if (!serial_port_->open(QIODevice::ReadWrite))
            qCritical() << serial_port_->errorString();

        serial_port_->setBaudRate(   config_.baud_rate_);
        serial_port_->setDataBits(   config_.data_bits_);
        serial_port_->setParity(     config_.parity_);
        serial_port_->setStopBits(   config_.stop_bits_);
        serial_port_->setFlowControl(config_.flow_control_);

        for (auto&& item: modbus_list_)
        {
            if (!item.second.modbus_device_->connectDevice())
            {
                qCritical() << item.second.modbus_device_->errorString();
            }
        }
    }
}

void Main_Window::on_socatReset_clicked()
{
    ui_->openBtn->setChecked(false);

    if (socat_process_)
    {
        socat_process_->terminate();
        socat_process_->waitForFinished();
        delete socat_process_;
    }

    auto [new_socat, pathFrom, pathTo] = create_socat();
    socat_process_ = new_socat;

    if (!pathFrom.isEmpty() && dbus_)
    {
        dbus_->setPath(pathTo);
        ui_->portName->setText(pathFrom);
        ui_->openBtn->setChecked(true);

        ui_->statusbar->showMessage("Server port address is " + pathTo);
    }
}

Main_Window::~Main_Window()
{
    QSettings s;
    s.setValue("portName", ui_->portName->text());

    term_handler(0);

    delete ui_;
}
