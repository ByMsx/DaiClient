#ifndef DAI_NET_PROTOCOL_H
#define DAI_NET_PROTOCOL_H

#include <QLoggingCategory>

#include <Helpz/net_protocol.h>

#include <plus/dai/authentication_info.h>

namespace Dai {

class Worker;

namespace Client {

Q_DECLARE_LOGGING_CATEGORY(NetClientLog)

class Protocol : public QObject, public Helpz::Network::Protocol
{
    Q_OBJECT
public:
    Protocol(Worker* worker, const Authentication_Info &auth_info);

    Worker* worker() const;

    const Authentication_Info& auth_info() const;

private:
    Worker *worker_;
    Authentication_Info auth_info_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_NET_PROTOCOL_H
