#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <functional>

#if (__cplusplus > 201402L) && (!defined(__GNUC__) || (__GNUC__ >= 7))
#include <variant>
#endif

#include <QVector>

#include <Dai/log/log_pack.h>
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
    QPair<quint32, quint32> log_range(quint8 log_type, qint64 date_ms);
    void log_value_data(const QPair<quint32, quint32> &range, QVector<quint32>* not_found, QVector<Log_Value_Item>* data_out);
    void log_event_data(const QPair<quint32, quint32> &range, QVector<quint32>* not_found, QVector<Log_Event_Item>* data_out);
private:
    void get_log_range_values(const Helpz::Database::Table &table, const QPair<quint32, quint32> &range, QVector<quint32>* not_found,
                              std::function<void(const QSqlQuery&)> callback);
};

} // namespace Dai

#endif // DATABASE_MANAGER_H
