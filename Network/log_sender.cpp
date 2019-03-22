
#include <Dai/commands.h>

#include "worker.h"
#include "Database/db_manager.h"

#include "log_sender.h"

namespace Dai {
namespace Client {

Log_Sender::Log_Sender(Protocol* protocol) :
    QObject(),
    protocol_(protocol)
{
    moveToThread(protocol->thread());
    timer_.moveToThread(protocol->thread());

    connect(&timer_, &QTimer::timeout, this, &Log_Sender::send_log_packs);
    timer_.setSingleShot(true);

    qRegisterMetaType<Log_Value_Item>("Log_Value_Item");
    qRegisterMetaType<Log_Event_Item>("Log_Event_Item");

    auto w = protocol->worker();
    connect(this, &Log_Sender::get_log_range, w->database(), &DBManager::log_range, Qt::BlockingQueuedConnection);
    connect(this, &Log_Sender::get_log_value_data, w->database(), &DBManager::log_value_data, Qt::BlockingQueuedConnection);
    connect(this, &Log_Sender::get_log_event_data, w->database(), &DBManager::log_event_data, Qt::BlockingQueuedConnection);

    connect(w, &Worker::change, this, &Log_Sender::send_value_log, Qt::QueuedConnection);
    connect(w, &Worker::event_message, this, &Log_Sender::send_event_log, Qt::QueuedConnection);
}

void Log_Sender::send_log_range(Log_Type_Wrapper log_type, qint64 date_ms, uint8_t msg_id)
{
    if (log_type.is_valid())
    {
        protocol_->send_answer(Cmd::LOG_RANGE, msg_id) << log_type << get_log_range(log_type, date_ms);
    }
}

void Log_Sender::send_log_data(Log_Type_Wrapper log_type, QPair<quint32, quint32> range, uint8_t msg_id)
{
    if (!log_type.is_valid())
    {
        return;
    }

    QVector<quint32> not_found;

    if (log_type == LOG_VALUE)
    {
        QVector<Log_Value_Item> data_value;
        get_log_value_data(range, &not_found, &data_value);
        protocol_->send_answer(Cmd::LOG_DATA, msg_id) << log_type << not_found << data_value;
    }
    else // log_type == LOG_EVENT
    {
        QVector<Log_Event_Item> data_event;
        get_log_event_data(range, &not_found, &data_event);
        protocol_->send_answer(Cmd::LOG_DATA, msg_id) << log_type << not_found << data_event;
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
        protocol_->send(Cmd::LOG_PACK_VALUES) << value_pack_;
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
