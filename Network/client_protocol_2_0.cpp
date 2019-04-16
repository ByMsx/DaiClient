#include <QTemporaryFile>

#include <Helpz/dtls_version.h>
#include <Helpz/net_version.h>
#include <Helpz/db_version.h>
#include <Helpz/srv_version.h>
#include <Helpz/dtls_client_node.h>

#include <Dai/commands.h>
#include <Dai/lib.h>

#include "worker.h"
#include "client_protocol_2_0.h"

namespace Dai {
namespace Client {

Protocol_2_0::Protocol_2_0(Worker *worker, Structure_Synchronizer *structure_synchronizer, const Authentication_Info &auth_info) :
    Protocol{worker, auth_info},
    log_sender_(this),
    structure_sync_(structure_synchronizer)
{
    connect_worker_signals();
}

Protocol_2_0::~Protocol_2_0()
{
    QMetaObject::invokeMethod(structure_sync_, "set_protocol", Qt::QueuedConnection);
}

void Protocol_2_0::connect_worker_signals()
{
    qRegisterMetaType<ParamValuesPack>("ParamValuesPack");

    prj_ = worker()->prj->ptr();

    connect(this, &Protocol_2_0::restart, worker(), &Worker::restart_service_object, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::write_to_item, worker(), &Worker::writeToItem, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::set_mode, worker(), &Worker::setMode, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::set_param_values, worker(), &Worker::setParamValues, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::exec_script_command, worker()->prj->ptr(), &ScriptedProject::console, Qt::QueuedConnection);

    connect(worker(), &Worker::modeChanged, this, &Protocol_2_0::send_mode, Qt::QueuedConnection);
    connect(worker(), &Worker::status_added, this, &Protocol_2_0::send_status_added, Qt::QueuedConnection);
    connect(worker(), &Worker::status_removed, this, &Protocol_2_0::send_status_removed, Qt::QueuedConnection);
    connect(worker(), &Worker::paramValuesChanged, this, &Protocol_2_0::send_param_values, Qt::QueuedConnection);

    /*
    qRegisterMetaType<Code_Item>("Code_Item");
    qRegisterMetaType<std::vector<uint>>("std::vector<uint>");
    connect(this, &Protocol_2_0::setCode, worker, &Worker::setCode, Qt::BlockingQueuedConnection);
//    connect(this, &Protocol_2_0::lostValues, worker, &Worker::sendLostValues, Qt::QueuedConnection);
    connect(this, &Protocol_2_0::getServerInfo, worker->prj->ptr(), &Project::dumpInfoToStream, Qt::DirectConnection);
    connect(this, &Protocol_2_0::setServerInfo, worker->prj->ptr(), &Project::initFromStream, Qt::BlockingQueuedConnection);
    connect(this, &Protocol_2_0::saveServerInfo, worker->database(), &Database::saveProject, Qt::BlockingQueuedConnection);
    */
}

void Protocol_2_0::ready_write()
{
    auto ctrl = dynamic_cast<Helpz::DTLS::Client_Node*>(writer());
    if (ctrl)
    {
        qCDebug(NetClientLog) << "Connected. Server choose protocol:" << ctrl->application_protocol().c_str();
    }

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

    case Cmd::RESTART:              apply_parse(data_dev, &Protocol_2_0::restart);              break;
    case Cmd::WRITE_TO_ITEM:        apply_parse(data_dev, &Protocol_2_0::write_to_item);        break;
    case Cmd::WRITE_TO_ITEM_FILE:   process_item_file(data_dev);                                break;
    case Cmd::SET_MODE:             apply_parse(data_dev, &Protocol_2_0::set_mode);             break;
    case Cmd::SET_PARAM_VALUES:     apply_parse(data_dev, &Protocol_2_0::set_param_values);     break;
    case Cmd::EXEC_SCRIPT_COMMAND:  apply_parse(data_dev, &Protocol_2_0::parse_script_command, &data_dev);  break;

    case Cmd::GET_PROJECT:          Helpz::apply_parse(data_dev, DATASTREAM_VERSION, &Structure_Synchronizer::send_project_structure, structure_sync_, msg_id, &data_dev); break;
    case Cmd::MODIFY_PROJECT:       Helpz::apply_parse(data_dev, DATASTREAM_VERSION, &Structure_Synchronizer::process_modify_message, structure_sync_, &data_dev, worker()->db_pending(), QString()); break;

    default:
        if (cmd >= Helpz::Network::Cmd::USER_COMMAND)
        {
            qCCritical(NetClientLog) << "UNKNOWN MESSAGE" << cmd;
        }
        break;
    }
}

void Protocol_2_0::process_answer_message(uint8_t msg_id, uint16_t cmd, QIODevice& /*data_dev*/)
{
    qCWarning(NetClientLog) << "unprocess answer" << int(msg_id) << cmd;
}

void Protocol_2_0::parse_script_command(uint32_t user_id, const QString& script, QIODevice* data_dev)
{
    QVariantList arguments;
    bool is_function = data_dev->bytesAvailable();
    if (is_function)
    {
        parse_out(*data_dev, arguments);
    }
    exec_script_command(user_id, script, is_function, arguments);
}

void Protocol_2_0::process_item_file(QIODevice& data_dev)
{
    if (!data_dev.isOpen())
    {
        if (!data_dev.open(QIODevice::ReadOnly))
        {
            qCCritical(NetClientLog) << "Fail to open data_dev in process_item_file";
            return;
        }
    }

    QString file_name;
    auto file = dynamic_cast<QTemporaryFile*>(&data_dev);
    if (file)
    {
        file->setAutoRemove(false);
        file_name = file->fileName();
        file->close();
    }
    else
    {
        QTemporaryFile t_file;
        if (t_file.open())
        {
            t_file.setAutoRemove(false);
            file_name = t_file.fileName();

            char buffer[1024];
            int length;
            while ((length = data_dev.read(buffer, sizeof(buffer))) > 0)
                t_file.write(buffer, length);
            t_file.close();
        }
    }

    if (!file_name.isEmpty())
        QMetaObject::invokeMethod(worker(), "write_to_item_file", Qt::QueuedConnection, Q_ARG(QString, file_name));
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
    }, std::chrono::seconds(15)) << auth_info() << structure_sync_->modified();
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

void Protocol_2_0::send_mode(uint32_t user_id, uint mode_id, quint32 group_id)
{
    send(Cmd::SET_MODE) << user_id << mode_id << group_id;
}

void Protocol_2_0::send_status_added(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t)
{
    send(Cmd::ADD_STATUS) << group_id << info_id << args;
}

void Protocol_2_0::send_status_removed(quint32 group_id, quint32 info_id, uint32_t)
{
    send(Cmd::REMOVE_STATUS) << group_id << info_id;
}

void Protocol_2_0::send_param_values(uint32_t user_id, const ParamValuesPack& pack)
{
    send(Cmd::SET_PARAM_VALUES) << user_id << pack;
}

// -----------------------

#if 0
#include <botan/parsing.h>

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
    const QUuid& device() const { return m_device; }
    const QString& username() const { return m_login; }

//    bool canConnect() const override { return !(/*m_device.isNull() || */m_login.isEmpty() || m_password.isEmpty()); }
signals:
    void getServerInfo(QIODevice* dev) const;
    void setServerInfo(QIODevice* dev, QVector<Param_Type_Item> *param_items_out, bool* = nullptr);
    void saveServerInfo(const QVector<Param_Type_Item>& param_values, Project *proj);

    bool setCode(const Code_Item&);
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
            QVector<Param_Type_Item> param_values;
            emit setServerInfo(&msg, &param_values);
            emit saveServerInfo(param_values, worker->prj->ptr());

            auto wait_it = wait_map.find(cmdGetServerInfo);
            if (wait_it != wait_map.cend())
                wait_it->second->finish();
            break;
        }
        }
    }
private:
    bool m_import_config;

    Helpz::Network::WaiterMap wait_map;
    QVector<QPair<QUuid, QString>> lastUserDevices;
};
#endif

} // namespace Client
} // namespace Dai
