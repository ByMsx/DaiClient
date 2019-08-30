#ifndef DAI_LOG_SENDER_H
#define DAI_LOG_SENDER_H

#include <QTimer>

#include <Dai/log/log_type.h>
#include <Dai/log/log_pack.h>

#include <Database/db_log_helper.h>
#include "client_protocol.h"

namespace Dai {
namespace Client {

class Log_Sender : public QObject
{
    Q_OBJECT
public:
    explicit Log_Sender(Protocol* protocol);
    ~Log_Sender();

    void send_log_range(Log_Type_Wrapper log_type, qint64 from_time_ms, qint64 to_time_ms, uint8_t msg_id);
    void send_log_data(Log_Type_Wrapper log_type, qint64 from_time_ms, qint64 to_time_ms, uint8_t msg_id);

public slots:
    void send_value_log(const Log_Value_Item &item, bool immediately = false);
    void send_event_log(const Log_Event_Item &item);
private slots:
    void send_log_packs();
private:
    Protocol* protocol_;
    Database::Log_Helper db_helper_;

    QTimer timer_;

    QVector<Log_Value_Item> value_pack_;
    QVector<Log_Event_Item> event_pack_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_LOG_SENDER_H
