#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <functional>
#include <variant>

#include <QVector>

#include "Dai/database.h"

namespace Dai {

class DBManager : public QObject, public Database
{
    Q_OBJECT
public:
    DBManager(const Helpz::Database::ConnectionInfo &info = {{}, {}, {}}, const QString& name = {});
//    using Database::Database;

    bool setDayTime(uint id, const TimeRange& range);
    void getListValues(const QVector<quint32> &ids, QVector<quint32> &found, QVector<ValuePackItem> &pack);

    void saveCode(uint type, const QString& code);

// -----> Sync database
    struct LogDataT {
        QVector<quint32> not_found;
        std::variant<QVector<ValuePackItem>, QVector<EventPackItem>> data;
    };

    Dai::DBManager::LogDataT getLogData(quint8 log_type, const QPair<quint32, quint32> &range);
    QPair<quint32, quint32> getLogRange(quint8 log_type, qint64 date_ms);
// <--------------------
};

} // namespace Dai

#endif // DATABASE_MANAGER_H
