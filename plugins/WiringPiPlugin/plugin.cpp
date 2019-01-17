#include <QDebug>
#include <QSettings>
#include <QFile>

#include <wiringPi.h>

#include <Helpz/settingshelper.h>
#include <Dai/deviceitem.h>

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

void WiringPiPlugin::configure(QSettings *settings, Project *)
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
}

bool WiringPiPlugin::check(Device* dev)
{
    const QVector<DeviceItem *> &items = dev->items();
    bool state;
    for (DeviceItem * item: items) {
        state = digitalRead(item->unit().toUInt()) ? true : false;
        if (!item->isConnected() || item->getRawValue().toBool() != state)
            QMetaObject::invokeMethod(item, "setRawValue", Qt::QueuedConnection, Q_ARG(const QVariant&, state));
    }

    return true;
}

void WiringPiPlugin::stop() {}

void WiringPiPlugin::write(DeviceItem *item, const QVariant &raw_data) {
    digitalWrite(item->unit().toUInt(), raw_data.toBool() ? HIGH : LOW);
}

} // namespace Modbus
} // namespace Dai
