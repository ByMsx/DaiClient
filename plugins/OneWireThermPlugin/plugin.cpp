#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QDir>

#include <Helpz/settingshelper.h>
#include <Dai/deviceitem.h>
#include <Dai/device.h>

#include "plugin.h"

namespace Dai {
namespace OneWireTherm {

Q_LOGGING_CATEGORY(OneWireThermLog, "OneWireTherm")

// ----------

OneWireThermPlugin::OneWireThermPlugin() :
    QObject()
{
    qCDebug(OneWireThermLog) << "init" << this;
}

OneWireThermPlugin::~OneWireThermPlugin()
{
    qCDebug(OneWireThermLog) << "free" << this;
}

void OneWireThermPlugin::configure(QSettings *settings, Project *)
{
    /*
    using Helpz::Param;
    conf = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Param<QString>,Param<QSerialPort::BaudRate>,Param<QSerialPort::DataBits>,
                            Param<QSerialPort::Parity>,Param<QSerialPort::StopBits>,Param<QSerialPort::FlowControl>,Param<int>,Param<int>,Param<int>>
        #endif
            (
                settings, "OneWireTherm",
                Param<QString>{"Port", "ttyUSB0"},
    ).unique_ptr<Conf>();
    */
    obtain_device_list();
}

void OneWireThermPlugin::obtain_device_list() noexcept
{
    devices_map_.clear();
    QString devices_path = "/sys/bus/w1/devices/";
    QDir devices_dir(devices_path);
    QStringList devices = devices_dir.entryList(QStringList() << "28-*", QDir::Dirs);
    for (int i = 0; i < devices.length(); ++i)
    {
        devices_map_.emplace(i + 1, devices_path + devices.at(i) + "/w1_slave");
    }
}

bool OneWireThermPlugin::check(Device* dev)
{
    const QVector<DeviceItem *> &items = dev->items();
    QVariant value;
    int unit;
    QList<QByteArray> data_lines;
    int idx;
    double t;
    bool ok;

    for (DeviceItem * item: items)
    {
        unit = item->param("unit").toInt();
        if (unit < 1)
        {
            continue;
        }
        value.clear();

        if (!check_and_open_file(unit))
        {
            if (item->isConnected())
                qCWarning(OneWireThermLog) << "Read failed" << "unit: " << unit << file_.fileName() << file_.errorString();
        }
        else
        {
            data_lines = file_.readAll().split('\n');
            if (data_lines.size() >= 2 && data_lines.at(0).right(3).toUpper() == "YES")
            {
                idx = data_lines.at(1).indexOf("t=");
                if (idx != -1)
                {
                    t = data_lines.at(1).mid(idx + 2).toInt(&ok) / 1000.;
                    if (ok)
                        value = t;
                }
            }
            file_.close();
        }

        if (item->raw_value() != value)
        {
            QMetaObject::invokeMethod(item, "setRawValue", Qt::QueuedConnection, Q_ARG(const QVariant&, value));
        }
    }

    return true;
}

bool OneWireThermPlugin::check_and_open_file(int unit) noexcept
{
    QString path;
    if (get_device_file_path(unit, path))
    {
        if (!try_open_file(path))
        {
            obtain_device_list();
            if (!get_device_file_path(unit, path) || !try_open_file(path))
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool OneWireThermPlugin::get_device_file_path(int unit, QString &path_out) noexcept
{
    auto path_it = devices_map_.find(unit);
    if (path_it == devices_map_.cend())
    {
        obtain_device_list();
        path_it = devices_map_.find(unit);
        if (path_it == devices_map_.cend())
        {
            return false;
        }
    }
    path_out = path_it->second;
    return true;
}

bool OneWireThermPlugin::try_open_file(const QString &path) noexcept
{
    file_.setFileName(path);
    if (!file_.open(QIODevice::ReadOnly))
    {
        return false;
    }
    return true;
}

void OneWireThermPlugin::stop() {}
void OneWireThermPlugin::write(std::vector<Write_Cache_Item>& /*items*/) {}

} // namespace Modbus
} // namespace Dai
