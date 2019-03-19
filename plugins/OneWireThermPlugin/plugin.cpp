#include <QDebug>
#include <QSettings>
#include <QFile>

#include <Helpz/settingshelper.h>
#include <Dai/deviceitem.h>

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

}

bool OneWireThermPlugin::check(Device* dev)
{
    const QVector<DeviceItem *> &items = dev->items();
    QVariant value;
    QString unit;
    QList<QByteArray> data_lines;
    int idx;
    double t;
    bool ok;

    for (DeviceItem * item: items) {
        unit = item->unit().toString();
        if (unit.isEmpty())
            continue;
        value.clear();
        file_.setFileName(QString("/sys/bus/w1/devices/28-%1/w1_slave").arg(unit));
        if (!file_.open(QIODevice::ReadOnly)) {
            if (item->isConnected())
                qCWarning(OneWireThermLog) << "Read failed" << file_.fileName() << file_.errorString();
        } else {
            data_lines = file_.readAll().split('\n');
            if (data_lines.size() >= 2 && data_lines.at(0).right(3).toUpper() == "YES") {
                idx = data_lines.at(1).indexOf("t=");
                if (idx != -1) {
                    t = data_lines.at(1).mid(idx + 2).toInt(&ok) / 1000.;
                    if (ok)
                        value = t;
                }
            }
            file_.close();
        }

        if (item->getRawValue() != value) {
            QMetaObject::invokeMethod(item, "setRawValue", Qt::QueuedConnection, Q_ARG(const QVariant&, value));
        }
    }

    return true;
}

void OneWireThermPlugin::stop() {}

void OneWireThermPlugin::write(DeviceItem *item, const QVariant &raw_data, uint32_t user_id) {
//    digitalWrite(item->unit(), raw_data.toBool() ? HIGH : LOW);
}

} // namespace Modbus
} // namespace Dai
