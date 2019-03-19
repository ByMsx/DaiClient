#ifndef DAI_CHECKER_H
#define DAI_CHECKER_H

#include <QThread>
#include <QTimer>
#include <QLoggingCategory>
#include <QMutexLocker>

#include <map>

#include <Helpz/simplethread.h>

#include "Dai/project.h"

namespace Dai {

Q_DECLARE_LOGGING_CATEGORY(CheckerLog)

class Worker;
typedef std::map<DeviceItem*, QVariant> ChangesList;

struct Write_Cache_Item {
    uint32_t user_id;
    DeviceItem* dev_item;
    QVariant raw_value;

    bool operator ==(DeviceItem* other_dev_item)
    {
        return dev_item == other_dev_item;
    }
};

class Checker : public QObject
{
    Q_OBJECT
public:
    explicit Checker(Worker* worker, int interval = 1500, const QString& pluginstr = {}, QObject *parent = 0);
    ~Checker();
    void loadPlugins(const QStringList& allowed_plugins);

    void breakChecking();
public slots:
    void stop();
    void start();
private:
private slots:
    void checkDevices();
    void write_data(DeviceItem* item, const QVariant& raw_data, uint32_t user_id = 0);
    void writeCache();
private:
    void writeItem(DeviceItem* item, const QVariant& raw_data, uint32_t user_id = 0);

    QTimer check_timer, write_timer;

    Project* prj;
//    SerialPort::Manager sp_mng;
    std::vector<Write_Cache_Item> write_cache_;

    bool b_break;

    std::shared_ptr<PluginTypeManager> PluginTypeMng;
//    friend class ModbusThread;
//    friend struct ModbusReadHelper;
};

} // namespace Dai

#endif // DAI_CHECKER_H
