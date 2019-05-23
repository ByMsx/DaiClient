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
    QMetaObject::invokeMethod(w->webSock_th->ptr(), "sendEventMessage", Qt::QueuedConnection,
                              Q_ARG(Project_Info, this), Q_ARG(QVector<Log_Event_Item>, QVector<Log_Event_Item>{event}));
}

void Websocket_Item::modeChanged(uint mode_id, uint group_id) {

    QMetaObject::invokeMethod(w->webSock_th->ptr(), "sendModeChanged", Qt::QueuedConnection,
                              Q_ARG(Project_Info, this), Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id));
}

void Websocket_Item::procCommand(uint32_t user_id, quint32 user_team_id, quint32 proj_id, quint8 cmd, const QByteArray &raw_data)
{
    Q_UNUSED(user_team_id)
    Q_UNUSED(proj_id)

    QByteArray data(4, Qt::Uninitialized);
    data += raw_data;
    QDataStream ds(&data, QIODevice::ReadWrite);
    ds.setVersion(Helpz::Network::Protocol::DATASTREAM_VERSION);
    ds << user_id;
    ds.device()->seek(0);

    try {
        switch (cmd) {
        case wsConnectInfo:
            send(this, w->webSock_th->ptr()->prepare_connect_state_message(id(), "127.0.0.1", QDateTime::currentDateTime().timeZone(), 0));
            break;

        case wsRestart:                 Helpz::apply_parse(ds, &Worker::restart_service_object, w); break;
        case wsWriteToDevItem:          Helpz::apply_parse(ds, &Worker::writeToItem, w); break;
        case wsChangeGroupMode:         Helpz::apply_parse(ds, &Worker::setMode, w); break;
        case wsChangeGroupParamValues:  Helpz::apply_parse(ds, &Worker::set_group_param_values, w); break;
        case wsStructModify:            Helpz::apply_parse(ds, &Client::Structure_Synchronizer::process_modify_message, w->structure_sync_.get(), ds.device(), QString()); break;
        case wsExecScript:              Helpz::apply_parse(ds, &Websocket_Item::parse_script_command, this, &ds); break;

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
    w->prj->ptr()->console(user_id, script, is_function, arguments);
}

} // namespace Dai
