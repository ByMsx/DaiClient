
#include "worker.h"
#include "client_protocol.h"

namespace Dai {
namespace Client {

Q_LOGGING_CATEGORY(NetClientLog, "net.client")

Protocol::Protocol(Worker* worker, const Authentication_Info &auth_info) :
    QObject(),
    worker_(worker), auth_info_(auth_info)
{
    moveToThread(&worker->net_protocol_thread_);
}

Worker* Protocol::worker() const
{
    return worker_;
}

const Authentication_Info &Protocol::auth_info() const
{
    return auth_info_;
}

} // namespace Client
} // namespace Dai
