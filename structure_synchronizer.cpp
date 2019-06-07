#include <Helpz/db_builder.h>

#include <Dai/commands.h>
#include <Dai/db/translation.h>
#include <Dai/db/auth_user.h>
#include <Dai/db/auth_group.h>

#include "worker.h"
#include "Network/client_protocol_2_0.h"
#include "structure_synchronizer.h"

namespace Dai {
namespace Client {

Structure_Synchronizer::Structure_Synchronizer(Helpz::Database::Thread *db_thread) :
    QObject(), Dai::Structure_Synchronizer(db_thread),
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

    uint8_t flags = struct_type & STRUCT_TYPE_FLAGS;
    struct_type &= ~STRUCT_TYPE_FLAGS;

    if (flags & STRUCT_TYPE_HASH_FLAG)
    {
        if (flags & STRUCT_TYPE_ITEM_FLAG)
        {
            send_structure_items_hash(struct_type, msg_id);
        }
        else if (struct_type)
        {
            db_thread()->add_query([this, msg_id, struct_type](Helpz::Database::Base *db)
            {
                send_structure_hash(struct_type, msg_id, *db);
            });
        }
        else
        {
            db_thread()->add_query([this, msg_id](Helpz::Database::Base *db)
            {
                send_structure_hash_for_all(msg_id, *db);
            });
        }
    }
    else
    {
        if (flags & STRUCT_TYPE_ITEM_FLAG)
        {
            Helpz::apply_parse(*data_dev, Helpz::Network::Protocol::DATASTREAM_VERSION, &Structure_Synchronizer::send_structure_items, this, struct_type, msg_id);
        }
        else
        {
            db_thread()->add_query([this, msg_id, struct_type](Helpz::Database::Base *db)
            {
                send_structure(struct_type, msg_id, *db);
            });
        }
    }
}

void Structure_Synchronizer::send_structure_items_hash(uint8_t struct_type, uint8_t msg_id)
{
    db_thread()->add_query([this, struct_type, msg_id](Helpz::Database::Base *db)
    {
        Helpz::Network::Protocol_Sender helper = protocol_->send_answer(Cmd::GET_PROJECT, msg_id);
        helper << uint8_t(struct_type | STRUCT_TYPE_FLAGS);
        helper.timeout(nullptr, std::chrono::minutes(5), std::chrono::seconds(90));
        add_structure_items_hash(struct_type, helper, *db);
    });
}

void Structure_Synchronizer::add_structure_items_hash(uint8_t struct_type, QDataStream& ds, Helpz::Database::Base& db)
{
    ds << get_structure_hash_vect_by_type(struct_type, db);
}

void Structure_Synchronizer::send_structure_items(const QVector<uint32_t>& id_vect, uint8_t struct_type, uint8_t msg_id)
{
    db_thread()->add_query([this, id_vect, struct_type, msg_id](Helpz::Database::Base *db)
    {
        Helpz::Network::Protocol_Sender helper = protocol_->send_answer(Cmd::GET_PROJECT, msg_id);
        helper << uint8_t(struct_type | STRUCT_TYPE_ITEM_FLAG);
        helper.timeout(nullptr, std::chrono::minutes(5), std::chrono::seconds(90));
        add_structure_items_data(struct_type, id_vect, helper, *db);
    });
}

void Structure_Synchronizer::send_structure_hash(uint8_t struct_type, uint8_t msg_id, Helpz::Database::Base& db)
{
    protocol_->send_answer(Cmd::GET_PROJECT, msg_id)
            << uint8_t(struct_type | STRUCT_TYPE_HASH_FLAG)
            << get_structure_hash(struct_type, db);
}

void Structure_Synchronizer::send_structure_hash_for_all(uint8_t msg_id, Helpz::Database::Base& db)
{
    protocol_->send_answer(Cmd::GET_PROJECT, msg_id)
            << uint8_t(STRUCT_TYPE_HASH_FLAG)
            << get_structure_hash_for_all(db);
}

void Structure_Synchronizer::send_structure(uint8_t struct_type, uint8_t msg_id, Helpz::Database::Base& db)
{
    Helpz::Network::Protocol_Sender sender = protocol_->send_answer(Cmd::GET_PROJECT, msg_id);
    sender << struct_type;
    sender.timeout(nullptr, std::chrono::minutes(5), std::chrono::seconds(90));
    add_structure_data(struct_type, sender, db);
}

void Structure_Synchronizer::send_modify_response(uint8_t struct_type, const QByteArray &buffer)
{
    if (protocol_)
    {
        if (struct_type == STRUCT_TYPE_AUTH_USER)
        {
            QString sql = "INSERT INTO %1.house_list_employee (team_id,user_id) "
                          "SELECT 1, au.id FROM %1.house_list_employee hle "
                          "RIGHT JOIN %1.auth_user au ON hle.user_id = au.id WHERE hle.id IS NULL;";
            sql = sql.arg(protocol_->worker()->database_info().common_db_name());
            db_thread()->add_pending_query(std::move(sql), std::vector<QVariantList>());
        }

        protocol_->send(Cmd::MODIFY_PROJECT).writeRawData(buffer.constData(), buffer.size());
    }
}

} // namespace Client
} // namespace Dai
