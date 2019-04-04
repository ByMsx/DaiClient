#ifndef DAI_ONEWIRETHERMPLUGIN_H
#define DAI_ONEWIRETHERMPLUGIN_H

#include <memory>

#include <QLoggingCategory>
#include <QFile>

#include "plugin_global.h"
#include <Dai/checkerinterface.h>

namespace Dai {
namespace OneWireTherm {

Q_DECLARE_LOGGING_CATEGORY(OneWireThermLog)

class ONEWIRETHERMPLUGINSHARED_EXPORT OneWireThermPlugin : public QObject, public Checker_Interface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DaiCheckerInterface_iid FILE "checkerinfo.json")
    Q_INTERFACES(Dai::Checker_Interface)

public:
    OneWireThermPlugin();
    ~OneWireThermPlugin();

    // CheckerInterface interface
public:
    void configure(QSettings* settings, Project*) override;
    bool check(Device *dev) override;
    void stop() override;
    void write(std::vector<Write_Cache_Item>& items) override;
private:
    QFile file_;
};

} // namespace OneWireTherm
} // namespace Dai

#endif // DAI_ONEWIRETHERMPLUGIN_H
