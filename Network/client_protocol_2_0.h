#ifndef DAI_PROTOCOL_2_0_H
#define DAI_PROTOCOL_2_0_H

#include "client_protocol.h"

namespace Dai {

class Worker;

namespace Client {

class Protocol_2_0 : public Protocol
{
    Q_OBJECT
public:
    Protocol_2_0(const Authentication_Info &auth_info, Worker* worker);

private:
    void connect_worker_signals();

    void ready_write() override;
    void process_message(uint8_t msg_id, uint16_t cmd, QIODevice& data_dev) override;

    void start_authentication();

    Worker *worker_;
};

} // namespace Client
} // namespace Dai

#endif // DAI_PROTOCOL_2_0_H
