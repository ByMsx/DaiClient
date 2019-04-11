#ifndef DAI_LOG_VALUE_SAVE_TIMER_H
#define DAI_LOG_VALUE_SAVE_TIMER_H

#include <QTimer>

#include <Helpz/db_thread.h>

#include <Dai/log/log_pack.h>

namespace Dai {

class Project;

class Log_Value_Save_Timer : public QObject
{
    Q_OBJECT
public:
    Log_Value_Save_Timer();
    Log_Value_Save_Timer(Project* project, Helpz::Database::Thread* db_thread);

    void start(int period, Project* project, Helpz::Database::Thread* db_thread);
    void stop();

signals:
    void change(const Log_Value_Item& item, bool immediately);
private:
    void process_items();
    void save(Helpz::Database::Base* db, QVector<Log_Value_Item> pack);

    Project* prj_;
    Helpz::Database::Thread* db_thread_;

    int period_;
    QTimer timer_;
    std::vector<QTimer*> timers_list_;
    std::map<quint32, QVariant> cached_values_;
};

} // namespace Dai

#endif // DAI_LOG_VALUE_SAVE_TIMER_H
