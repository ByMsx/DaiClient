#include <QDebug>
#include <QSettings>
#include <QFile>

#include <wiringPi.h>

#include <Helpz/settingshelper.h>
#include <Dai/deviceitem.h>
#include <Dai/device.h>
#include <Dai/project.h>
#include "plugin.h"

namespace Dai {
namespace WiringPi {

Q_LOGGING_CATEGORY(WiringPiLog, "WiringPi")

// ----------

WiringPiPlugin::WiringPiPlugin() :
    QObject(),
    b_break(false)
{
    qCDebug(WiringPiLog) << "init" << this;
}

WiringPiPlugin::~WiringPiPlugin()
{
    qCDebug(WiringPiLog) << "free" << this;
}

void WiringPiPlugin::configure(QSettings *settings, Project *project)
{
    /*
    using Helpz::Param;
    conf = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Param<QString>,Param<QSerialPort::BaudRate>,Param<QSerialPort::DataBits>,
                            Param<QSerialPort::Parity>,Param<QSerialPort::StopBits>,Param<QSerialPort::FlowControl>,Param<int>,Param<int>,Param<int>>
        #endif
            (
                settings, "WiringPi",
                Param<QString>{"Port", "ttyUSB0"},
    ).unique_ptr<Conf>();
    */

    wiringPiSetup();
    QVariant mode;
    QVariant pin;
    for (auto * device : project->devices())
    {
        const QVector<DeviceItem *> &items = device->items();
        for (DeviceItem * item: items)
        {
            mode = item->param("mode");
            pin = item->param("pin");
            if (mode.isValid() && pin.isValid())
            {
                if (mode.toString().trimmed().toLower() == "in")
                {
                    pinMode(pin.toUInt(), INPUT);
                }
                else if (mode.toString().trimmed().toLower() == "out")
                {
                    pinMode(pin.toUInt(), OUTPUT);
                }
            }
        }
    }
}

bool WiringPiPlugin::check(Device* dev)
{
    const QVector<DeviceItem *> &items = dev->items();
    bool state;
    QVariant pin;
    QVariant mode;
    for (DeviceItem * item: items)
    {
        mode = item->param("mode");
        if (mode.isValid() && mode.toString().trimmed().toLower() == "in")
        {
            pin = item->param("pin");
            if (pin.isValid())
            {
                state = digitalRead(pin.toUInt()) ? true : false;
                if (!item->isConnected() || item->raw_value().toBool() != state)
                {
                    QMetaObject::invokeMethod(item, "set_raw_value", Qt::QueuedConnection, Q_ARG(const QVariant&, state));
                }
            }
        }

    }

    return true;
}

void WiringPiPlugin::stop() {}

void WiringPiPlugin::write(std::vector<Write_Cache_Item>& items)
{
    QVariant pin;
    QVariant mode;
    for (const Write_Cache_Item& item: items)
    {
        mode = item.dev_item_->param("mode");
        if (mode.isValid() && mode.toString().trimmed().toLower() == "out")
        {
            pin = item.dev_item_->param("pin");
            if (pin.isValid())
            {
                digitalWrite(pin.toUInt(), item.raw_data_.toBool() ? HIGH : LOW);
            }
        }
    }
}

} // namespace Modbus
} // namespace Dai
