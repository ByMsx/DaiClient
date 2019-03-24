#include <QDebug>
#include <QSettings>
#include <QFile>

#include <Helpz/settingshelper.h>
#include <Dai/deviceitem.h>
#include <Dai/device.h>
#include <Dai/db/item_type.h>

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
        switch (static_cast<Item_Type::RegisterType>(item->register_type())) {
        case Item_Type::rtDiscreteInputs:
        case Item_Type::rtCoils:
            value = random(-32767, 32768) > 0;
            break;
        case Item_Type::rtInputRegisters:
        case Item_Type::rtHoldingRegisters:
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

void RandomPlugin::write(DeviceItem *item, const QVariant &raw_data, uint32_t user_id)
{
    writed_list_.insert(item->id());
}

int RandomPlugin::random(int min, int max) const {
    return min + (qrand() % static_cast<int>(max - min + 1));
}

} // namespace Random
} // namespace Dai
