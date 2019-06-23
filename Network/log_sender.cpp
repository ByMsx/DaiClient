
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

void Log_Sender::send_log_range(Log_Type_Wrapper log_type, qint64 date_ms, uint8_t msg_id)
{
    if (log_type.is_valid())
    {
        db_helper_.log_range(log_type, date_ms, [=](const QPair<quint32, quint32>& range)
        {
            protocol_->send_answer(Cmd::LOG_RANGE, msg_id) << log_type << range;
        });
    }
}

void Log_Sender::send_log_data(Log_Type_Wrapper log_type, QPair<quint32, quint32> range, uint8_t msg_id)
{
    if (!log_type.is_valid())
    {
        return;
    }

    if (log_type == LOG_VALUE)
    {
        db_helper_.log_value_data(range, [this, msg_id](const QVector<quint32>& not_found, const QVector<Log_Value_Item>& data_value)
        {
            protocol_->send_answer(Cmd::LOG_DATA, msg_id) << uint8_t(LOG_VALUE) << not_found << data_value;
        });
    }
    else // log_type == LOG_EVENT
    {
        db_helper_.log_event_data(range, [this, msg_id](const QVector<quint32>& not_found, const QVector<Log_Event_Item>& data_event)
        {
            protocol_->send_answer(Cmd::LOG_DATA, msg_id) << uint8_t(LOG_EVENT) << not_found << data_event;
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
    if (item.type() == QtDebugMsg && item.who().startsWith("net"))
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
