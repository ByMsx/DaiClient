#include <Helpz/db_builder.h>

#include <Dai/commands.h>

#include "worker.h"

#include "log_sender.h"

namespace Dai {
namespace Client {

Log_Sender::Log_Sender(Protocol* protocol) :
    QObject(),
    protocol_(protocol),
    db_helper_(protocol->worker()->db_pending())
{
    moveToThread(protocol->thread());
    timer_.moveToThread(protocol->thread());

    connect(protocol->thread(), &QThread::finished, &timer_, &QTimer::stop, Qt::DirectConnection);
    connect(&timer_, &QTimer::timeout, this, &Log_Sender::send_log_packs);
    timer_.setSingleShot(true);

    auto w = protocol->worker();
    connect(w, &Worker::change, this, &Log_Sender::send_value_log, Qt::QueuedConnection);
    connect(w, &Worker::event_message, this, &Log_Sender::send_event_log, Qt::QueuedConnection);
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

void Log_Sender::send_value_log(const Log_Value_Item& item, bool immediately)
{
    value_pack_.push_back(item);
    timer_.start(immediately ? 10 : 250);
}

void Log_Sender::send_event_log(const Log_Event_Item& item)
{
    if (item.type_id() == QtDebugMsg && item.category().startsWith("net"))
    {
        return;
    }
    event_pack_.push_back(item);
    timer_.start(1000);
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
