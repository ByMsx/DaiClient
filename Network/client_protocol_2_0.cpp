
#include <Helpz/dtls_version.h>
#include <Helpz/net_version.h>
#include <Helpz/db_version.h>
#include <Helpz/srv_version.h>
#include <Helpz/dtls_client_controller.h>

#include <Dai/commands.h>
#include <Dai/lib.h>

#include "worker.h"
#include "client_protocol_2_0.h"

namespace Dai {
namespace Client {

Protocol_2_0::Protocol_2_0(Worker *worker, const Authentication_Info &auth_info) :
    Protocol{worker, auth_info},
    log_sender_(this)
{
    connect_worker_signals();
}

void Protocol_2_0::connect_worker_signals()
{
    qRegisterMetaType<ParamValuesPack>("ParamValuesPack");

    while (!worker()->prj->ptr() && !worker()->prj->wait(5));
    prj_ = worker()->prj->ptr();

    connect(this, &Protocol_2_0::restart, worker(), &Worker::serviceRestart, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::write_to_item, worker(), &Worker::writeToItem, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::set_mode, worker(), &Worker::setMode, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::set_param_values, worker(), &Worker::setParamValues, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::exec_script_command, worker()->prj->ptr(), &ScriptedProject::console, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::modify_project, worker(), &Worker::applyStructModify, Qt::BlockingQueuedConnection);

    connect(worker(), &Worker::modeChanged, this, &Protocol_2_0::send_mode, Qt::QueuedConnection);
    connect(worker(), &Worker::statusAdded, this, &Protocol_2_0::send_status_added, Qt::QueuedConnection);
    connect(worker(), &Worker::statusRemoved, this, &Protocol_2_0::send_status_removed, Qt::QueuedConnection);
    connect(worker(), &Worker::paramValuesChanged, this, &Protocol_2_0::sendParamValues, Qt::QueuedConnection);

    /*
    qRegisterMetaType<CodeItem>("CodeItem");
    qRegisterMetaType<std::vector<uint>>("std::vector<uint>");
    connect(this, &Protocol_2_0::setControlState, worker, &Worker::setControlState, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::setCode, worker, &Worker::setCode, Qt::BlockingQueuedConnection);
//    connect(this, &Protocol_2_0::lostValues, worker, &Worker::sendLostValues, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::getServerInfo, worker->prj->ptr(), &Project::dumpInfoToStream, Qt::DirectConnection);
    connect(this, &Protocol_2_0::setServerInfo, worker->prj->ptr(), &Project::initFromStream, Qt::BlockingQueuedConnection);
    connect(this, &Protocol_2_0::saveServerInfo, worker->database(), &Database::saveProject, Qt::BlockingQueuedConnection);
    */
}

void Protocol_2_0::ready_write()
{
    auto ctrl = dynamic_cast<Helpz::DTLS::Client_Controller*>(writer());
    qCDebug(NetClientLog) << "Connected. Server choose protocol:" << ctrl->application_protocol().c_str();

    start_authentication();
}

void Protocol_2_0::process_message(uint8_t msg_id, uint16_t cmd, QIODevice &data_dev)
{
    switch (cmd)
    {

    case Cmd::NO_AUTH:              start_authentication(); break;
    case Cmd::VERSION:              send_version(msg_id);   break;
    case Cmd::TIME_INFO:            send_time_info(msg_id); break;

    case Cmd::LOG_RANGE:            Helpz::apply_parse(data_dev, DATASTREAM_VERSION, &Log_Sender::send_log_range, &log_sender_, msg_id);    break;
    case Cmd::LOG_DATA:             Helpz::apply_parse(data_dev, DATASTREAM_VERSION, &Log_Sender::send_log_data, &log_sender_, msg_id);     break;

    case Cmd::RESTART:              restart();   break;
    case Cmd::WRITE_TO_ITEM:        apply_parse(data_dev, &Protocol_2_0::write_to_item);        break;
    case Cmd::SET_MODE:             apply_parse(data_dev, &Protocol_2_0::set_mode);             break;
    case Cmd::SET_PARAM_VALUES:     apply_parse(data_dev, &Protocol_2_0::set_param_values);     break;
    case Cmd::EXEC_SCRIPT_COMMAND:  apply_parse(data_dev, &Protocol_2_0::exec_script_command);  break;

    case Cmd::GET_PROJECT:          apply_parse(data_dev, &Protocol_2_0::send_project_structure, msg_id, &data_dev); break;
    case Cmd::MODIFY_PROJECT:       /*send_answer(cmd, msg_id) << */apply_parse(data_dev, &Protocol_2_0::modify_project, &data_dev); break;

    default:
        if (cmd >= Helpz::Network::Cmd::USER_COMMAND)
        {
            qCCritical(NetClientLog) << "UNKNOWN MESSAGE" << cmd;
        }
        break;
    }
}

void Protocol_2_0::process_answer_message(uint8_t msg_id, uint16_t cmd, QIODevice& data_dev)
{
    qCWarning(NetClientLog) << "unprocess answer" << int(msg_id) << cmd;
}

void Protocol_2_0::start_authentication()
{
    send(Cmd::AUTH).answer([this](QIODevice& data_dev)
    {
        bool authorized;
        parse_out(data_dev, authorized);
        qDebug(NetClientLog) << "Authentication status:" << authorized;
    }).timeout([]() {
        std::cout << "Authentication timeout" << std::endl;
    }, std::chrono::seconds(15)) << auth_info();
}

void Protocol_2_0::send_project_structure(uint8_t struct_type, uint8_t msg_id, QIODevice* data_dev)
{
    Helpz::Network::Protocol_Sender helper = std::move(send_answer(Cmd::GET_PROJECT, msg_id));
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
        ds->setVersion(DATASTREAM_VERSION);
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
            apply_parse(*data_dev, &Protocol_2_0::add_codes, ds.get());
            qDebug(NetClientLog) << "code sended size:" << helper.device()->size();
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

void Protocol_2_0::add_checker_types(QDataStream& ds)
{
    if (prj_->PluginTypeMng)
    {
        ds << *prj_->PluginTypeMng;
    }
}

void Protocol_2_0::add_device_items(QDataStream& ds)
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

void Protocol_2_0::add_groups(QDataStream& ds)
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

void Protocol_2_0::add_codes(const QVector<uint32_t>& ids, QDataStream* ds)
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

void Protocol_2_0::send_version(uint8_t msg_id)
{
    send_answer(Cmd::VERSION, msg_id)
            << Helpz::DTLS::ver_major() << Helpz::DTLS::ver_minor() << Helpz::DTLS::ver_build()
            << Helpz::Network::ver_major() << Helpz::Network::ver_minor() << Helpz::Network::ver_build()
            << Helpz::Database::ver_major() << Helpz::Database::ver_minor() << Helpz::Database::ver_build()
            << Helpz::Service::ver_major() << Helpz::Service::ver_minor() << Helpz::Service::ver_build()
            << Lib::ver_major() << Lib::ver_minor() << Lib::ver_build()
            << (quint8)VER_MJ << (quint8)VER_MN << (int)VER_B;
}

void Protocol_2_0::send_time_info(uint8_t msg_id)
{
    auto dt = QDateTime::currentDateTime();
    send_answer(Cmd::TIME_INFO, msg_id) << dt << dt.timeZone();
}

void Protocol_2_0::send_mode(uint mode_id, quint32 group_id)
{
    send(Cmd::SET_MODE) << mode_id << group_id;
}

void Protocol_2_0::send_status_added(quint32 group_id, quint32 info_id, const QStringList& args)
{
    send(Cmd::ADD_STATUS) << group_id << info_id << args;
}

void Protocol_2_0::send_status_removed(quint32 group_id, quint32 info_id)
{
    send(Cmd::REMOVE_STATUS) << group_id << info_id;
}

void Protocol_2_0::sendParamValues(const ParamValuesPack& pack)
{
    send(Cmd::SET_PARAM_VALUES) << pack;
}

// -----------------------

#if 0
#include <QDebug>
#include <QCryptographicHash>
#include <QDataStream>
#include <QTimeZone>

#include <botan/parsing.h>

#include <Dai/project.h>

#include "worker.h"
#include "Database/db_manager.h"

#include <QDateTime>
#include <QTimer>

#include <Helpz/simplethread.h>
#include <Helpz/dtls_client.h>
#include <Helpz/waithelper.h>

#include <Dai/typemanager/typemanager.h>
#include <Dai/param/paramgroup.h>
#include <Dai/deviceitem.h>
#include <plus/dai/network.h>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE


#include "lib.h"
//#include "version.h"

class Client : public QObject, public Helpz::Network::Protocol
{
    Q_OBJECT
public:
    Client(Worker *worker, const QString& hostname, quint16 port, const QString& login, const QString& password, const QUuid& device, int checkServerInterval) :
        Helpz::DTLS::Protocol(Botan::split_on("dai/1.1,dai/1.0", ','),
                              qApp->applicationDirPath() + "/tls_policy.conf", hostname, port, checkServerInterval),
        m_login(login), m_password(password), m_device(device), m_import_config(false),
        worker(worker)
    {
        if (canConnect())
            init_client();
    }

    //static void packValue(Prt::ValuesPack *pack, uint item_id, const DeviceItem::ValueType &raw,
    //                                  const DeviceItem::ValueType &val, uint time, uint db_id)
    //{
    //    auto p_item = pack->add_item();
    //    p_item->set_id(item_id);
    //    p_item->mutable_raw_value()->CopyFrom(raw);
    //    p_item->mutable_value()->CopyFrom(val);
    //    p_item->set_time(time);
    //    p_item->set_dbid(db_id);
    //}
    const QUuid& device() const { return m_device; }
    const QString& username() const { return m_login; }

//    bool canConnect() const override { return !(/*m_device.isNull() || */m_login.isEmpty() || m_password.isEmpty()); }
signals:
    void getServerInfo(QIODevice* dev) const;
    void setServerInfo(QIODevice* dev, QVector<ParamTypeItem> *param_items_out, bool* = nullptr);
    void saveServerInfo(const QVector<ParamTypeItem>& param_values, Project *proj);

    void setControlState(quint32 section_id, quint32 device_type, const QVariant& raw_data);
    bool setCode(const CodeItem&);
public slots:
    void setImportFlag(bool import_config)
    {
        if (m_import_config != import_config)
        {
            m_import_config = import_config;
            init_client();
        }
    }
    void refreshAuth(const QUuid& devive_uuid, const QString& username, const QString& password)
    {
        close_connection();
        setDevice(devive_uuid);
        setLogin(username);
        setPassword(password);
        if (canConnect())
            init_client();
    }
    void importDevice(const QUuid& devive_uuid)
    {
        /**
     *   1. Установить, подключиться, получить все данные
     *   2. Отключится
     *   3. Импортировать полученные данные
     **/

        Helpz::Network::Waiter w(cmdGetServerInfo, wait_map);
        if (!w) return;

        send(cmdGetServerInfo) << m_login << m_password << devive_uuid;
        w.helper->wait();
    }
    QUuid createDevice(const QString& name, const QString &latin, const QString& description)
    {
        if (name.isEmpty() || latin.isEmpty())
            return {};

        Helpz::Network::Waiter w(cmdCreateDevice, wait_map);
        if (w)
        {
            send(cmdCreateDevice) << m_login << m_password << name << latin << description;
            if (w.helper->wait())
                return w.helper->result().toUuid();
        }

        return {};
    }

    //void setId(int id) { m_id = id; }

    //void sendLostValues(const QVector<ValuePackItem> &valuesPack) {
    //    send(cmdGetLostValues) << valuesPack;
    //}
    //void sendNotFoundIds(const QVector<quint32> &ids) {
    //    send(cmdIdsNotFound) << ids;
    //}

    QVector<QPair<QUuid, QString>> getUserDevices()
    {
        Helpz::Network::Waiter w(cmdAuth, wait_map);
        if (!w)
            return lastUserDevices;

        send(cmdAuth) << m_login << m_password << QUuid();
        bool res = w.helper->wait();
        if (res)
        {
            res = w.helper->result().toBool();

            if (res)
            {
                // TODO: What?
            }
        }
        return lastUserDevices;
    }
protected:
    void process_message(uint8_t msg_id, uint16_t cmd, QIODevice& data_dev) override
    {
        switch(cmd)
        {
        case cmdAuth:
            if (!authorized)
            {
                parse_out(data_dev, lastUserDevices);
                auto wait_it = wait_map.find(cmdAuth);
                if (wait_it != wait_map.cend())
                    wait_it->second->finish(authorized);

                // TODO: Disconnect?
                // close_connection();
                break;
            }

            if (m_import_config)
                send(cmdGetServerInfo);
            break;

        case cmdCreateDevice:
        {
            auto wait_it = wait_map.find(cmdCreateDevice);
            if (wait_it != wait_map.cend())
            {
                QUuid uuid;
                parse_out(data_dev, uuid);
                wait_it->second->finish(uuid);
            }
            break;
        }
        case cmdServerInfo:
        {
            close_connection();
            QVector<ParamTypeItem> param_values;
            emit setServerInfo(&msg, &param_values);
            emit saveServerInfo(param_values, worker->prj->ptr());

            auto wait_it = wait_map.find(cmdGetServerInfo);
            if (wait_it != wait_map.cend())
                wait_it->second->finish();
            break;
        }
        case cmdGetServerInfo:
        {
            break;

            //        QByteArray dateData;
            //        {
            //            QDataStream ds(&dateData, QIODevice::WriteOnly);
            //            ds << QDateTime::currentDateTime();
            //        }

            //        info->set_datedata(dateData.constData(), dateData.size());
            //        sendMessage(this, cmdServerInfo, info.get());

            //        auto&& helper = send(cmdServerInfo);
            //        auto dt = QDateTime::currentDateTime();
            //        helper << dt << dt.timeZone();
            //        emit getServerInfo(&helper.dataStream());
            //        break;
        }
        case cmdSetControlState:
            apply_parse(data_dev, &Client::setControlState, 1);
            break;
            //    case cmdGetLostValues:
            //        apply_parse(msg, &Client::lostValues);
            //        break;

        case cmdSetCode:
            send(cmd) << apply_parse(data_dev, &Client::setCode);
            break;

        case cmdGetCode:
            send(cmd) << Helpz::apply_parse(data_dev, static_cast<<QDataStream::Version>(DATASTREAM_VERSION), &CodeManager::type, &prj->CodeMng);
            break;

        }
    }
private:
    void sendLogData(uint8_t log_type, const QPair<quint32, quint32>& range)
    {
    }

    bool m_import_config;

    Helpz::Network::WaiterMap wait_map;
    QVector<QPair<QUuid, QString>> lastUserDevices;
};
#endif

} // namespace Client
} // namespace Dai
