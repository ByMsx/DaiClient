#include <Dai/commands.h>

#include "worker.h"
#include "websocket_item.h"
//#include "n"

namespace Dai {

Websocket_Item::Websocket_Item(Worker *obj) :
    QObject(), Project_Info(),
    w(obj)
{
    set_id(1);
    set_teams({1});
    connect(w, &Worker::modeChanged, this, &Websocket_Item::modeChanged, Qt::QueuedConnection);
}

Websocket_Item::~Websocket_Item()
{
    disconnect(this, 0, 0, 0);
}

void Websocket_Item::send_event_message(const Log_Event_Item& event)
{
    QMetaObject::invokeMethod(w->websock_th_->ptr(), "sendEventMessage", Qt::QueuedConnection,
                              Q_ARG(Project_Info, this), Q_ARG(QVector<Log_Event_Item>, QVector<Log_Event_Item>{event}));
}

void Websocket_Item::modeChanged(uint mode_id, uint group_id) {

    QMetaObject::invokeMethod(w->websock_th_->ptr(), "sendModeChanged", Qt::QueuedConnection,
                              Q_ARG(Project_Info, this), Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id));
}

void Websocket_Item::proc_command(std::shared_ptr<Network::Websocket_Client> client, quint32 proj_id, quint8 cmd, const QByteArray &raw_data)
{
    Q_UNUSED(proj_id)

    QByteArray data(4, Qt::Uninitialized);
    data += raw_data;
    QDataStream ds(&data, QIODevice::ReadWrite);
    ds.setVersion(Helpz::Network::Protocol::DATASTREAM_VERSION);
    ds << client->id_;
    ds.device()->seek(0);

    try {
        switch (cmd) {
        case WS_CONNECTION_STATE:
            send(*this, w->websock_th_->ptr()->prepare_connection_state_message(id(), CS_CONNECTED));
            break;

        case WS_RESTART:                  Helpz::apply_parse(ds, &Worker::restart_service_object, w); break;
        case WS_WRITE_TO_DEV_ITEM:        Helpz::apply_parse(ds, &Worker::writeToItem, w); break;
        case WS_CHANGE_GROUP_MODE:        Helpz::apply_parse(ds, &Worker::setMode, w); break;
        case WS_CHANGE_GROUP_PARAM_VALUES:Helpz::apply_parse(ds, &Worker::set_group_param_values, w); break;
        case WS_STRUCT_MODIFY:            Helpz::apply_parse(ds, &Client::Structure_Synchronizer::process_modify_message, w->structure_sync_.get(), ds.device(), QString()); break;
        case WS_EXEC_SCRIPT:              Helpz::apply_parse(ds, &Websocket_Item::parse_script_command, this, &ds); break;

        default:
            qWarning() << "Unknown WebSocket Message:" << (WebSockCmd)cmd;
    //        pClient->sendBinaryMessage(message);
            break;
        }
    } catch(const std::exception& e) {
        qCritical() << "WebSock:" << e.what();
    } catch(...) {
        qCritical() << "WebSock unknown exception";
    }
}

void Websocket_Item::parse_script_command(uint32_t user_id, const QString& script, QDataStream* data)
{
    QVariantList arguments;
    bool is_function = data->device()->bytesAvailable();
    if (is_function)
    {
        Helpz::parse_out(*data, arguments);
    }
    w->prj()->console(user_id, script, is_function, arguments);
}

} // namespace Dai
