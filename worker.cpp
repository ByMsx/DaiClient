
#include <QDebug>
#include <QDir>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <botan/parsing.h>

#include <Helpz/consolereader.h>
#include <Helpz/dtls_client.h>

#include <Dai/commands.h>
#include <Dai/checkerinterface.h>
#include <Dai/db/group_status_item.h>

#include "websocket_item.h"
#include "worker.h"

namespace Dai {

namespace Z = Helpz;
using namespace std::placeholders;

Worker::Worker(QObject *parent) :
    QObject(parent)
{
    qRegisterMetaType<uint32_t>("uint32_t");

    auto s = settings();

    int log_period = init_logging(s.get());
    init_Database(s.get());
    init_Project(s.get()); // инициализация структуры проекта
    init_Checker(s.get()); // запуск потока опроса устройств
    init_network_client(s.get()); // подключение к серверу
    init_LogTimer(log_period); // сохранение статуса устройства по таймеру

    // используется для подключения к Orange на прямую
    initDjango(s.get());
    initWebSocketManager(s.get());

    emit started();

//    QTimer::singleShot(15000, thread(), SLOT(quit())); // check delete error
}

template<typename T>
void stop_thread(T** thread_ptr)
{
    if (*thread_ptr)
    {
        QThread* thread = *thread_ptr;
        thread->quit();
        if (!thread->wait(15000))
            thread->terminate();
        delete *thread_ptr;
        *thread_ptr = nullptr;
    }
}

Worker::~Worker()
{
    blockSignals(true);
    QObject::disconnect(this, 0, 0, 0);

    websock_item.reset();
    stop_thread(&webSock_th);
    stop_thread(&django_th);

    stop_thread(&log_timer_thread_);
    item_values_timer.stop();

    net_protocol_thread_.quit(); net_protocol_thread_.wait();
    net_thread_.reset();
    stop_thread(&checker_th);
    stop_thread(&prj);

    if (db_mng)
        delete db_mng;
    db_pending_thread_.reset();
}

DBManager* Worker::database() const { return db_mng; }
Helpz::Database::Thread* Worker::db_pending() { return db_pending_thread_.get(); }
const Helpz::Database::Connection_Info& Worker::database_info() const { return *db_info_; }

/*static*/ std::unique_ptr<QSettings> Worker::settings()
{
    QString configFileName = QCoreApplication::applicationDirPath() + QDir::separator() + QCoreApplication::applicationName() + ".conf";
    return std::unique_ptr<QSettings>(new QSettings(configFileName, QSettings::NativeFormat));
}

std::shared_ptr<Client::Protocol_2_0> Worker::net_protocol()
{
    return std::static_pointer_cast<Client::Protocol_2_0>(net_thread_->client()->protocol());
}

int Worker::init_logging(QSettings *s)
{
    std::tuple<bool, bool, int> t = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Z::Param<bool>,Z::Param<bool>,Z::Param<int>>
        #endif
            (
            s, "Log",
            Z::Param<bool>{"Debug", false},
            Z::Param<bool>{"Syslog", true},
            Z::Param<int>{"Period", 60 * 30} // 30 минут
    )();

    logg().set_debug(std::get<0>(t));
#ifdef Q_OS_UNIX
    logg().set_syslog(std::get<1>(t));
#endif
    QMetaObject::invokeMethod(&logg(), "init", Qt::QueuedConnection);
    return std::get<2>(t);
}

void Worker::init_Database(QSettings* s)
{
    db_info_ = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Z::Param<QString>,Z::Param<QString>,Z::Param<QString>,Z::Param<QString>,Z::Param<int>,Z::Param<QString>,Z::Param<QString>>
        #endif
            (
                s, "Database",
                Z::Param<QString>{"Name", "deviceaccess_local"},
                Z::Param<QString>{"User", "DaiUser"},
                Z::Param<QString>{"Password", ""},
                Z::Param<QString>{"Host", "localhost"},
                Z::Param<int>{"Port", -1},
                Z::Param<QString>{"Driver", "QMYSQL"},
                Z::Param<QString>{"ConnectOptions", QString()}
    ).unique_ptr<Helpz::Database::Connection_Info>();
    if (!db_info_)
        throw std::runtime_error("Failed get database config");

    db_pending_thread_.reset(new Helpz::Database::Thread{Helpz::Database::Connection_Info(*db_info_)});

    qRegisterMetaType<QVector<Group_Status_Item>>("QVector<Group_Status_Item>");
    qRegisterMetaType<QVector<View>>("QVector<View>");
    qRegisterMetaType<QVector<View_Item>>("QVector<View_Item>");
    db_mng = new DBManager(*db_info_, "Worker_" + QString::number((quintptr)this));
    connect(this, &Worker::status_added, this, &Worker::add_status, Qt::QueuedConnection);
    connect(this, &Worker::status_removed, this, &Worker::remove_status, Qt::QueuedConnection);
}

void Worker::init_Project(QSettings* s)
{
    Helpz::ConsoleReader* cr = nullptr;
    if (Service::instance().isImmediately())
        cr = new Helpz::ConsoleReader(this);

//    qRegisterMetaType<std::shared_ptr<Dai::Prt::ServerInfo>>("std::shared_ptr<Dai::Prt::ServerInfo>");

    prj = ScriptsThread()(s, "Server", this,
                          cr,
                          Z::Param<QString>{"SSHHost", "80.89.129.98"},
                          Z::Param<bool>{"AllowShell", false}
                          );
    prj->start(QThread::HighPriority);
}

void Worker::init_Checker(QSettings* s)
{
    qRegisterMetaType<Device*>("Device*");

    checker_th = CheckerThread()(s, "Checker", this, Z::Param<QStringList>{"Plugins", QStringList{"ModbusPlugin","WiringPiPlugin"}} );
    checker_th->start();
}

void Worker::init_network_client(QSettings* s)
{
    net_protocol_thread_.start();
    structure_sync_.moveToThread(&net_protocol_thread_);
    structure_sync_.set_project(prj->ptr());

    qRegisterMetaType<Log_Value_Item>("Log_Value_Item");
    qRegisterMetaType<Log_Event_Item>("Log_Event_Item");
    qRegisterMetaType<QVector<Log_Value_Item>>("QVector<Log_Value_Item>");
    qRegisterMetaType<QVector<Log_Event_Item>>("QVector<Log_Event_Item>");
    qRegisterMetaType<QVector<quint32>>("QVector<quint32>");

    Authentication_Info auth_info = Helpz::SettingsHelper{
                s, "RemoteServer",
                Z::Param<QString>{"Login",              QString()},
                Z::Param<QString>{"Password",           QString()},
                Z::Param<QString>{"ProjectName",        QString()},
                Z::Param<QUuid>{"Device",               QUuid()},
            }.obj<Authentication_Info>();

    if (!auth_info)
    {
        return;
    }

    const QString default_dir = qApp->applicationDirPath() + '/';
    auto [ tls_policy_file, host, port, protocols, recpnnect_interval_sec ]
            = Helpz::SettingsHelper{
                s, "RemoteServer",
                Z::Param<QString>{"TlsPolicyFile", default_dir + "tls_policy.conf"},
                Z::Param<QString>{"Host",               "deviceaccess.ru"},
                Z::Param<QString>{"Port",               "25588"},
                Z::Param<QString>{"Protocols",          "dai/2.0,dai/1.1"},
                Z::Param<uint32_t>{"ReconnectSeconds",       15}
            }();

    Helpz::DTLS::Create_Client_Protocol_Func_T func = [this, auth_info](const std::string& /*app_protocol*/) -> std::shared_ptr<Helpz::Network::Protocol>
    {
        std::shared_ptr<Client::Protocol_2_0> ptr(new Client::Protocol_2_0{this, &structure_sync_, auth_info});
        QMetaObject::invokeMethod(&structure_sync_, "set_protocol", Qt::BlockingQueuedConnection, Q_ARG(std::shared_ptr<Client::Protocol_2_0>, ptr));
        return std::static_pointer_cast<Helpz::Network::Protocol>(ptr);
    };

    Helpz::DTLS::Client_Thread_Config conf{ tls_policy_file.toStdString(), host.toStdString(), port.toStdString(),
                Botan::split_on(protocols.toStdString(), ','), recpnnect_interval_sec };
    conf.set_create_protocol_func(std::move(func));

    net_thread_.reset(new Helpz::DTLS::Client_Thread{std::move(conf)});
}

void Worker::init_LogTimer(int period)
{
    connect(&item_values_timer, &QTimer::timeout, [this]()
    {
        for (auto it: waited_item_values)
            if (!db_mng->setDevItemValue(it.first, it.second.first, it.second.second))
            {
                // TODO: Do something
            }
        waited_item_values.clear();
    });
    item_values_timer.setInterval(5000);
    item_values_timer.setSingleShot(true);

    log_timer_thread_ = new Log_Value_Save_Timer_Thread(prj->ptr(), db_pending());
    log_timer_thread_->start();
    connect(log_timer_thread_->ptr(), &Log_Value_Save_Timer::change, this, &Worker::change, Qt::QueuedConnection);
}

void Worker::initDjango(QSettings *s)
{
    django_th = DjangoThread()(s, "Django",
                             Helpz::Param<QString>{"manage", "/var/www/dai/manage.py"});
    django_th->start();
}

void Worker::initWebSocketManager(QSettings *s)
{
    std::tuple<bool> en_t = Helpz::SettingsHelper<Helpz::Param<bool>>(s, "WebSocket", Helpz::Param<bool>{"Enabled", true})();
    if (!std::get<0>(en_t))
        return;

    webSock_th = WebSocketThread()(
                s, "WebSocket",
                Helpz::Param<quint16>{"Port", 25589},
                Helpz::Param<QString>{"CertPath", QString()},
                Helpz::Param<QString>{"KeyPath", QString()});
    webSock_th->start();

    connect(webSock_th->ptr(), &Network::WebSocket::checkAuth,
                     django_th->ptr(), &DjangoHelper::checkToken, Qt::BlockingQueuedConnection);

    websock_item.reset(new Websocket_Item(this));
    connect(this, &Worker::event_message, websock_item.get(), &Websocket_Item::send_event_message, Qt::DirectConnection);
    connect(webSock_th->ptr(), &Network::WebSocket::throughCommand,
            websock_item.get(), &Websocket_Item::procCommand, Qt::BlockingQueuedConnection);
    connect(websock_item.get(), &Websocket_Item::send, webSock_th->ptr(), &Network::WebSocket::send, Qt::QueuedConnection);
}

void Worker::restart_service_object(uint32_t user_id)
{
    Log_Event_Item event {0, user_id, QtInfoMsg, 0, Service::Log().categoryName(), "The service restarts."};
    add_event_message(event);
    QTimer::singleShot(50, this, SIGNAL(serviceRestart()));
}

void Worker::logMessage(QtMsgType type, const Helpz::LogContext &ctx, const QString &str)
{
    if (ctx.category().startsWith("net"))
    {
        return;
    }

    Log_Event_Item event{0, 0, 0, type, ctx.category(), str};

    static QRegularExpression re("^(\\d+)\\|");
    QRegularExpressionMatch match = re.match(str);
    if (match.hasMatch())
    {
        event.set_user_id(match.captured(1).toUInt());
        event.set_msg(str.right(str.size() - (match.capturedEnd(1) + 1)));
    }

    add_event_message(event);
}

void Worker::add_event_message(const Log_Event_Item& event)
{
    if (db_mng->eventLog(const_cast<Log_Event_Item&>(event)))
    {
        event_message(event);
    }
}

void Worker::processCommands(const QStringList &args)
{
    QList<QCommandLineOption> opt{
        { { "c", "console" }, QCoreApplication::translate("main", "Execute command in JS console."), "script" },

        { { "d", "dev", "device" }, QCoreApplication::translate("main", "Device"), "ethX" },


        // A boolean option with a single name (-p)
        {"p",
            QCoreApplication::translate("main", "Show progress during copy")},
        // A boolean option with multiple names (-f, --force)
        {{"f", "force"},
            QCoreApplication::translate("main", "Overwrite existing files.")},
        // An option with a value
        {{"t", "target-directory"},
            QCoreApplication::translate("main", "Copy all source files into <directory>."),
            QCoreApplication::translate("main", "directory")},
    };


    QCommandLineParser parser;
    parser.setApplicationDescription("Dai service");
    parser.addHelpOption();
    parser.addOptions(opt);

    parser.process(args);

    if (parser.isSet(opt.at(0)))
    {
        QMetaObject::invokeMethod(prj->ptr(), "console", Qt::QueuedConnection,
                                  Q_ARG(QString, parser.value(opt.at(0))));
    }
    else if (false) {

    }
    else
        qCInfo(Service::Log) << args << parser.helpText();
}

QString Worker::getUserDevices()
{
    QVector<QPair<QUuid, QString>> devices;

    auto proto = net_protocol();
    if (proto)
    {
        QMetaObject::invokeMethod(proto.get(), "getUserDevices", Qt::BlockingQueuedConnection,
                              QReturnArgument<QVector<QPair<QUuid, QString>>>("QVector<QPair<QUuid, QString>>", devices));
    }

    QJsonArray json;
    QJsonObject obj;
    for (auto&& device: devices)
    {
        obj["device"] = device.first.toString();
        obj["name"] = device.second;
        json.push_back(obj);
    }

    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString Worker::getUserStatus()
{
    QJsonObject json;
    auto proto = net_protocol();
    if (proto)
    {
        json["user"] = proto->auth_info().login();
        json["device"] = proto->auth_info().device_id().toString();
    }
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

void Worker::initDevice(const QString &device, const QString &device_name, const QString &device_latin, const QString &device_desc)
{
    QUuid devive_uuid(device);
    if (devive_uuid.isNull() && (device_name.isEmpty() || device_latin.isEmpty()))
        return;

    /** Работа:
     * 1. Если есть device
     *   1. Установить, подключиться, получить все данные
     *   2. Отключится
     *   3. Импортировать полученные данные
     * 2. Если есть device_name
     *   1. Подключится с пустым device
     *   2. Отправить команду на создание нового device, подождать и получить UUID в ответе
     *   3. Сохранить UUID
     * 3. Перезагрузится
     **/

    auto proto = net_protocol();
    if (!devive_uuid.isNull() && proto)
        QMetaObject::invokeMethod(proto.get(), "importDevice", Qt::BlockingQueuedConnection, Q_ARG(QUuid, devive_uuid));

    QUuid new_uuid;
    if (!device_name.isEmpty() && proto)
        QMetaObject::invokeMethod(proto.get(), "createDevice", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QUuid, new_uuid),
                                  Q_ARG(QString, device_name), Q_ARG(QString, device_latin), Q_ARG(QString, device_desc));
    else
        new_uuid = devive_uuid;

    auto s = settings();
    s->beginGroup("RemoteServer");
    s->setValue("Device", new_uuid.toString());
    s->endGroup();

    emit serviceRestart();
}

void Worker::clearServerConfig()
{
    saveServerData(QUuid(), QString(), QString());
}

void Worker::saveServerAuthData(const QString &login, const QString &password)
{
    auto proto = net_protocol();
    if (proto)
    {
        saveServerData(proto->auth_info().device_id(), login, password);
    }
}

void Worker::saveServerData(const QUuid &devive_uuid, const QString &login, const QString &password)
{
    auto s = settings();
    s->beginGroup("RemoteServer");
    s->setValue("Device", devive_uuid.toString());
    s->setValue("Login", login);
    s->setValue("Password", password);
    s->endGroup();

    auto proto = net_protocol();
    if (proto)
    {
        QMetaObject::invokeMethod(proto.get(), "refreshAuth", Qt::QueuedConnection,
                              Q_ARG(QUuid, devive_uuid), Q_ARG(QString, login), Q_ARG(QString, password));
    }
}

bool Worker::setDayTime(uint section_id, uint dayStartSecs, uint dayEndSecs)
{
    bool res = false;
    if (Section* sct = prj->ptr()->sectionById( section_id ))
    {
        TimeRange tempRange( dayStartSecs, dayEndSecs );
        if (*sct->day_time() != tempRange)
            if (res = db_mng->setDayTime( section_id, tempRange ), res)
            {
                *sct->day_time() = tempRange;
                prj->ptr()->dayTimeChanged(/*sct*/);
            }
    }
    return res;
}

void Worker::writeToItem(uint32_t user_id, uint32_t item_id, const QVariant &raw_data)
{
    if (DeviceItem* item = prj->ptr()->itemById( item_id ))
    {
        if (item->register_type() == Item_Type::rtFile)
        {
            last_file_item_and_user_id_ = std::make_pair(item_id, user_id);
        }
        QMetaObject::invokeMethod(item->group(), "writeToControl", Qt::QueuedConnection,
                                  Q_ARG(DeviceItem*, item), Q_ARG(QVariant, raw_data), Q_ARG(uint32_t, 0), Q_ARG(uint32_t, user_id) );
    }
    else
    {
        qCWarning(Service::Log).nospace() << user_id << "| item for write not found. item_id: " << item_id;
    }
}

void Worker::write_to_item_file(const QString& file_name)
{
    if (DeviceItem* item = prj->ptr()->itemById( last_file_item_and_user_id_.first ))
    {
        QMetaObject::invokeMethod(item->group(), "writeToControl", Qt::QueuedConnection,
                                  Q_ARG(DeviceItem*, item), Q_ARG(QVariant, file_name), Q_ARG(uint32_t, 0), Q_ARG(uint32_t, last_file_item_and_user_id_.second));
    }
    else
    {
        qCWarning(Service::Log).nospace() << last_file_item_and_user_id_.second << "| item for write file not found. item_id: " << last_file_item_and_user_id_.first;
    }
}

bool Worker::setMode(uint32_t user_id, uint32_t mode_id, uint32_t group_id)
{
    bool res = db_mng->setMode(mode_id, group_id);
    if (res)
        QMetaObject::invokeMethod(prj->ptr(), "setMode", Qt::QueuedConnection, Q_ARG(uint32_t, user_id), Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id) );
    return res;
}

void Worker::setParamValues(uint32_t user_id, const ParamValuesPack &pack)
{
    QMetaObject::invokeMethod(prj->ptr(), "setParamValues", Qt::QueuedConnection, Q_ARG(uint32_t, user_id), Q_ARG(ParamValuesPack, pack));

    QString dbg_msg = "Params changed:";
    for (const ParamValueItem& item: pack)
    {
        db_mng->saveParamValue(item.first, item.second);
        dbg_msg += "\n " + QString::number(item.first) + ": \"" + item.second.left(16) + "\"";
    }

    Log_Event_Item event {0, user_id, QtDebugMsg, 0, Service::Log().categoryName(), dbg_msg};
    add_event_message(event);

    emit paramValuesChanged(user_id, pack);
}

QVariant db_get_group_status_item_id(Helpz::Database::Base* db, const QString& table_name, quint32 group_id, quint32 info_id)
{
    auto q = db->select({table_name, {"id"}}, QString("WHERE group_id = %1 AND status_id = %2").arg(group_id).arg(info_id));
    if (q.next())
    {
        return q.value(0);
    }
    return {};
}

void Worker::add_status(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t user_id)
{
    db_pending_thread_->add_query([=](Helpz::Database::Base* db)
    {
        auto table = Helpz::Database::db_table<Group_Status_Item>();

        QVariant id_value = db_get_group_status_item_id(db, table.name_, group_id, info_id);
        Group_Status_Item item{id_value.toUInt(), group_id, info_id, args};

        if (id_value.isValid())
        {
            if (db->update({table.name_, {"args"}}, {args.join(';')}, "id=" + id_value.toString()))
            {
                structure_sync_.send_status_update(user_id, item);
            }
        }
        else
        {
            table.field_names_.removeFirst(); // remove id
            if (db->insert(table, {group_id, info_id, args.join(';')}, &id_value))
            {
                item.set_id(id_value.toUInt());
                structure_sync_.send_status_insert(user_id, item);
            }
        }
    });
}

void Worker::remove_status(quint32 group_id, quint32 info_id, uint32_t user_id)
{
    db_pending_thread_->add_query([=](Helpz::Database::Base* db)
    {
        auto table_name = Helpz::Database::db_table_name<Group_Status_Item>();
        QVariant id_value = db_get_group_status_item_id(db, table_name, group_id, info_id);
        if (id_value.isValid())
        {
            if (db->del(table_name, QString("group_id = %1 AND status_id = %2").arg(group_id).arg(info_id)).numRowsAffected())
            {
                structure_sync_.send_status_delete(user_id, id_value.toUInt());
            }
        }
    });
}

void Worker::update_plugin_param_names(const QVector<Plugin_Type>& plugins)
{
    QByteArray data;
    QDataStream ds(&data, QIODevice::ReadWrite);
    ds << plugins << uint32_t(0) << uint32_t(0);
    ds.device()->seek(0);
    structure_sync_.process_modify_message(0, STRUCT_TYPE_CHECKER_TYPES, ds.device(), db_pending());

    for (Device* dev: prj->ptr()->devices())
    {
        for (const Plugin_Type& plugin: plugins)
        {
            if (plugin.id() == dev->checker_id())
            {
                dev->set_param_name_list(plugin.param_names_device());
                for (DeviceItem *item: dev->items())
                {
                    item->set_param_name_list(plugin.param_names_device_item());
                }
                break;
            }
        }
    }
}

void Worker::newValue(DeviceItem *item, uint32_t user_id)
{
    waited_item_values[item->id()] = std::make_pair(item->raw_value(), item->value());
    if (!item_values_timer.isActive())
        item_values_timer.start();

    Log_Value_Item pack_item{0, 0, user_id, item->id(), item->raw_value(), item->value()};

    bool immediately = prj->ptr()->item_type_mng_.save_algorithm(item->type_id()) == Item_Type::saSaveImmediately;
    if (immediately && !Database::Helper::save_log_changes(db_mng, pack_item))
    {
        qWarning(Service::Log).nospace() << user_id << "|Упущенное значение:" << item->toString() << item->value().toString();
        // TODO: Error event
    } else if (prj->ptr()->item_type_mng_.save_algorithm(item->type_id()) == Item_Type::saInvalid)
        qWarning(Service::Log).nospace() << user_id << "|Неправильный параметр сохранения: " << item->toString();

    emit change(pack_item, immediately);

    if (webSock_th)
    {
        QVector<Log_Value_Item> pack{pack_item};
        QMetaObject::invokeMethod(webSock_th->ptr(), "sendDeviceItemValues", Qt::QueuedConnection,
                                  Q_ARG(Project_Info, websock_item.get()), Q_ARG(QVector<Log_Value_Item>, pack));
    }
}

} // namespace Dai
