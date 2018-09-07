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

namespace GH = ::Dai;

void DBusTTY::setPath(const QString &path) { m_path = path; }
QString DBusTTY::ttyPath() const { return m_path; }

MainWindow* obj = nullptr;

void term_handler(int)
{
    if (obj)
    {
        obj->m_socat->terminate();
        obj->m_socat->waitForFinished();
        delete obj->m_socat;

        for (auto&& item: obj->modbus_list)
        {
            item.second.serialPort->close();
            item.second.socat->terminate();
            item.second.socat->waitForFinished();
            delete item.second.socat;
        }
    }
    std::cerr << "Termination.\n";
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    db_mng(), sct_mng(&db_mng),
    m_socat(nullptr)
{
    obj = this;

//    db_mng.setTypeManager(&sct_mng);

    conf.baudRate = QSerialPort::Baud9600;

    ui->setupUi(this);

    qsrand(QDateTime::currentDateTime().toTime_t());

    QSettings dai_s("DaiClient.conf", QSettings::NativeFormat);

    init_Database(&dai_s);
    init();


//    QLoggingCategory::setFilterRules(QStringLiteral("qt.modbus* = true"));
//    QLoggingCategory::setFilterRules(QStringLiteral("qt.modbus*=true;qt.modbus.lowlevel*=true;"));
    m_serialPort = new QSerialPort(this);
    connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::proccessData);
    connect(&timer, &QTimer::timeout, this, &MainWindow::changeTemperature);
    timer.setInterval(3000);
    timer.start();

    QSettings s;
    ui->portName->setText(s.value("portName", "/dev/pts/").toString());

    // -----------------------------------
    std::signal(SIGTERM, term_handler);
    std::signal(SIGINT, term_handler);
    std::signal(SIGKILL, term_handler);

    if (QDBusConnection::sessionBus().isConnected())
    {
        if (QDBusConnection::sessionBus().registerService("ru.deviceaccess.Dai.Emulator"))
        {
            dbus = new DBusTTY;
            QDBusConnection::sessionBus().registerObject("/", dbus, QDBusConnection::ExportAllInvokables);
        }
        else
            qCritical() << "Уже зарегистрирован";
    }

    on_socatReset_clicked();
}

void MainWindow::handleDeviceError(QModbusDevice::Error newError)
{
    auto modbusDevice = qobject_cast<QModbusServer*>(sender());
    if (newError == QModbusDevice::NoError || !modbusDevice)
        return;

    statusBar()->showMessage(modbusDevice->errorString(), 5000);
}

void MainWindow::onStateChanged(int state)
{
    Q_UNUSED(state)
//    auto modbusDevice = static_cast<QModbusRtuSerialSlave*>(sender());
//    qDebug() << "onStateChanged" << modbusDevice->serverAddress() <<  state;
}

MainWindow::~MainWindow()
{
    QSettings s;
    s.setValue("portName", ui->portName->text());

    term_handler(0);

    delete ui;
}

void MainWindow::init_Database(QSettings* s)
{
    s->beginGroup("Database");
    qDebug() << "Open SQL is" << db_mng.createConnection({
                    s->value("Name", "dai_main").toString(),
                    s->value("User", "DaiUser").toString(),
                    s->value("Password", "?").toString() });
    s->endGroup();
}

void MainWindow::init()
{
    auto box = static_cast<QGridLayout*>(ui->content->layout());

    while (auto it = box->takeAt(0)) {
        delete it->widget();
        delete it;
    }

    sct_mng.initFromDatabase(&db_mng);
//    db_mng.fillTypes();
//    devs = db_mng.fillDevices();

    std::map<quint32, bool> hasZeroUnit;
    std::map<quint32, bool>::iterator hasZeroUnitIt;
    for (GH::Device* dev: sct_mng.devices())
    {
        hasZeroUnit.clear();
//        hasZeroUnit = false;
        for (GH::DeviceItem* item: dev->items())
        {
            hasZeroUnitIt = hasZeroUnit.find(item->type());
            if (hasZeroUnitIt == hasZeroUnit.end())
                hasZeroUnitIt = hasZeroUnit.emplace(item->type(), false).first;

            if (item->unit() == 0 && !hasZeroUnitIt->second)
                hasZeroUnitIt->second = true;
        }

        for (auto it = hasZeroUnit.cbegin(); it != hasZeroUnit.cend(); ++it)
            if (!it->second)
                dev->createItem(0, 0, 0, "0");
    }

//    int column = -1;
    for (GH::Device* dev: sct_mng.devices())
    {
        if (dev->items().size() == 0 ||
                dev->items().first()->type() == GH::Prt::itProcessor)
            continue;

        ModbusDeviceItem item;
        item = createSocat();
        item.device = new QModbusRtuSerialSlave(this);
        item.device->setConnectionParameter(QModbusDevice::SerialPortNameParameter, item.portFromName );
        item.device->setConnectionParameter(QModbusDevice::SerialParityParameter,   conf.parity);
        item.device->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, conf.baudRate);
        item.device->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, conf.dataBits);
        item.device->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, conf.stopBits);
        item.device->setServerAddress(dev->address());

//        connect(modbusDevice, &QModbusServer::stateChanged,
//                this, &MainWindow::onStateChanged);
        connect(item.device, &QModbusServer::errorOccurred,
                this, &MainWindow::handleDeviceError);

        item.serialPort = new QSerialPort(this);
        item.serialPort->setPortName(item.portToName);
        item.serialPort->setBaudRate(   conf.baudRate);
        item.serialPort->setDataBits(   conf.dataBits);
        item.serialPort->setParity(     conf.parity);
        item.serialPort->setStopBits(   conf.stopBits);
        item.serialPort->setFlowControl(conf.flowControl);

        if (!item.serialPort->open(QIODevice::ReadWrite))
            qCritical() << item.serialPort->errorString();

        connect(item.serialPort, &QSerialPort::readyRead, this, &MainWindow::socketDataReady);

        item.box = new MainBox(&sct_mng.ItemTypeMng, dev, item.device, ui->content);
        modbus_list.emplace(dev->address(), item);

        box->addWidget( item.box, box->rowCount(), 0, 1, 2 );
        /*continue;

        QWidget* w = nullptr;
        switch (dev->type()) {
        case GH::Prt::itTemperature:
            w = new TemperatureBox(dev.get(), ui->content);
            break;
        case GH::Prt::itMistiner:
        case GH::Prt::itWindow:
            w = new WindowBox(dev.get(), ui->content);
            break;
        case GH::Prt::itProcessor:
            continue;
        default:
            if (dev->item_size() && GH::DeviceItem::isControl( dev->item(0).type() ))
                w = new OnOffBox(dev.get(), ui->content);
            else
                w = new LightBox(dev.get(), ui->content);
            break;
        }

        if (dev->type() == GH::Prt::itTemperature ||
                dev->type() == GH::Prt::itWindow ||
                dev->type() == GH::Prt::itMistiner)
            box->addWidget( w, box->rowCount(), 0, 1, 2 );
        else
        {
            column = (column + 1) % 2;
            box->addWidget( w, box->rowCount() - column, column);
        }*/
    }
    box->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding), box->rowCount(), 0);
}

MainWindow::SocatInfo MainWindow::createSocat()
{
    SocatInfo info;

    info.socat = new QProcess;
    info.socat->setProcessChannelMode(QProcess::MergedChannels);
    info.socat->setReadChannel(QProcess::StandardOutput);
    info.socat->setProgram("/usr/bin/socat");
    info.socat->setArguments(QStringList() << "-d" << "-d" << "pty,raw,echo=0,user=kirill" << "pty,raw,echo=0,user=kirill");

    auto dbg = qDebug() << info.socat->program() << info.socat->arguments();
    info.socat->start(QIODevice::ReadOnly);
    if (info.socat->waitForStarted() && info.socat->waitForReadyRead() && info.socat->waitForReadyRead())
    {
        auto data = info.socat->readAllStandardOutput();
        QRegularExpression re("/dev/pts/\\d+");
        auto it = re.globalMatch(data);

        QStringList args;
        while (it.hasNext())
            args << it.next().captured(0);

        if (args.count() == 2)
        {
            info.portFromName = args.at(0);
            info.portToName = args.at(1);
            dbg << info.portFromName << info.portToName;
        }
        else
            qCritical() << data << info.socat->readAllStandardError();
    }
    else
        qCritical() << info.socat->errorString();
    return info;
}

void MainWindow::changeTemperature()
{
    for (GH::Device* dev: sct_mng.devices())
        for (GH::DeviceItem* item: dev->items())
        {
            if (!item->isControl() && item->isConnected())
            {
                auto val = item->needNormalize() ? (qrand() % 100) + 240 : (qrand() % 3000) + 50;
                item->setData(val, val);
            }
        }

    timer.stop();
}

QElapsedTimer t;
QByteArray buff;

void MainWindow::proccessData()
{
    if (!t.isValid())
        t.start();

    if (t.elapsed() >= 50)
        buff.clear();

    buff += m_serialPort->readAll();

    auto it = modbus_list.find( (uchar)buff.at(0) );
    if (it != modbus_list.cend())
    {
        if (it->second.box->isUsed())
            it->second.serialPort->write(buff);
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

        for (GH::DevicePtr& dev: sct_mng.devices())
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

void MainWindow::socketDataReady()
{
    QThread::msleep(50);
    m_serialPort->write(static_cast<QSerialPort*>(sender())->readAll());
}

void MainWindow::on_openBtn_toggled(bool open)
{
    if (m_serialPort->isOpen())
    {
        for (auto&& item: modbus_list)
            item.second.device->disconnectDevice();
        m_serialPort->close();
    }

    if (open)
    {
        m_serialPort->setPortName( ui->portName->text() );
        m_serialPort->setBaudRate(   conf.baudRate);
        m_serialPort->setDataBits(   conf.dataBits);
        m_serialPort->setParity(     conf.parity);
        m_serialPort->setStopBits(   conf.stopBits);
        m_serialPort->setFlowControl(conf.flowControl);

        if (!m_serialPort->open(QIODevice::ReadWrite))
            qCritical() << m_serialPort->errorString();

        m_serialPort->setBaudRate(   conf.baudRate);
        m_serialPort->setDataBits(   conf.dataBits);
        m_serialPort->setParity(     conf.parity);
        m_serialPort->setStopBits(   conf.stopBits);
        m_serialPort->setFlowControl(conf.flowControl);

        for (auto&& item: modbus_list)
        {
            if (!item.second.device->connectDevice())
                qCritical() << item.second.device->errorString();
        }
    }
}

void MainWindow::on_socatReset_clicked()
{
    ui->openBtn->setChecked(false);

    if (m_socat)
    {
        m_socat->terminate();
        m_socat->waitForFinished();
        delete m_socat;
    }

    auto [new_socat, pathFrom, pathTo] = createSocat();
    m_socat = new_socat;

    if (!pathFrom.isEmpty() && dbus)
    {
        dbus->setPath(pathTo);
        ui->portName->setText(pathFrom);
        ui->openBtn->setChecked(true);

        ui->statusbar->showMessage("Server port address is " + pathTo);
    }
}
