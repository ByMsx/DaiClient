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

void Structure_Synchronizer::send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice *data_dev, Helpz::Database::Thread *thread)
{
    if (!protocol_)
    {
        return;
    }

    if (struct_type & STRUCT_TYPE_HASH_FLAG)
    {
        struct_type &= ~STRUCT_TYPE_HASH_FLAG;
        if (struct_type)
        {
            if (struct_type == STRUCT_TYPE_SCRIPTS)
            {
                send_structure_codes_hash(msg_id);
            }
            else
            {
                thread->add_query([this, msg_id, struct_type](Helpz::Database::Base *db)
                {
                    send_structure_hash(struct_type, msg_id, db);
                });
            }
        }
        else
        {
            thread->add_query([this, msg_id](Helpz::Database::Base *db)
            {
                send_structure_hash_for_all(msg_id, db);
            });
        }
    }
    else
    {
        if (struct_type == STRUCT_TYPE_SCRIPTS)
        {
            Helpz::apply_parse(*data_dev, Helpz::Network::Protocol::DATASTREAM_VERSION, &Structure_Synchronizer::send_structure_codes, this, msg_id);
        }
        else
        {
            thread->add_query([this, msg_id, struct_type](Helpz::Database::Base *db)
            {
                send_structure(struct_type, msg_id, db);
            });
        }
    }
}

void Structure_Synchronizer::send_structure_hash(uint8_t struct_type, uint8_t msg_id, Helpz::Database::Base* db)
{
    protocol_->send_answer(Cmd::GET_PROJECT, msg_id)
            << uint8_t(struct_type | STRUCT_TYPE_HASH_FLAG)
            << get_structure_hash(struct_type, db);
}

void Structure_Synchronizer::send_structure_hash_for_all(uint8_t msg_id, Helpz::Database::Base* db)
{
    protocol_->send_answer(Cmd::GET_PROJECT, msg_id)
            << uint8_t(STRUCT_TYPE_HASH_FLAG)
            << get_structure_hash_for_all(db);
}

void Structure_Synchronizer::send_structure(uint8_t struct_type, uint8_t msg_id, Helpz::Database::Base* db)
{
    Helpz::Network::Protocol_Sender helper = protocol_->send_answer(Cmd::GET_PROJECT, msg_id);
    helper << struct_type;
    helper.timeout(nullptr, std::chrono::minutes(5), std::chrono::seconds(90));
    add_structure_data(struct_type, &helper, db);
}

void Structure_Synchronizer::send_structure_codes_hash(uint8_t msg_id)
{
    protocol_->send_answer(Cmd::GET_PROJECT, msg_id)
            << uint8_t(STRUCT_TYPE_SCRIPTS | STRUCT_TYPE_HASH_FLAG)
            << prj_->get_codes_checksum();
}

void Structure_Synchronizer::send_structure_codes(const QVector<uint32_t> &ids, uint8_t msg_id)
{
    Helpz::Network::Protocol_Sender sender = protocol_->send_answer(Cmd::GET_PROJECT, msg_id);
    sender << uint8_t(STRUCT_TYPE_SCRIPTS);

    Code_Item* code;
    uint32_t count = 0;
    auto pos = sender.device()->pos();
    sender << count;
    for (uint32_t id: ids)
    {
        code = prj_->code_mng_.get_type(id);
        if (code->id())
        {
            sender << *code;
            ++count;
        }
    }
    sender.device()->seek(pos);
    sender << count;
    sender.device()->seek(sender.device()->size());
    // qDebug(NetClientLog) << "code sended size:" << sender.device()->size();
}

void Structure_Synchronizer::send_modify_response(const QByteArray &buffer)
{
    if (protocol_)
    {
        protocol_->send(Cmd::MODIFY_PROJECT).writeRawData(buffer.constData(), buffer.size());
    }
}

} // namespace Client
} // namespace Dai
