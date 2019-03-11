#ifndef DAI_LOG_SENDER_H
#define DAI_LOG_SENDER_H

#include <QTimer>

#include <Dai/log/log_type.h>
#include <Dai/log/log_pack.h>

#include "client_protocol.h"

namespace Dai {
namespace Client {

class Log_Sender : public QObject
{
    Q_OBJECT
public:
    explicit Log_Sender(Protocol* protocol);

    void send_log_range(Log_Type_Wrapper log_type, qint64 date_ms, uint8_t msg_id);
    void send_log_data(Log_Type_Wrapper log_type, QPair<quint32, quint32> range, uint8_t msg_id);
signals:
    QPair<quint32, quint32> get_log_range(quint8 log_type, qint64 date);
    void get_log_value_data(const QPair<quint32, quint32> &range, QVector<quint32>* not_found, QVector<ValuePackItem>* data_out);
    void get_log_event_data(const QPair<quint32, quint32> &range, QVector<quint32>* not_found, QVector<EventPackItem>* data_out);

public slots:
    void send_value_log(const ValuePackItem &item, bool immediately = false);
    void send_event_log(const EventPackItem &item);
private slots:
    void send_log_packs();
private:
    Protocol* protocol_;

    QTimer timer_;

    QVector<ValuePackItem> value_pack_;
    QVector<EventPackItem> event_pack_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_LOG_SENDER_H
