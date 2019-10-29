#ifndef DAI_LOG_SENDER_H
#define DAI_LOG_SENDER_H

#include <thread>
#include <mutex>
#include <condition_variable>

#include <QTimer>

#include <Dai/log/log_type.h>
#include <Dai/log/log_pack.h>

#include <Database/db_log_helper.h>
#include "client_protocol.h"

namespace Dai {
namespace Ver_2_2 {
namespace Client {

using namespace Dai::Client;

class Log_Sender
{
public:
    explicit Log_Sender(Protocol_Base* protocol);

    void send_data(Log_Type_Wrapper log_type, uint8_t msg_id);
private:

    template<typename T>
    void send_log_data(uint8_t log_type);

    Protocol_Base* protocol_;
};

} // namespace Client
} // namespace Ver_2_2
} // namespace Dai

#endif // DAI_LOG_SENDER_H
