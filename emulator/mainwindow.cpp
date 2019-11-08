#include <QSettings>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QtDBus>

#include <QHostAddress>
#include <QInputDialog>
#include <QGroupBox>
#include <QTreeView>
#include <QMenu>

#include <csignal>
#include <iostream>

#include <Helpz/settingshelper.h>
#include "device_tree_view_delegate.h"

#include <Dai/device.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QGroupBox>
#include "devices_table_model.h"
#include "device_item_table_item.h"

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
    DeviceTreeViewDelegate *delegate = new DeviceTreeViewDelegate(ui_->tree_view);
    ui_->tree_view->setItemDelegateForColumn(2, delegate);

    init();              
}

Main_Window::~Main_Window()
{
    QStringList favorites_list;
    for (Dai::Device* dev: dai_project_.devices())
    {
        for (Dai::DeviceItem* dev_item: dev->items())
        {
            if (dev_item->param("is_favorite").toBool())
            {
                favorites_list.push_back(QString::number(dev_item->id()));
            }
        }
    }

    QSettings s;
    s.setValue("portName", ui_->portName->text());
    s.setValue("favorites_only", ui_->favorites_only->isChecked());
    if (db_manager_.is_open() || !favorites_list.isEmpty())
    {
        std::cerr << "Save favorites: " << favorites_list.join(',').toStdString() << std::endl;
        s.setValue("favorites_list", favorites_list.join(','));
    }

    term_handler(0);

    delete ui_;
}

void Main_Window::init() noexcept
{
    std::signal(SIGTERM, term_handler);
    std::signal(SIGINT, term_handler);
    std::signal(SIGKILL, term_handler);

    config_.baud_rate_ = QSerialPort::Baud9600;
    qsrand(QDateTime::currentDateTime().toTime_t());

    init_database();
    db_manager_.init_project(&dai_project_);

    fill_data();

    init_client_connection();

    connect(&temp_timer_, &QTimer::timeout, this, &Main_Window::changeTemperature);
    temp_timer_.setInterval(3000);
//    temp_timer_.start();
}

void Main_Window::init_database() noexcept
{
    QSettings dai_settings("DaiClient.conf", QSettings::NativeFormat);

    auto db_info = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Z::Param<QString>,Z::Param<QString>,Z::Param<QString>,Z::Param<QString>,Z::Param<int>,Z::Param<QString>,Z::Param<QString>>
        #endif
            (
                &dai_settings, "Database",
                Helpz::Param<QString>{"Name", "deviceaccess_local"},
                Helpz::Param<QString>{"User", "DaiUser"},
                Helpz::Param<QString>{"Password", ""},
                Helpz::Param<QString>{"Host", "localhost"},
                Helpz::Param<int>{"Port", -1},
                Helpz::Param<QString>{"Driver", "QMYSQL"},
                Helpz::Param<QString>{"ConnectOptions", QString()}
    ).obj<Helpz::Database::Connection_Info>();

    db_manager_.set_connection_info(db_info);
    qDebug() << "Open SQL is" << db_manager_.create_connection();
}

void Main_Window::fill_data() noexcept
{
    QSettings dai_settings;

    bool favorites_only = dai_settings.value("favorites_only", false).toBool();
//    dai_settings.setValue("favorites_list", "164,167,228,229,175,275,176,184,198,201,230,231,209,276,210,218");
    QStringList favorites_list = dai_settings.value("favorites_list").toString().split(',');
    qDebug() << "Favorites:" << favorites_list;

    int dev_address;
    QVariant address_var;

    devices_table_model_ = new DevicesTableModel(&dai_project_.item_type_mng_);
    ui_->favorites_only->setChecked(favorites_only && !favorites_list.isEmpty());
    devices_table_model_->set_use_favorites_only(ui_->favorites_only->isChecked());
    ui_->tree_view->setModel(devices_table_model_);

    for (GH::Device* dev: dai_project_.devices())
    {
        if (dev->items().size() > 0)
        {
            address_var = dev->param("address");
            if (!address_var.isValid() || address_var.isNull())
            {
                continue;
            }

            for (Dai::DeviceItem* dev_item: dev->items())
            {
                const uint8_t reg_type = dai_project_.item_type_mng_.register_type(dev_item->type_id());
                if (reg_type == Dai::Item_Type::rtHoldingRegisters ||
                    reg_type == Dai::Item_Type::rtInputRegisters)
                {
                    auto val = dev_item->need_normalize() ? (qrand() % 100) + 240 : (qrand() % 3000) + 50;
                    dev_item->set_data(val, val);
                }

                for (auto it = favorites_list.begin(); it != favorites_list.end(); ++it)
                {
                    if (dev_item->id() == it->toUInt())
                    {
                        dev_item->set_param("is_favorite", true);
                        favorites_list.erase(it);
                        break;
                    }
                }
            }

            dev_address = address_var.toInt();
            auto map_it = modbus_list_.find(dev_address);

            if (map_it == modbus_list_.cend())
            {
                Modbus_Device_Item modbus_device_item;
                modbus_device_item = create_socat();
                modbus_device_item.modbus_device_ = new QModbusRtuSerialSlave(this);
                modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialPortNameParameter, modbus_device_item.port_from_name_);
                modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialParityParameter, config_.parity_);
                modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, config_.baud_rate_);
                modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, config_.data_bits_);
                modbus_device_item.modbus_device_->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, config_.stop_bits_);
                modbus_device_item.modbus_device_->setServerAddress(dev_address);
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

                modbus_device_item.device_table_item_ = new DeviceTableItem(&dai_project_.item_type_mng_, modbus_device_item.modbus_device_, dev);
                devices_table_model_->appendChild(modbus_device_item.device_table_item_);

                modbus_list_.emplace(dev_address, modbus_device_item);
            }
            else
            {
                DeviceTableItem* device_table_item = map_it->second.device_table_item_;
                if (device_table_item != nullptr)
                {
                    device_table_item->assign(dev);
                }
            }
        }
    }

    expand_tree_view();
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

void Main_Window::expand_tree_view()
{
    ui_->tree_view->expandAll();
    for (int column = 0; column < devices_table_model_->columnCount(); ++column) {
        ui_->tree_view->resizeColumnToContents(column);
    }
}

Main_Window::Socat_Info Main_Window::create_socat()
{
    Socat_Info info;

    info.socat_process_ = new QProcess;
    info.socat_process_->setProcessChannelMode(QProcess::MergedChannels);
    info.socat_process_->setReadChannel(QProcess::StandardOutput);
    info.socat_process_->setProgram("/usr/bin/socat");
    // TODO need take the username from GUI
    info.socat_process_->setArguments(QStringList() << "-d" << "-d" << ("pty,raw,echo=0,user=" + get_user()) << ("pty,raw,echo=0,user=" + get_user()));

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

QString Main_Window::get_user() noexcept
{
    QString user = qgetenv("USER");
    if (user.isEmpty())
    {
        user = qgetenv("USERNAME");
    }
    return user;
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
            if (!item->is_control() && item->is_connected())
            {
                auto val = item->need_normalize() ? (qrand() % 100) + 240 : (qrand() % 3000) + 50;
                item->set_data(val, val);
            }
        }

    temp_timer_.stop();
}

QElapsedTimer t;
QByteArray buff;

void Main_Window::proccessData()
{
    ui_->light_indicator_->toggle();

    if (!t.isValid())
        t.start();

    if (t.elapsed() >= 50)
        buff.clear();

    buff += serial_port_->readAll();

    auto it = modbus_list_.find( (uchar)buff.at(0) );
    if (it != modbus_list_.cend())
    {
        if (it->second.device_table_item_->isUse())
        {
            it->second.serial_port_->write(buff);
        }
    }
    else
        qDebug() << "Device not found";

    t.restart();
}

void Main_Window::socketDataReady()
{
    QThread::msleep(50);
    serial_port_->write(static_cast<QSerialPort*>(sender())->readAll());
}

void Main_Window::on_openBtn_toggled(bool open)
{
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

        ui_->statusbar->showMessage("Server port address is " + pathTo + " speed " + QString::number(config_.baud_rate_));
    }
}

void Main_Window::on_favorites_only_toggled(bool checked)
{
    devices_table_model_->set_use_favorites_only(checked);
    expand_tree_view();
}

void Main_Window::on_tree_view_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = ui_->tree_view->indexAt(pos);
    if (index.isValid() && index.parent().isValid())
    {
        auto deviceTableItem = dynamic_cast<DeviceItemTableItem*>(static_cast<DevicesTableItem*>(index.internalPointer()));
        if (deviceTableItem)
        {
            bool is_favorite = deviceTableItem->is_favorite();
            QMenu menu;
            QAction* act;
            if (is_favorite)
            {
                act = menu.addAction(QIcon(":/img/del.png"), "Убрать из избранного");
            }
            else
            {
                act = menu.addAction(QIcon(":/img/star.png"), "Добавить в избранное");
            }
            QAction* res_act = menu.exec(ui_->tree_view->viewport()->mapToGlobal(pos));
            if (res_act == act && devices_table_model_->set_is_favorite(index, !is_favorite))
            {
                expand_tree_view();
            }
        }
    }
}
