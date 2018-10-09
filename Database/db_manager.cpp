#include <QSqlQuery>
#include <QSqlResult>
#include <QSqlRecord>
#include <QVariant>
#include <QDebug>

#include <memory>

#include <Dai/project.h>

#include "Network/n_client.h"
#include "db_manager.h"

namespace Dai {

DBManager::DBManager(const Helpz::Database::ConnectionInfo &info, const QString &name) :
    Database(info, name)
{
    qRegisterMetaType<Dai::DBManager::LogDataT>("Dai::DBManager::LogDataT");
}

bool DBManager::setDayTime(uint id, const TimeRange &range)
{
    return update({"house_section", {"dayStart", "dayEnd"}},
        {range.start(), range.end()}, "id=" + QString::number(id));
}

void DBManager::getListValues(const QVector<quint32>& ids, QVector<quint32> &found, QVector<ValuePackItem> &pack)
{
    QString sql("SELECT id, item_id, date, raw_value, value FROM house_logs WHERE id IN (");

    bool first = true;
    for (quint32 id: ids)
    {
        if (first) first = false;
        else
            sql += ',';
        sql += QString::number(id);
    }
    sql += ')';

    pack.clear();
    quint32 id;
    auto q = exec(sql);
    if (q.isActive())
    {
//        DeviceItem::ValueType raw_val, val;

        while(q.next())
        {
            id = q.value(0).toUInt();
            pack.push_back(ValuePackItem{ id, q.value(1).toUInt(), q.value(2).toDateTime().toMSecsSinceEpoch(), q.value(3), q.value(4)});
            found.push_back(id);
        }
    }
}

void DBManager::saveCode(uint type, const QString &code)
{
    update({"house_codes", {"text"}}, { code }, "id=" + QString::number(type));
}

// -----> Sync database

DBManager::LogDataT DBManager::getLogData(quint8 log_type, const QPair<quint32, quint32> &range)
{
    LogDataT res;

    auto getLogRangeValues = [&](const Helpz::Database::Table &table, std::function<void(const QSqlQuery&)> callback)
    {
        quint32 id, next_id = range.first;
        auto q = select(table, QString("WHERE id >= %1 AND id <= %2").arg(range.first).arg(range.second));
        if (q.isActive())
            while(q.next())
            {
                id = q.value(0).toUInt();
                while (next_id < id)
                    res.not_found.push_back(next_id++);
                ++next_id;

                callback(q);
            }
    };

    switch (log_type) {
    case ValueLog: {
        QVector<ValuePackItem> pack;
        getLogRangeValues({"house_logs", {"id", "item_id", "date", "raw_value", "value"}}, [&pack](const QSqlQuery& q) {
            pack.push_back(ValuePackItem{ q.value(0).toUInt(), q.value(1).toUInt(),
                                     q.value(2).toDateTime().toMSecsSinceEpoch(), q.value(3), q.value(4)});
        });
        res.data = std::move(pack);
        break;
    }
    case EventLog: {
        QVector<EventPackItem> pack;
        getLogRangeValues({"house_eventlog", {"id", "type", "date", "who", "msg"}}, [&pack](const QSqlQuery& q) {
            pack.push_back(EventPackItem{ q.value(0).toUInt(), q.value(1).toUInt(),
                                     q.value(2).toDateTime().toMSecsSinceEpoch(), q.value(3).toString(), q.value(4).toString()});
        });
        res.data = std::move(pack);
        break;
    }
    default:
        break;
    }
    return res;
}

QPair<quint32, quint32> DBManager::getLogRange(quint8 log_type, qint64 date_ms)
{
    QString tableName = logTableName(log_type);

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

// <--------------------

} // namespace Dai
