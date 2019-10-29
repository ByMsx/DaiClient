#ifndef DAI_LOG_VALUE_SAVE_TIMER_H
#define DAI_LOG_VALUE_SAVE_TIMER_H

#include <QTimer>

#include <Helpz/db_thread.h>

#include <Dai/log/log_pack.h>

namespace Dai {

class Worker;
class Project;
class ID_Timer;

class Log_Value_Save_Timer : public QObject
{
    Q_OBJECT
public:
    Log_Value_Save_Timer(Project* project, Worker* worker);
    ~Log_Value_Save_Timer();

signals:
    void change(const Log_Value_Item& item, bool immediately);
public slots:
    void add_log_value_item(const Log_Value_Item& item);
    void add_log_event_item(const Log_Event_Item& item);
private slots:
    void save_item_values();
    void send_value_pack();
    void send_event_pack();
private:
    void stop();
    void process_items(int timer_id);
    void save(Helpz::Database::Base* db, QVector<Log_Value_Item> pack);

    template<typename T>
    void send(uint16_t cmd, std::shared_ptr<QVector<T>> pack);
    template<typename T>
    bool save_to_db(const QVector<T>& pack);

    Project* prj_;
    Worker* worker_;

    std::vector<ID_Timer*> timers_list_;
    std::map<quint32, QVariant> cached_values_;

    QVector<Log_Value_Item> value_pack_;
    QVector<Log_Event_Item> event_pack_;

    std::map<quint32, std::pair<QVariant, QVariant>> waited_item_values_;
    QTimer item_values_timer_, value_pack_timer_, event_pack_timer_;
};

} // namespace Dai

#endif // DAI_LOG_VALUE_SAVE_TIMER_H
