#include <Dai/commands.h>

#include "worker.h"
#include "Network/client_protocol_2_0.h"
#include "structure_synchronizer.h"

namespace Dai {
namespace Client {

Structure_Synchronizer::Structure_Synchronizer() :
    QObject(),
    protocol_(nullptr)
{
    qRegisterMetaType<std::shared_ptr<Client::Protocol_2_0>>("std::shared_ptr<Client::Protocol_2_0>");
}

void Structure_Synchronizer::set_project(Project *project)
{
    prj_ = project;
}

void Structure_Synchronizer::send_status_insert(uint32_t user_id, const Group_Status_Item& item)
{
    if (protocol_)
    {
        protocol_->send(Cmd::MODIFY_PROJECT) << user_id << uint8_t(STRUCT_TYPE_GROUP_STATUS) << uint32_t(0) << uint32_t(1) << item << uint32_t(0);
    }
}

void Structure_Synchronizer::send_status_update(uint32_t user_id, const Group_Status_Item& item)
{
    if (protocol_)
    {
        protocol_->send(Cmd::MODIFY_PROJECT) << user_id << uint8_t(STRUCT_TYPE_GROUP_STATUS) << uint32_t(1) << item << uint32_t(0) << uint32_t(0);
    }
}

void Structure_Synchronizer::send_status_delete(uint32_t user_id, uint32_t group_status_id)
{
    if (protocol_)
    {
        protocol_->send(Cmd::MODIFY_PROJECT) << user_id << uint8_t(STRUCT_TYPE_GROUP_STATUS) << uint32_t(0) << uint32_t(0) << uint32_t(1) << group_status_id;
    }
}

void Structure_Synchronizer::set_protocol(std::shared_ptr<Protocol_2_0> protocol)
{
    protocol_ = std::move(protocol);
}

void Structure_Synchronizer::send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice *data_dev)
{
    if (!protocol_)
    {
        return;
    }

    Helpz::Network::Protocol_Sender helper = std::move(protocol_->send_answer(Cmd::GET_PROJECT, msg_id));
    helper << struct_type;

    std::unique_ptr<QDataStream> ds;
    std::unique_ptr<QBuffer> buf;

    bool hash_flag = struct_type & STRUCT_TYPE_HASH_FLAG;
    if (hash_flag)
    {
        struct_type &= ~STRUCT_TYPE_HASH_FLAG;

        buf.reset(new QBuffer{});
        buf->open(QIODevice::ReadWrite);
        ds.reset(new QDataStream{buf.get()});
        ds->setVersion(protocol_->DATASTREAM_VERSION);
    }
    else
    {
        ds.reset(&helper);
    }

    switch (static_cast<StructureType>(struct_type))
    {
    case STRUCT_TYPE_DEVICES:           *ds << prj_->devices();         break;
    case STRUCT_TYPE_CHECKER_TYPES:     add_checker_types(*ds);         break;
    case STRUCT_TYPE_DEVICE_ITEMS:      add_device_items(*ds);          break;
    case STRUCT_TYPE_DEVICE_ITEM_TYPES: *ds << prj_->item_type_mng_;    break;
    case STRUCT_TYPE_SECTIONS:          *ds << prj_->sections();        break;
    case STRUCT_TYPE_GROUPS:            add_groups(*ds);                break;
    case STRUCT_TYPE_GROUP_TYPES:       *ds << prj_->group_type_mng_;   break;
    case STRUCT_TYPE_GROUP_MODES:       *ds << prj_->mode_type_mng_;    break;
    case STRUCT_TYPE_GROUP_PARAMS:      *ds << prj_->get_param_items(); break;
    case STRUCT_TYPE_GROUP_PARAM_TYPES: *ds << prj_->param_mng_;        break;
    case STRUCT_TYPE_GROUP_STATUS:      add_group_status_items(*ds);    break;
    case STRUCT_TYPE_GROUP_STATUS_INFO: *ds << prj_->status_mng_;       break;
    case STRUCT_TYPE_GROUP_STATUS_TYPE: *ds << prj_->status_type_mng_;  break;
    case STRUCT_TYPE_SIGNS:             *ds << prj_->sign_mng_;         break;
    case STRUCT_TYPE_VIEW:              add_views(*ds);                 break;
    case STRUCT_TYPE_VIEW_ITEM:         add_view_itemss(*ds);           break;
    case STRUCT_TYPE_SCRIPTS:
    {
        if (hash_flag)
        {
            hash_flag = false;
            ds.reset();

            helper << prj_->get_codes_checksum();
        }
        else
        {
            Helpz::apply_parse(*data_dev, Helpz::Network::Protocol::DATASTREAM_VERSION, &Structure_Synchronizer::add_codes, this, ds.get());
            // qDebug(NetClientLog) << "code sended size:" << helper.device()->size();
        }
        break;
    }
    default:
        helper.release();
        break;
    }

    if (hash_flag)
    {
        ds->device()->seek(0);
        QByteArray data = ds->device()->readAll();
        helper << qChecksum(data.constData(), data.size());
    }
    else
    {
        ds.release();
    }

    helper.timeout(nullptr, std::chrono::minutes(5), std::chrono::seconds(90));
}

void Structure_Synchronizer::send_modify_response(const QByteArray &buffer)
{
    if (protocol_)
    {
        protocol_->send(Cmd::MODIFY_PROJECT).writeRawData(buffer.constData(), buffer.size());
    }
}

void Structure_Synchronizer::add_checker_types(QDataStream &ds)
{
    if (prj_->plugin_type_mng_)
    {
        ds << *prj_->plugin_type_mng_;
    }
}

void Structure_Synchronizer::add_device_items(QDataStream &ds)
{
    auto pos = ds.device()->pos();
    uint32_t count = 0;
    ds << quint32(0);

    for (Device* dev: prj_->devices())
    {
        for (DeviceItem* dev_item: dev->items())
        {
            ds << dev_item;
            ++count;
        }
    }
    ds.device()->seek(pos);
    ds << count;
}

void Structure_Synchronizer::add_groups(QDataStream &ds)
{
    auto pos = ds.device()->pos();
    uint32_t count = 0;
    ds << quint32(0);
    for (Section* sct: prj_->sections())
    {
        for (ItemGroup* group: sct->groups())
        {
            ds << *group;
            ++count;
        }
    }
    ds.device()->seek(pos);
    ds << count;
}

void Structure_Synchronizer::add_group_status_items(QDataStream& ds)
{
    if (!protocol_)
        return;

    QVector<Group_Status_Item> items;
    QMetaObject::invokeMethod(protocol_->worker()->database(), "get_group_status_items", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QVector<Group_Status_Item>, items));
    ds << items;
}

void Structure_Synchronizer::add_views(QDataStream& ds)
{
    if (!protocol_)
        return;

    QVector<View> items;
    QMetaObject::invokeMethod(protocol_->worker()->database(), "get_views", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QVector<View>, items));
    ds << items;
}

void Structure_Synchronizer::add_view_itemss(QDataStream& ds)
{
    if (!protocol_)
        return;

    QVector<View_Item> items;
    QMetaObject::invokeMethod(protocol_->worker()->database(), "get_view_items", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QVector<View_Item>, items));
    ds << items;
}

void Structure_Synchronizer::add_codes(const QVector<uint32_t> &ids, QDataStream *ds)
{
    Code_Item* code;
    uint32_t count = 0;
    auto pos = ds->device()->pos();
    *ds << count;
    for (uint32_t id: ids)
    {
        code = prj_->code_mng_.get_type(id);
        if (code->id())
        {
            *ds << *code;
            ++count;
        }
    }
    ds->device()->seek(pos);
    *ds << count;
    ds->device()->seek(ds->device()->size());
}

} // namespace Client
} // namespace Dai
