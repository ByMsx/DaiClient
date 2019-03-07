#include "client_protocol.h"

namespace Dai {
namespace Client {

Q_LOGGING_CATEGORY(NetClientLog, "net.client")

Protocol::Protocol(const Authentication_Info &auth_info) :
    auth_info_(auth_info)
{
}

const Authentication_Info &Protocol::auth_info() const
{
    return auth_info_;
}

} // namespace Client
} // namespace Dai
