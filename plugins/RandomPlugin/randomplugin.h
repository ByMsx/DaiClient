#ifndef DAI_RANDOMPLUGIN_H
#define DAI_RANDOMPLUGIN_H

#include <QLoggingCategory>

#include <memory>
#include <set>

#include "randomplugin_global.h"
#include <Dai/checkerinterface.h>

namespace Dai {
namespace Random {

Q_DECLARE_LOGGING_CATEGORY(RandomLog)

class RANDOMPLUGINSHARED_EXPORT RandomPlugin : public QObject, public CheckerInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DaiCheckerInterface_iid FILE "checkerinfo.json")
    Q_INTERFACES(Dai::CheckerInterface)
public:
    RandomPlugin();

    // CheckerInterface interface
public:
    void configure(QSettings* settings, Project*) override;
    bool check(Device *dev) override;
    void stop() override;
    void write(DeviceItem* item, const QVariant& raw_data, uint32_t user_id) override;
private:
    int random(int min, int max) const;
    std::set<quint32> writed_list_;
    QVariant value;
};

} // namespace Random
} // namespace Dai

#endif // DAI_RANDOMPLUGIN_H
