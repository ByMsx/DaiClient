#ifndef DAI_CHECKER_H
#define DAI_CHECKER_H

#include <QThread>
#include <QTimer>
#include <QLoggingCategory>
#include <QMutexLocker>

#include <map>

#include <Helpz/simplethread.h>

//#include <Dai/project.h>
#include <Dai/write_cache_item.h>

namespace Dai {

Q_DECLARE_LOGGING_CATEGORY(CheckerLog)

namespace Database {
class Plugin_Type;
class Plugin_Type_Manager;
} // namespace Database
class Scripted_Project;
class Worker;
typedef std::map<DeviceItem*, QVariant> ChangesList;

class Checker : public QObject
{
    Q_OBJECT
public:
    explicit Checker(Worker* worker, const QStringList& plugins = {}, QObject *parent = 0);
    ~Checker();
    void loadPlugins(const QStringList& allowed_plugins, Worker* worker);

    void break_checking();
public slots:
    void stop();
    void start();
private:
private slots:
    void check_devices();
    void write_data(DeviceItem* item, const QVariant& raw_data, uint32_t user_id = 0);
    void write_cache();
private:
    void write_items(Database::Plugin_Type* plugin, std::vector<Write_Cache_Item>& items);

    QTimer check_timer_, write_timer_;

    Scripted_Project* prj_;
    std::map<Database::Plugin_Type*, std::vector<Write_Cache_Item>> write_cache_;

    bool b_break, first_check_;

    std::shared_ptr<Database::Plugin_Type_Manager> plugin_type_mng_;

    struct Check_Info
    {
        bool status_;
        qint64 time_;
    };
    QMap<uint32_t, Check_Info> last_check_time_map_;
};

} // namespace Dai

#endif // DAI_CHECKER_H
