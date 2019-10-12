#include <Helpz/db_builder.h>

#include <Dai/commands.h>

#include "worker.h"

#include "log_sender.h"

namespace Dai {
namespace Client {

Log_Sender::Log_Sender(Protocol* protocol) :
    protocol_(protocol),
    db_helper_(protocol->worker()->db_pending()),
    timer_break_(false),
    timer_wakeup_(std::chrono::system_clock::now()),
    timer_thread_(&Log_Sender::timer_run, this)
{
}

Log_Sender::~Log_Sender()
{
    {
        std::lock_guard lock(mutex_);
        timer_break_ = true;
        cond_.notify_one();
    }

    if (timer_thread_.joinable())
    {
        timer_thread_.join();
    }
}

void Log_Sender::send_log_range(Log_Type_Wrapper log_type, qint64 from_time_ms, qint64 to_time_ms, uint8_t msg_id)
{
    if (log_type.is_valid())
    {
        QString table_name = log_table_name(log_type),
                where = "timestamp_msecs >= " + QString::number(from_time_ms) +
                        " AND timestamp_msecs <= " + QString::number(to_time_ms);
        protocol_->worker()->db_pending()->add([this, table_name, where, msg_id](Helpz::Database::Base* db)
        {
            protocol_->send_answer(Cmd::LOG_RANGE_COUNT, msg_id) << db->row_count(table_name, where);
        });
    }
}

void Log_Sender::send_log_data(Log_Type_Wrapper log_type, qint64 from_time_ms, qint64 to_time_ms, uint8_t msg_id)
{
    if (!log_type.is_valid())
    {
        return;
    }

    QString where = "WHERE timestamp_msecs >= " + QString::number(from_time_ms) +
                    " AND timestamp_msecs <= " + QString::number(to_time_ms);

    if (log_type == LOG_VALUE)
    {
        protocol_->worker()->db_pending()->add([this, where, msg_id](Helpz::Database::Base* db)
        {
            protocol_->send_answer(Cmd::LOG_DATA, msg_id) << Helpz::Database::db_build_list<Log_Value_Item>(*db, where);
        });
    }
    else // log_type == LOG_EVENT
    {
        protocol_->worker()->db_pending()->add([this, where, msg_id](Helpz::Database::Base* db)
        {
            protocol_->send_answer(Cmd::LOG_DATA, msg_id) << Helpz::Database::db_build_list<Log_Event_Item>(*db, where);
        });
    }
}

void Log_Sender::send(const Log_Value_Item& item)
{
    std::lock_guard lock(mutex_);
    value_pack_.push_back(item);
    start_timer(item.need_to_save() ? 10 : 250);
}

void Log_Sender::send(const Log_Event_Item& item)
{
    if (item.type_id() == QtDebugMsg && item.category().startsWith("net"))
    {
        return;
    }

    std::lock_guard lock(mutex_);
    event_pack_.push_back(item);
    start_timer(1000);
}

void Log_Sender::start_timer(int new_interval_value)
{
    std::chrono::milliseconds new_interval(new_interval_value);
    auto now = std::chrono::system_clock::now();
    if (timer_wakeup_ <= now || timer_wakeup_ - now > new_interval)
    {
        timer_wakeup_ = now + new_interval;
        cond_.notify_one();
    }
}

void Log_Sender::timer_run()
{
    std::unique_lock lock(mutex_);
    while (!timer_break_)
    {
        send_log_packs();

        if (timer_wakeup_ > std::chrono::system_clock::now())
            cond_.wait_until(lock, timer_wakeup_);
        else
            cond_.wait(lock);
    }
}

void Log_Sender::send_log_packs()
{
    if (value_pack_.size())
    {
        protocol_->send(Cmd::LOG_PACK_VALUES).timeout(nullptr, std::chrono::seconds(46), std::chrono::seconds(15)) << value_pack_;
        value_pack_.clear();
    }

    if (event_pack_.size())
    {
        protocol_->send(Cmd::LOG_PACK_EVENTS) << event_pack_;
        event_pack_.clear();
    }
}

} // namespace Client
} // namespace Dai
