#ifndef DAI_WEBSOCKET_ITEM_H
#define DAI_WEBSOCKET_ITEM_H

#include <memory>

#include "plus/dai/proj_info.h"
#include <Dai/log/log_pack.h>

namespace Dai {
namespace Network {
class Websocket_Client;
} // namespace Network

class Worker;

class Websocket_Item : public QObject, public Project_Info
{
    Q_OBJECT
public:
    Websocket_Item(Worker* obj);
    ~Websocket_Item();

signals:
    void send(const Project_Info &proj, const QByteArray& data) const;
public slots:
    void send_event_message(const Log_Event_Item& event);

    void modeChanged(uint mode_id, uint group_id);
    void proc_command(std::shared_ptr<Network::Websocket_Client> client, quint32 proj_id, quint8 cmd, const QByteArray& raw_data);
    void parse_script_command(uint32_t user_id, const QString &script, QDataStream* data);
private:
    Worker* w;
};

} // namespace Dai

#endif // DAI_WEBSOCKET_ITEM_H
