#include <QDateTime>

#include <Helpz/db_base.h>

#include <Dai/log/log_type.h>

#include "db_log_helper.h"

namespace Dai {
namespace Database {

Log_Helper::Log_Helper(Helpz::Database::Thread* db_thread) :
    db_(db_thread)
{
}

void Log_Helper::log_range(quint8 log_type, qint64 date_ms, std::function<void (const QPair<quint32, quint32>&)> callback)
{
    QString where;
    QVariantList values;

    QDateTime date = QDateTime::fromMSecsSinceEpoch(date_ms);
    if (date_ms > 0 && date.isValid())
    {
        where = "WHERE date > ? ";
        values.push_back(date);
    }
    where += "ORDER BY date ASC LIMIT 10000;";

    QString sql = db_->db()->select_query({log_table_name(log_type), {"id"}}, where, values.size());

    db_->add_query([sql, values, callback](Helpz::Database::Base* db)
    {
        uint32_t start = 0, end = 0, id;

        QSqlQuery q = db->exec(sql, values);
        if (q.isActive())
        {
            while(q.next())
            {
                id = q.value(0).value<uint32_t>();
                if (start == 0)
                    start = end = id;
                else if (id == (end + 1))
                    ++end;
                else
                    break;
            }
        }
        callback(qMakePair(start, end));
    });
}

void Log_Helper::log_value_data(const QPair<quint32, quint32>& range, std::function<void(const QVector<quint32>&, const QVector<Log_Value_Item>&)> callback)
{
    log_data(range, callback);
}

void Log_Helper::log_event_data(const QPair<quint32, quint32>& range, std::function<void(const QVector<quint32>&, const QVector<Log_Event_Item>&)> callback)
{
    log_data(range, callback);
}

QString Log_Helper::get_log_range_sql(const Helpz::Database::Table& table, const QPair<quint32, quint32>& range)
{
    return db_->db()->select_query(table, QString("WHERE id >= %1 AND id <= %2").arg(range.first).arg(range.second));
}

template<typename T>
void Log_Helper::log_data(const QPair<quint32, quint32>& range, std::function<void (const QVector<quint32>&, const QVector<T>&)>& callback)
{
    quint32 range_first = range.first;
    QString sql = get_log_range_sql(Helpz::Database::db_table<T>(), range);

    db_->add_query([sql, range_first, callback](Helpz::Database::Base* db)
    {
        QSqlQuery q = db->exec(sql);
        if (q.isActive())
        {
            quint32 id, next_id = range_first;
            QVector<quint32> not_found;
            QVector<T> data;
            while (q.next())
            {
                id = q.value(0).toUInt();
                while (next_id < id)
                {
                    not_found.push_back(next_id++);
                }
                ++next_id;

//                data.push_back(db_build<T>(q));
            }

            callback(not_found, data);
        }
    });
}

} // namespace Database
} // namespace Dai
