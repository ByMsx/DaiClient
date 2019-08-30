#ifndef DAI_LOG_SENDER_H
#define DAI_LOG_SENDER_H

#include <thread>
#include <mutex>
#include <condition_variable>

#include <QTimer>

#include <Dai/log/log_type.h>
#include <Dai/log/log_pack.h>

#include <Database/db_log_helper.h>
#include "client_protocol.h"

namespace Dai {
namespace Client {

class Log_Sender : public QObject
{
    Q_OBJECT
public:
    explicit Log_Sender(Protocol* protocol);
    ~Log_Sender();

    void send_log_range(Log_Type_Wrapper log_type, qint64 from_time_ms, qint64 to_time_ms, uint8_t msg_id);
    void send_log_data(Log_Type_Wrapper log_type, qint64 from_time_ms, qint64 to_time_ms, uint8_t msg_id);

public slots:
    void send_value_log(const Log_Value_Item &item, bool immediately = false);
    void send_event_log(const Log_Event_Item &item);
private:
    void start_timer(int new_interval_value);
    void timer_run();
    void send_log_packs();
    Protocol* protocol_;
    Database::Log_Helper db_helper_;

//    QTimer timer_;

    QVector<Log_Value_Item> value_pack_;
    QVector<Log_Event_Item> event_pack_;

    bool timer_break_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::chrono::system_clock::time_point timer_wakeup_;
    std::thread timer_thread_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_LOG_SENDER_H
