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

class RANDOMPLUGINSHARED_EXPORT RandomPlugin : public QObject, public Checker_Interface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DaiCheckerInterface_iid FILE "checkerinfo.json")
    Q_INTERFACES(Dai::Checker_Interface)
public:
    RandomPlugin();

    // CheckerInterface interface
public:
    void configure(QSettings* settings, Project*) override;
    bool check(Device *dev) override;
    void stop() override;
    void write(std::vector<Write_Cache_Item>& items) override;
private:
    int random(int min, int max) const;
    std::set<quint32> writed_list_;
    QVariant value;
};

} // namespace Random
} // namespace Dai

#endif // DAI_RANDOMPLUGIN_H
