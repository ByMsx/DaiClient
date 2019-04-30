#include <QSqlQuery>
#include <QSqlResult>
#include <QSqlRecord>
#include <QVariant>
#include <QDebug>

#include <memory>

#include <Helpz/db_table.h>
#include <Helpz/db_builder.h>

#include <Dai/project.h>
#include <Dai/log/log_type.h>

#include "db_manager.h"

namespace Dai {

using namespace Helpz::Database;
//DBManager::DBManager(const Helpz::Database::Connection_Info& info, const QString &name) :
//    Database(info, name) {}

bool DBManager::setDayTime(uint id, const TimeRange &range)
{
    return update({db_table_name<Section>(), {"dayStart", "dayEnd"}},
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
    auto q = select(db_table<Log_Value_Item>(), suffix);
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

} // namespace Dai
