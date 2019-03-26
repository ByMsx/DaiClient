#include <QSqlQuery>
#include <QSqlResult>
#include <QSqlRecord>
#include <QVariant>
#include <QDebug>

#include <memory>

#include <Dai/project.h>
#include <Dai/log/log_type.h>
#include <plus/dai/database_table.h>

#include "db_manager.h"

namespace Dai {

//DBManager::DBManager(const Helpz::Database::Connection_Info& info, const QString &name) :
//    Database(info, name) {}

bool DBManager::setDayTime(uint id, const TimeRange &range)
{
    return update({Database::db_table_name<Section>(), {"dayStart", "dayEnd"}},
        {range.start(), range.end()}, "id=" + QString::number(id));
}

void DBManager::getListValues(const QVector<quint32>& ids, QVector<quint32> &found, QVector<Log_Value_Item> &pack)
{
    QString suffix("WHERE id IN (");

    bool first = true;
    for (quint32 id: ids)
    {
        if (first) first = false;
        else
            suffix += ',';
        suffix += QString::number(id);
    }
    suffix += ')';

    pack.clear();
    quint32 id;
    auto q = select(Database::db_table<Log_Value_Item>(), suffix);
    if (q.isActive())
    {
//        DeviceItem::ValueType raw_val, val;

        while(q.next())
        {
            id = q.value(0).toUInt();
            pack.push_back(Log_Value_Item{ id, q.value(1).toDateTime().toMSecsSinceEpoch(), q.value(2).toUInt(), q.value(3).toUInt(), q.value(4), q.value(5)});
            found.push_back(id);
        }
    }
}

void DBManager::saveCode(uint type, const QString &code)
{
    update({"house_codes", {"text"}}, { code }, "id=" + QString::number(type));
}

// -----> Sync database

QPair<quint32, quint32> DBManager::log_range(quint8 log_type, qint64 date_ms)
{
    auto log_table_name = [](uint8_t log_type, const QString& db_name = QString()) -> QString
    {
        switch (static_cast<Log_Type>(log_type))
        {
        case LOG_VALUE: return Database::db_table_name<Log_Value_Item>(db_name);
        case LOG_EVENT: return Database::db_table_name<Log_Event_Item>(db_name);
        default: break;
        }
        return {};
    };
    QString tableName = log_table_name(log_type);

    QString where;
    QVariantList values;

    QDateTime date = QDateTime::fromMSecsSinceEpoch(date_ms);
    if (date_ms > 0 && date.isValid()) {
        where = "WHERE date > ? ";
        values.push_back(date);
    }
    where += "ORDER BY date ASC LIMIT 10000;";

    uint32_t start = 0, end = 0, id;

    QSqlQuery q = select({tableName, {"id"}}, where, values);
    if (q.isActive())
        while(q.next()) {
            id = q.value(0).value<uint32_t>();
            if (start == 0)
                start = end = id;
            else if (id == (end + 1))
                ++end;
            else
                break;
        }
    return {start, end};
}

void DBManager::log_value_data(const QPair<quint32, quint32>& range, QVector<quint32>* not_found, QVector<Log_Value_Item>* data_out)
{
    get_log_range_values(Database::db_table<Log_Value_Item>(), range, not_found, [data_out](const QSqlQuery& q)
    {
        data_out->push_back(Log_Value_Item{ q.value(0).toUInt(), q.value(1).toDateTime().toMSecsSinceEpoch(), q.value(2).toUInt(),
                                            q.value(3).toUInt(), q.value(4), q.value(5)});
    });
}

void DBManager::log_event_data(const QPair<quint32, quint32>& range, QVector<quint32>* not_found, QVector<Log_Event_Item>* data_out)
{
    get_log_range_values(Database::db_table<Log_Event_Item>(), range, not_found, [data_out](const QSqlQuery& q)
    {
        data_out->push_back(Log_Event_Item{ q.value(0).toUInt(), q.value(1).toDateTime().toMSecsSinceEpoch(), q.value(2).toUInt(),
                                            q.value(3).toUInt(), q.value(4).toString(), q.value(5).toString()});
    });
}

void DBManager::get_log_range_values(const Helpz::Database::Table& table, const QPair<quint32, quint32>& range, QVector<quint32>* not_found,
                                     std::function<void (const QSqlQuery&)> callback)
{
    quint32 id, next_id = range.first;
    auto q = select(table, QString("WHERE id >= %1 AND id <= %2").arg(range.first).arg(range.second));
    while(q.isActive() && q.next())
    {
        id = q.value(0).toUInt();
        while (next_id < id)
        {
            not_found->push_back(next_id++);
        }
        ++next_id;

        callback(q);
    }
}

// <--------------------

} // namespace Dai
