#include "one_wire_therm_task.h"

#include <QFile>
#include <QDir>
#include <Dai/deviceitem.h>
#include <Dai/device.h>
#include <QDebug>

namespace Dai {
namespace OneWireTherm {

Q_LOGGING_CATEGORY(OneWireThermLog, "OneWireTherm")

One_Wire_Therm_Task::One_Wire_Therm_Task(QObject *parent) : QObject(parent)
{
    obtain_device_list();
}

void One_Wire_Therm_Task::obtain_device_list() noexcept
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

void One_Wire_Therm_Task::read_therm_data(Device *dev)
{
    const QVector<DeviceItem *> &items = dev->items();
    int unit;
    QList<QByteArray> data_lines;
    int idx;
    double t;
    bool ok;
    std::map<DeviceItem*, QVariant> new_values;

    for (DeviceItem * item: items)
    {
        unit = item->param("unit").toInt();
        if (unit >= 1)
        {
            if (!check_and_open_file(unit))
            {
                if (item->is_connected())
                {
                    qCWarning(OneWireThermLog) << "Read failed unit: " << unit << file_.fileName() << file_.errorString();
                }
                new_values.emplace(item, QVariant());
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
                        {
                            new_values.emplace(item, QVariant(t));
                        }
                    }
                }
                else
                {
                    new_values.emplace(item, QVariant());
                }
                file_.close();
            }
        }
    }

    if (!new_values.empty())
    {
        QMetaObject::invokeMethod(dev, "set_device_items_values",
                                  QArgument<std::map<DeviceItem*, QVariant>>("std::map<DeviceItem*, QVariant>", new_values));
    }
}

bool One_Wire_Therm_Task::check_and_open_file(int unit) noexcept
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

bool One_Wire_Therm_Task::get_device_file_path(int unit, QString &path_out) noexcept
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

bool One_Wire_Therm_Task::try_open_file(const QString &path) noexcept
{
    file_.setFileName(path);
    if (!file_.open(QIODevice::ReadOnly))
    {
        return false;
    }
    return true;
}

} // namespace OneWireTherm
} // namespace Dai
