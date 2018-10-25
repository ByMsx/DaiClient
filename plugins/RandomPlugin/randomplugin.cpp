#include <QDebug>
#include <QSettings>
#include <QFile>

#include <Helpz/settingshelper.h>
#include <Dai/deviceitem.h>
#include <Dai/typemanager/typemanager.h>

#include "randomplugin.h"

namespace Dai {
namespace Random {

Q_LOGGING_CATEGORY(RandomLog, "random")

RandomPlugin::RandomPlugin() : QObject() {}

void RandomPlugin::configure(QSettings *settings, Project *)
{
    using Helpz::Param;
//    auto [interval] = Helpz::SettingsHelper(
//                settings, "Random",
//                Param{"interval", 350},
//    ).unique_ptr<Conf>();

    qsrand(time(NULL));
}

bool RandomPlugin::check(Device* dev)
{
    if (!dev)
        return false;

    for (DeviceItem* item: dev->items())
    {
        if (writed_list_.find(item->id()) != writed_list_.cend())
            continue;
        switch (static_cast<ItemType::RegisterType>(item->registerType())) {
        case Dai::ItemType::rtDiscreteInputs:
        case Dai::ItemType::rtCoils:
            value = random(-32767, 32768) > 0;
            break;
        case Dai::ItemType::rtInputRegisters:
        case Dai::ItemType::rtHoldingRegisters:
            value = random(-32767, 32768);
            break;
        default:
            value.clear();
            break;
        }

        QMetaObject::invokeMethod(item, "setRawValue", Qt::QueuedConnection, Q_ARG(const QVariant&, value));
    }

    return true;
}

void RandomPlugin::stop() {}

void RandomPlugin::write(DeviceItem *item, const QVariant &raw_data)
{
    writed_list_.insert(item->id());
    QMetaObject::invokeMethod(item, "setRawValue", Qt::QueuedConnection, Q_ARG(const QVariant&, raw_data));
}

int RandomPlugin::random(int min, int max) const {
    return min + (qrand() % static_cast<int>(max - min + 1));
}

} // namespace Random
} // namespace Dai
