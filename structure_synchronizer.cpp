#include <Dai/commands.h>

#include "structure_synchronizer.h"

namespace Dai {
namespace Client {

Structure_Synchronizer::Structure_Synchronizer() :
    QObject(),
    modified_(false)
{
}

void Structure_Synchronizer::set_project(Project *project)
{
    prj_ = project;
}

bool Structure_Synchronizer::modified() const
{
    return modified_;
}

void Structure_Synchronizer::modify(uint32_t user_id, uint8_t structType, QIODevice* data_dev)
{
    modified_ = true;
//    using namespace Network;
    EventPackItem event {0, user_id, QtDebugMsg, 0, "project", "Change project structure "};
    {
        QTextStream ts(&event.text);
        ts << static_cast<StructureType>(structType) << " size " << data_dev->size();
    }

    try
    {
        auto v = Helpz::Network::Protocol::DATASTREAM_VERSION;
        switch (structType)
        {
        case STRUCT_TYPE_DEVICES:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyDevices, db_mng);
        case STRUCT_TYPE_CHECKER_TYPES:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyCheckerTypes, db_mng);
        case STRUCT_TYPE_DEVICE_ITEMS:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyDeviceItems, db_mng);
        case STRUCT_TYPE_DEVICE_ITEM_TYPES:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyDeviceItemTypes, db_mng);
        case STRUCT_TYPE_SECTIONS:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifySections, db_mng);
        case STRUCT_TYPE_GROUPS:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyGroups, db_mng);
        case STRUCT_TYPE_GROUP_TYPES:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyGroupTypes, db_mng);
        case STRUCT_TYPE_GROUP_PARAMS:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyGroupParams, db_mng);
        case STRUCT_TYPE_GROUP_PARAM_TYPES:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyGroupParamTypes, db_mng);
        case STRUCT_TYPE_GROUP_STATUSES:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyGroupStatuses, db_mng);
        case STRUCT_TYPE_GROUP_STATUS_TYPE:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyGroupStatusTypes, db_mng);
        case STRUCT_TYPE_SIGNS:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifySigns, db_mng);
        case STRUCT_TYPE_SCRIPTS:
            return Helpz::apply_parse(*data_dev, v, &Structure_Synchronizer::applyModifyScripts, db_mng);

        default: return false;
        }
    }
    catch(const std::exception& e)
    {
        event.type_id = QtCriticalMsg;
        event.text += " EXCEPTION: " + QString::fromStdString(e.what());
    }
    add_event_message(event);
    return false;
}

void Structure_Synchronizer::send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice *data_dev, Helpz::Network::Protocol *protocol)
{
    Helpz::Network::Protocol_Sender helper = std::move(protocol->send_answer(Cmd::GET_PROJECT, msg_id));
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
        ds->setVersion(protocol->DATASTREAM_VERSION);
    }
    else
    {
        ds.reset(&helper);
    }

    switch (struct_type)
    {
    case STRUCT_TYPE_DEVICES:           *ds << prj_->devices(); break;
    case STRUCT_TYPE_CHECKER_TYPES:     add_checker_types(*ds); break;
    case STRUCT_TYPE_DEVICE_ITEMS:      add_device_items(*ds);  break;
    case STRUCT_TYPE_DEVICE_ITEM_TYPES: *ds << prj_->ItemTypeMng; break;
    case STRUCT_TYPE_SECTIONS:          *ds << prj_->sections(); break;
    case STRUCT_TYPE_GROUPS:            add_groups(*ds);        break;
    case STRUCT_TYPE_GROUP_TYPES:       *ds << prj_->GroupTypeMng; break;
    case STRUCT_TYPE_GROUP_MODES:       *ds << prj_->ModeTypeMng; break;
    case STRUCT_TYPE_GROUP_PARAMS:      *ds << prj_->get_param_items(); break;
    case STRUCT_TYPE_GROUP_PARAM_TYPES: *ds << prj_->ParamMng; break;
    case STRUCT_TYPE_GROUP_STATUSES:    *ds << prj_->StatusMng; break;
    case STRUCT_TYPE_GROUP_STATUS_TYPE: *ds << prj_->StatusTypeMng; break;
    case STRUCT_TYPE_SIGNS:             *ds << prj_->SignMng; break;
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
            Helpz::apply_parse(*data_dev, protocol->DATASTREAM_VERSION, &Structure_Synchronizer::add_codes, this, ds.get());
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

void Structure_Synchronizer::add_checker_types(QDataStream &ds)
{
    if (prj_->PluginTypeMng)
    {
        ds << *prj_->PluginTypeMng;
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
            ds << group;
            ++count;
        }
    }
    ds.device()->seek(pos);
    ds << count;
}

void Structure_Synchronizer::add_codes(const QVector<uint32_t> &ids, QDataStream *ds)
{
    CodeItem* code;
    uint32_t count = 0;
    auto pos = ds->device()->pos();
    *ds << count;
    for (uint32_t id: ids)
    {
        code = prj_->CodeMng.getType(id);
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
