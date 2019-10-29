#include <Helpz/db_builder.h>

#include <Dai/commands.h>

#include "worker.h"

#include "log_sender.h"

namespace Dai {
namespace Ver_2_2 {
namespace Client {

Log_Sender::Log_Sender(Protocol_Base *protocol) :
    protocol_(protocol)
{
}

template<typename T>
void Log_Sender::send_log_data(uint8_t log_type)
{
    Helpz::Database::Base& db = Helpz::Database::Base::get_thread_local_instance();
    QVector<T> log_data = Helpz::Database::db_build_list<T>(db, "LIMIT 200");
    if (!log_data.empty())
    {
        protocol_->send(Cmd::LOG_DATA_REQUEST).answer([log_data](QIODevice& /*dev*/)
        {
            QString where = T::table_column_names().first() + " IN (";
            for (const T& item: log_data)
            {
                where += QString::number(item.timestamp_msecs()) + ',';
            }
            where.replace(where.length() - 1, 1, ')');

            Helpz::Database::Base& db = Helpz::Database::Base::get_thread_local_instance();
            db.del(Helpz::Database::db_table_name<T>(), where);
        }).timeout([log_type]() {
            qWarning(Sync_Log) << int(log_type) << "log send timeout";
        }, std::chrono::seconds{63}, std::chrono::seconds{30}) << log_type << log_data;
    }
}

void Log_Sender::send_data(Log_Type_Wrapper log_type, uint8_t msg_id)
{
    if (log_type.is_valid())
    {
        protocol_->send_answer(Cmd::LOG_DATA_REQUEST, msg_id) << true;

        if (log_type == LOG_VALUE)
        {
            send_log_data<Log_Value_Item>(log_type);
        }
        else if (log_type == LOG_EVENT)
        {
            send_log_data<Log_Event_Item>(log_type);
        }
    }
}

} // namespace Client
} // namespace Ver_2_2
} // namespace Dai
