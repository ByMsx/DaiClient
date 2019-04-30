#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <functional>

#if (__cplusplus > 201402L) && (!defined(__GNUC__) || (__GNUC__ >= 7))
#include <variant>
#endif

#include <QVector>

#include <Dai/log/log_pack.h>
#include <Dai/db/group_status_item.h>
#include <Dai/db/view.h>
#include <Dai/db/device_item_value.h>
#include <Dai/db/group_mode.h>
#include <plus/dai/database.h>

namespace Dai {

class DBManager : public Database::Helper
{
    Q_OBJECT
public:
//    DBManager(const Helpz::Database::Connection_Info &info = {{}, {}, {}}, const QString& name = {});
    using Database::Helper::Helper;

    bool setDayTime(uint id, const TimeRange& range);
    void getListValues(const QVector<quint32> &ids, QVector<quint32> &found, QVector<Log_Value_Item> &pack);

    void saveCode(uint type, const QString& code);
public slots:
};

} // namespace Dai

#endif // DATABASE_MANAGER_H
