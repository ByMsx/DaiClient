#ifndef DAI_LOG_VALUE_SAVE_TIMER_H
#define DAI_LOG_VALUE_SAVE_TIMER_H

#include <QTimer>

#include <Helpz/db_thread.h>

#include <Dai/log/log_pack.h>

namespace Dai {

class Project;
class ID_Timer;

class Log_Value_Save_Timer : public QObject
{
    Q_OBJECT
public:
    Log_Value_Save_Timer(Project* project, Helpz::Database::Thread* db_thread);
    ~Log_Value_Save_Timer();


signals:
    void change(const Log_Value_Item& item, bool immediately);
private:
    void stop();
    void process_items(int timer_id);
    void save(Helpz::Database::Base* db, QVector<Log_Value_Item> pack);

    Project* prj_;
    Helpz::Database::Thread* db_thread_;

    std::vector<ID_Timer*> timers_list_;
    std::map<quint32, QVariant> cached_values_;
};

} // namespace Dai

#endif // DAI_LOG_VALUE_SAVE_TIMER_H
