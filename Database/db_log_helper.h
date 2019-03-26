#ifndef DAI_DB_LOG_HELPER_H
#define DAI_DB_LOG_HELPER_H

#include <functional>

#include <QPair>

#include <Helpz/db_thread.h>
#include <Helpz/db_table.h>

#include <Dai/log/log_pack.h>

namespace Dai {
namespace Database {

class Log_Helper
{
public:
    Log_Helper(Helpz::Database::Thread* db_thread);

    void log_range(quint8 log_type, qint64 date_ms, std::function<void (const QPair<quint32, quint32>&)> callback);
    void log_value_data(const QPair<quint32, quint32>& range, std::function<void (const QVector<quint32>&, const QVector<Log_Value_Item>&)> callback);
    void log_event_data(const QPair<quint32, quint32>& range, std::function<void (const QVector<quint32>&, const QVector<Log_Event_Item>&)> callback);

private:
    template<typename T>
    void log_data(const QPair<quint32, quint32>& range, std::function<void (const QVector<quint32>&, const QVector<T>&)>& callback);

    Helpz::Database::Thread* db_;
};

} // namespace Database
} // namespace Dai

#endif // DAI_DB_LOG_HELPER_H
