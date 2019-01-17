#include <QDebug>
#include <QCryptographicHash>
#include <QDataStream>
#include <QTimeZone>

#include <botan/parsing.h>

#include <Helpz/dtls_version.h>
#include <Helpz/net_version.h>
#include <Helpz/db_version.h>
#include <Helpz/srv_version.h>

#include <Dai/project.h>
#include <Dai/commands.h>

#include "worker.h"
#include "Database/db_manager.h"
#include "n_client.h"

#include "lib.h"
//#include "version.h"

namespace Dai {

Q_LOGGING_CATEGORY(NetClientLog, "net.client")

namespace Network {

void Client::sendVersion()
{
    send(cmdVersion)
        << Helpz::DTLS::ver_major() << Helpz::DTLS::ver_minor() << Helpz::DTLS::ver_build()
        << Helpz::Network::ver_major() << Helpz::Network::ver_minor() << Helpz::Network::ver_build()
        << Helpz::Database::ver_major() << Helpz::Database::ver_minor() << Helpz::Database::ver_build()
        << Helpz::Service::ver_major() << Helpz::Service::ver_minor() << Helpz::Service::ver_build()
        << Lib::ver_major() << Lib::ver_minor() << Lib::ver_build()
        << (quint8)VER_MJ << (quint8)VER_MN << (int)VER_B;
}

Client::Client(Worker *worker, const QString &hostname, quint16 port, const QString &login, const QString &password, const QUuid &device, int checkServerInterval) :
    Helpz::DTLS::Client(Botan::split_on("dai/1.1,dai/1.0", ','), Helpz::Database::ConnectionInfo()/*worker->database_info()*/,
                           qApp->applicationDirPath() + "/tls_policy.conf", hostname, port, checkServerInterval),
    m_login(login), m_password(password), m_device(device), m_import_config(false),
    worker(worker)
{
    connect(&packTimer, &QTimer::timeout, this, &Client::sendPack);
    packTimer.setSingleShot(true);

    qRegisterMetaType<CodeItem>("CodeItem");
    qRegisterMetaType<ParamValuesPack>("ParamValuesPack");
    qRegisterMetaType<EventPackItem>("EventPackItem");
    qRegisterMetaType<std::vector<uint>>("std::vector<uint>");

    connect(this, &Client::restart, worker, &Worker::serviceRestart, Qt::QueuedConnection);
    connect(this, &Client::structModify, worker, &Worker::applyStructModify, Qt::BlockingQueuedConnection);
    connect(this, &Client::setControlState, worker, &Worker::setControlState, Qt::QueuedConnection);
    connect(this, &Client::writeToItem, worker, &Worker::writeToItem, Qt::QueuedConnection);

    connect(this, &Client::setMode, worker, &Worker::setMode, Qt::QueuedConnection);
    connect(this, &Client::setCode, worker, &Worker::setCode, Qt::BlockingQueuedConnection);

//    connect(this, &Client::lostValues, worker, &Worker::sendLostValues, Qt::QueuedConnection);
    connect(this, &Client::setParamValues, worker, &Worker::setParamValues, Qt::QueuedConnection);

// -----> Sync database
    connect(this, &Client::getLogRange, worker->database(), &DBManager::getLogRange, Qt::BlockingQueuedConnection);
    connect(this, &Client::getLogData, worker->database(), &DBManager::getLogData, Qt::BlockingQueuedConnection);
// <--------------------

    connect(worker, &Worker::paramValuesChanged, this, &Client::sendParamValues, Qt::QueuedConnection);
    connect(worker, &Worker::change, this, &Client::change, Qt::QueuedConnection);
    connect(worker, &Worker::modeChanged, this, &Client::modeChanged, Qt::QueuedConnection);
    connect(worker, &Worker::groupStatusChanged, this, &Client::groupStatusChanged, Qt::QueuedConnection);
    connect(worker, &Worker::statusAdded, this, &Client::statusAdded, Qt::QueuedConnection);
    connect(worker, &Worker::statusRemoved, this, &Client::statusRemoved, Qt::QueuedConnection);

    while (!worker->prj->ptr() && !worker->prj->wait(5));
    prj = worker->prj->ptr();

    connect(this, &Client::getServerInfo, worker->prj->ptr(), &Project::dumpInfoToStream, Qt::DirectConnection);
    connect(this, &Client::setServerInfo, worker->prj->ptr(), &Project::initFromStream, Qt::BlockingQueuedConnection);
    connect(this, &Client::saveServerInfo, worker->database(), &Database::saveProject, Qt::BlockingQueuedConnection);
    connect(this, &Client::execScript, worker->prj->ptr(), &ScriptedProject::console, Qt::QueuedConnection);

    if (canConnect())
        init_client();
}

///*static*/ void Client::packValue(Prt::ValuesPack *pack, uint item_id, const DeviceItem::ValueType &raw,
//                                  const DeviceItem::ValueType &val, uint time, uint db_id)
//{
//    auto p_item = pack->add_item();
//    p_item->set_id(item_id);
//    p_item->mutable_raw_value()->CopyFrom(raw);
//    p_item->mutable_value()->CopyFrom(val);
//    p_item->set_time(time);
//    p_item->set_dbid(db_id);
//}

const QUuid& Client::device() const { return m_device; }
const QString &Client::username() const { return m_login; }

bool Client::canConnect() const
{
    return !(/*m_device.isNull() || */m_login.isEmpty() || m_password.isEmpty());
}

//void Client::setId(int id) { m_id = id; }

void Client::change(const ValuePackItem& item, bool immediately)
{
    value_pack_.push_back(item);
    packTimer.start(immediately ? 10 : 250);
}

void Client::eventLog(const EventPackItem& item)
{
    if (item.type_id == QtDebugMsg && item.category.startsWith("net"))
        return;
    event_pack_.push_back(item);
    packTimer.start(1000);
//    auto helper = send(cmdEventMessage);
//    helper << type << category << text << time << db_id;
//    events_data += helper.pop_message();
}

void Client::modeChanged(uint mode_id, quint32 group_id) {
    qDebug(NetClientLog) << "modeChanged" << mode_id << group_id;
    send(cmdChangedMode) << mode_id << group_id;
}

void Client::statusAdded(quint32 group_id, quint32 info_id, const QStringList &args) {
    send(cmdGroupStatusAdded) << group_id << info_id << args;
}

void Client::statusRemoved(quint32 group_id, quint32 info_id) {
    send(cmdGroupStatusRemoved) << group_id << info_id;
}

void Client::groupStatusChanged(quint32 group_id, quint32 status)
{
    send(cmdChangedStatus) << group_id << status;
}

//void Client::sendLostValues(const QVector<ValuePackItem> &valuesPack) {
//    send(cmdGetLostValues) << valuesPack;
//}
//void Client::sendNotFoundIds(const QVector<quint32> &ids) {
//    send(cmdIdsNotFound) << ids;
//}

void Client::sendParamValues(const ParamValuesPack &pack)
{
    send(cmdSetParamValues) << pack;
}

void Client::setDevice(const QUuid &devive_uuid)
{
    if (m_device != devive_uuid)
        m_device = devive_uuid;
    // TODO: Close connection?
}

void Client::setLogin(const QString &username)
{
    if (m_login != username)
        m_login = username;
}

void Client::setPassword(const QString &password)
{
    if (m_password != password)
        m_password = password;
}

void Client::setImportFlag(bool import_config)
{
    if (m_import_config != import_config)
    {
        m_import_config = import_config;
        init_client();
    }
}

void Client::refreshAuth(const QUuid &devive_uuid, const QString &username, const QString &password)
{
    close_connection();
    setDevice(devive_uuid);
    setLogin(username);
    setPassword(password);
    if (canConnect())
        init_client();
}

void Client::importDevice(const QUuid &devive_uuid)
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

QUuid Client::createDevice(const QString &name, const QString &latin, const QString &description)
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

QVector<QPair<QUuid, QString>> Client::getUserDevices()
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

void Client::proccessMessage(quint16 cmd, QDataStream &msg)
{
    switch(cmd)
    {
    case cmdNoAuth:
//        qDebug() << "NoAuth";
        // send(cmdAuth) << m_login << m_password << m_device;
        sendAuthInfo();
        break;
    case cmdAuth:
    {
        bool authorized = Helpz::parse<bool>(msg);
        qDebug(NetClientLog) << "Auth" << authorized;

        if (!authorized)
        {
            lastUserDevices = Helpz::parse<QVector<QPair<QUuid, QString>>>(msg);
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
    }
    case cmdCreateDevice:
    {
        auto wait_it = wait_map.find(cmdCreateDevice);
        if (wait_it != wait_map.cend())
            wait_it->second->finish(Helpz::parse<QUuid>(msg));
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
        auto dt = QDateTime::currentDateTime();
        sendVersion();
        send(cmdDateTime) << dt << dt.timeZone();

        send(cmdItemTypeList) << prj->ItemTypeMng;
        send(cmdGroupTypeList) << prj->GroupTypeMng;
        send(cmdModeTypeLIst) << prj->ModeTypeMng;
        send(cmdSignList) << prj->SignMng;
        send(cmdStatusTypeList) << prj->StatusTypeMng;
        send(cmdStatusList) << prj->StatusMng;
        send(cmdParamList) << prj->ParamMng;

        send(cmdCodesChecksum) << prj->get_codes_checksum();

        send(cmdServerStructureInfo) << prj->sections() << prj->devices();
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
        applyParse(&Client::setControlState, msg);
        break;
    case cmdWriteToItem:
        applyParse(&Client::writeToItem, msg);
        break;
    case cmdSetMode:
        applyParse(&Client::setMode, msg);
        break;
//    case cmdGetLostValues:
//        applyParse(&Client::lostValues, msg);
//        break;
    case cmdSetParamValues:
        applyParse(&Client::setParamValues, msg);
        break;

    case cmdSetCode:
        send(cmd) << applyParse(&Client::setCode, msg);
        break;

    case cmdGetCode:
        send(cmd) << Helpz::applyParse(&CodeManager::type, &prj->CodeMng, msg);
        break;

    case cmdRestart:
        qCDebug(NetClientLog) << "Restart command received";
        restart();
        break;

    case cmdExecScript:
        execScript(Helpz::parse<QString>(msg));
        break;

// -----> Sync database
    case cmdLogRange: {
        uint8_t log_type; qint64 date_ms;
        while(!msg.atEnd()) {
            Helpz::parse_out(msg, log_type, date_ms);
            if (log_type != Dai::ValueLog && log_type != Dai::EventLog)
                break;

            send(cmd) << log_type << getLogRange(log_type, date_ms);
        }
        break;
    }

    case cmdLogData: {
        uint8_t log_type; QPair<quint32, quint32> range;
        Helpz::parse_out(msg, log_type, range);
        if (log_type != Dai::ValueLog && log_type != Dai::EventLog)
            break;

        auto res = getLogData(log_type, range);
#if (__cplusplus > 201402L) && (!defined(__GNUC__) || (__GNUC__ >= 7))
        try {
            switch (log_type) {
            case ValueLog: send(cmd) << log_type << res.not_found << std::get<0>(res.data); break;
            case EventLog: send(cmd) << log_type << res.not_found << std::get<1>(res.data); break;
            default: break;
            }
        }
        catch (const std::bad_variant_access&) {}
#else
        switch (log_type) {
            case ValueLog: send(cmd) << log_type << res.not_found << res.data_value; break;
            case EventLog: send(cmd) << log_type << res.not_found << res.data_event; break;
        default: break;
        }
#endif
        break;
    }
// <--------------------

// -----> Struct modify
    case cmdStructModify: {
        quint8 modifyType;
        Helpz::parse_out(msg, modifyType);
        qCDebug(NetClientLog) << "Received strucrure modify" << (StructureType)modifyType << "size" << msg.device()->size();
        send(cmd) << structModify(modifyType, &msg);
        break;
    }
// <--------------------

    case cmdFilePart:
    {
        if (!fileInfo)
        {
            fileInfo.reset(new ReceiveFileInfo);
            if (!fileInfo->file.open())
            {
                qCWarning(NetClientLog).noquote() << "Receive file fail." << fileInfo->file.errorString();
                fileInfo.reset();
                break;
            }

            connect(&fileInfo->timer, &QTimer::timeout, this, &Client::filePartTimeout);
        }

        FilePart filePart;
        msg >> filePart;

        if (fileInfo->file.pos() != filePart.pos)
        {
            if (fileInfo->file.size() <= filePart.pos)
                fileInfo->file.resize(filePart.pos + 1);
            fileInfo->file.seek(filePart.pos);
        }
        fileInfo->file.write(filePart.data);
        fileInfo->hash.addData(filePart.data);

        fileInfo->timer.start();
        break;
    }
    case cmdFileHash:
    {
        QByteArray fileHash;
        QString fileName;

        msg >> fileHash >> fileName;
        break;
    }

    default:
        if (cmd >= cmdAuth)
            qCCritical(NetClientLog) << "UNKNOWN MESSAGE" << cmd;
        break;
    }
}

void Client::sendPack()
{
    if (value_pack_.size())
    {
//        qCDebug(NetClientLog) << "Send changes count" << pack.item_size();
        send(cmdChangedValues) << value_pack_;
        value_pack_.clear();
    }

    if (event_pack_.size())
    {
//        std::cout << "Send events size" << event_pack_.size() << std::endl;
        send(cmdEventMessage) << event_pack_;
        event_pack_.clear();
    }
}

void Client::filePartTimeout()
{
    if (!fileInfo)
        return;

    fileInfo.reset();
}

void Client::sendAuthInfo()
{
    send(cmdAuth) << m_login << m_password << m_device;
}

void Client::sendLogData(uint8_t log_type, const QPair<quint32, quint32> &range)
{

}

void Client::readyWrite()
{
    qCDebug(NetClientLog) << "Connected. Server choose protocol:" << dtls->application_protocol().c_str();
    sendAuthInfo();
}

} // namespace Network
} // namespace Dai
