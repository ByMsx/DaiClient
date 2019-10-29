
#include <QDebug>
#include <QDir>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimer>

#include <botan-2/botan/parsing.h>

#include <Helpz/consolereader.h>
#include <Helpz/dtls_client.h>

#include <Dai/commands.h>
#include <Dai/checkerinterface.h>
#include <Dai/db/group_status_item.h>
#include <Dai/db/view.h>
#include <plus/dai/jwt_helper.h>
#include "dbus_object.h"

#include "websocket_item.h"
#include "worker.h"

namespace Dai {

namespace Z = Helpz;
using namespace std::placeholders;

Worker::Worker(QObject *parent) :
    QObject(parent),
    project_thread_(nullptr), prj_(nullptr),
    restart_timer_started_(false),
    dbus_(nullptr)
{
    qRegisterMetaType<uint32_t>("uint32_t");

    auto s = settings();

    init_logging(s.get());
    init_database(s.get());
    init_project(s.get()); // инициализация структуры проекта
    init_checker(s.get()); // запуск потока опроса устройств
    init_network_client(s.get()); // подключение к серверу
    init_log_timer(); // сохранение статуса устройства по таймеру

    // используется для подключения к Orange на прямую
    init_websocket_manager(s.get());
    init_dbus(s.get());

    emit started();

//    QTimer::singleShot(15000, thread(), SLOT(quit())); // check delete error
}

template<typename T>
void stop_thread(T** thread_ptr, unsigned long wait_time = 15000)
{
    if (*thread_ptr)
    {
        QThread* thread = *thread_ptr;
        thread->quit();
        if (!thread->wait(wait_time))
        {
            thread->terminate();
        }
        delete *thread_ptr;
        *thread_ptr = nullptr;
    }
}

Worker::~Worker()
{
    blockSignals(true);
    QObject::disconnect(this, 0, 0, 0);

    stop_thread(&log_timer_thread_, 30000);

    websock_item.reset();
    stop_thread(&websock_th_);

    net_protocol_thread_.quit(); net_protocol_thread_.wait();
    net_thread_.reset();
    stop_thread(&checker_th_);
    if (!project_thread_ && prj_)
    {
        delete prj_;
    }
    else
    {
        stop_thread(&project_thread_);
    }

    if (db_mng_)
        delete db_mng_;
    db_pending_thread_.reset();
    delete dbus_;
}

Database::Helper* Worker::database() const { return db_mng_; }
Helpz::Database::Thread* Worker::db_pending() { return db_pending_thread_.get(); }
const DB_Connection_Info &Worker::database_info() const { return *db_info_; }

/*static*/ std::unique_ptr<QSettings> Worker::settings()
{
    QString configFileName = QCoreApplication::applicationDirPath() + QDir::separator() + QCoreApplication::applicationName() + ".conf";
    return std::unique_ptr<QSettings>(new QSettings(configFileName, QSettings::NativeFormat));
}

std::shared_ptr<Ver_2_2::Client::Protocol> Worker::net_protocol()
{
    auto client = net_thread_->client();
    if (client)
        return std::static_pointer_cast<Ver_2_2::Client::Protocol>(client->protocol());
    return {};
}

/*static*/ void Worker::store_connection_id(const QUuid &connection_id)
{
    std::shared_ptr<QSettings> s = settings();
    s->beginGroup("RemoteServer");
    s->setValue("Device", connection_id);
    s->endGroup();
}

void Worker::init_logging(QSettings *s)
{
    std::tuple<bool, bool> t = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Z::Param<bool>,Z::Param<bool>>
        #endif
            (
            s, "Log",
            Z::Param<bool>{"Debug", false},
            Z::Param<bool>{"Syslog", true}
    )();

    logg().set_debug(std::get<0>(t));
#ifdef Q_OS_UNIX
    logg().set_syslog(std::get<1>(t));
#endif
    QMetaObject::invokeMethod(&logg(), "init", Qt::QueuedConnection);
}

void Worker::init_database(QSettings* s)
{
    db_info_ = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Z::Param<QString>,Z::Param<QString>,Z::Param<QString>,Z::Param<QString>,Z::Param<QString>,Z::Param<int>,Z::Param<QString>,Z::Param<QString>>
        #endif
            (
                s, "Database",
                Z::Param<QString>{"CommonName", QString()},
                Z::Param<QString>{"Name", "deviceaccess_local"},
                Z::Param<QString>{"User", "DaiUser"},
                Z::Param<QString>{"Password", ""},
                Z::Param<QString>{"Host", "localhost"},
                Z::Param<int>{"Port", -1},
                Z::Param<QString>{"Driver", "QMYSQL"},
                Z::Param<QString>{"ConnectOptions", QString()}
    ).unique_ptr<DB_Connection_Info>();
    if (!db_info_)
        throw std::runtime_error("Failed get database config");

    db_pending_thread_.reset(new Helpz::Database::Thread{Helpz::Database::Connection_Info(*db_info_)});

    qRegisterMetaType<QVector<Group_Status_Item>>("QVector<Group_Status_Item>");
    qRegisterMetaType<QVector<View>>("QVector<View>");
    qRegisterMetaType<QVector<View_Item>>("QVector<View_Item>");
    db_mng_ = new Database::Helper(*db_info_, "Worker_" + QString::number((quintptr)this));
    connect(this, &Worker::status_added, this, &Worker::add_status, Qt::QueuedConnection);
    connect(this, &Worker::status_removed, this, &Worker::remove_status, Qt::QueuedConnection);

    QString sql = "DELETE hle FROM %1.house_list_employee hle LEFT JOIN %1.auth_user au ON hle.user_id = au.id WHERE au.id IS NULL;";
    sql = sql.arg(db_info_->common_db_name());
    db_pending_thread_->add_pending_query(std::move(sql), std::vector<QVariantList>());
}

void Worker::init_project(QSettings* s)
{
    Helpz::ConsoleReader* cr = nullptr;
    if (Service::instance().isImmediately())
    {
        cr = new Helpz::ConsoleReader(this);

#ifdef QT_DEBUG
        if (qApp->arguments().indexOf("-debugger") != -1)
        {
            auto server_conf = Helpz::SettingsHelper{
                                s, "Server",
                                Z::Param<QString>{"SSHHost", "80.89.129.98"},
                                Z::Param<bool>{"AllowShell", false}
                            }();
            prj_ = new Scripted_Project(this, cr, std::get<0>(server_conf), std::get<1>(server_conf));
        }
#endif
    }

    if (!prj_)
    {
        project_thread_ = Scripts_Thread()(s, "Server", this,
                              cr,
                              Z::Param<QString>{"SSHHost", "80.89.129.98"},
                              Z::Param<bool>{"AllowShell", false}
                              );
        project_thread_->start(QThread::HighPriority);
    }
}

void Worker::init_checker(QSettings* s)
{
    qRegisterMetaType<Device*>("Device*");

    checker_th_ = Checker_Thread()(s, "Checker", this, Z::Param<QStringList>{"Plugins", QStringList{"ModbusPlugin","WiringPiPlugin"}} );
    checker_th_->start();
}

void Worker::init_network_client(QSettings* s)
{
    net_protocol_thread_.start();
    structure_sync_.reset(new Ver_2_2::Client::Structure_Synchronizer{ db_pending_thread_.get() });
    connect(structure_sync_.get(), &Ver_2_2::Client::Structure_Synchronizer::client_modified, this, &Worker::restart_service_object, Qt::QueuedConnection);
    structure_sync_->moveToThread(&net_protocol_thread_);
    structure_sync_->set_project(prj());

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

#define DAI_PROTOCOL_LATEST "dai/2.1"

    const QString default_dir = qApp->applicationDirPath() + '/';
    auto [ tls_policy_file, host, port, protocols, recpnnect_interval_sec ]
            = Helpz::SettingsHelper{
                s, "RemoteServer",
                Z::Param<QString>{"TlsPolicyFile", default_dir + "tls_policy.conf"},
                Z::Param<QString>{"Host",               "deviceaccess.ru"},
                Z::Param<QString>{"Port",               "25588"},
                DAI_PROTOCOL_LATEST",dai/2.0", // Z::Param<QString>{"Protocols",          "dai/2.0,dai/1.1"},
                Z::Param<uint32_t>{"ReconnectSeconds",       15}
            }();

    Helpz::DTLS::Create_Client_Protocol_Func_T func = [this, auth_info](const std::string& app_protocol) -> std::shared_ptr<Helpz::Network::Protocol>
    {
        if (app_protocol != DAI_PROTOCOL_LATEST)
        {
            qCritical(Service::Log) << "Server doesn't support protocol:" << DAI_PROTOCOL_LATEST << "server want:" << app_protocol.c_str();
        }

        std::shared_ptr<Ver_2_2::Client::Protocol> ptr(new Ver_2_2::Client::Protocol{this, structure_sync_.get(), auth_info});
        QMetaObject::invokeMethod(structure_sync_.get(), "set_protocol", Qt::BlockingQueuedConnection,
                                  Q_ARG(std::shared_ptr<Ver_2_2::Client::Protocol_Base>, ptr));
        return std::static_pointer_cast<Helpz::Network::Protocol>(ptr);
    };

    Helpz::DTLS::Client_Thread_Config conf{ tls_policy_file.toStdString(), host.toStdString(), port.toStdString(),
                Botan::split_on(protocols.toStdString(), ','), recpnnect_interval_sec };
    conf.set_create_protocol_func(std::move(func));

    net_thread_.reset(new Helpz::DTLS::Client_Thread{std::move(conf)});
}

void Worker::init_log_timer()
{
    log_timer_thread_ = new Log_Value_Save_Timer_Thread(prj(), this);
    log_timer_thread_->start();
    connect(log_timer_thread_->ptr(), &Log_Value_Save_Timer::change, this, &Worker::change, Qt::QueuedConnection);
}

void Worker::init_websocket_manager(QSettings *s)
{
    std::tuple<bool, QByteArray> en_t = Helpz::SettingsHelper<Helpz::Param<bool>, Helpz::Param<QByteArray>
            >(s, "WebSocket",
              Helpz::Param<bool>{"Enabled", true},
              Helpz::Param<QByteArray>{"SecretKey", QByteArray()}
              )();
    if (!std::get<0>(en_t))
        return;

    QByteArray secret_key = std::get<1>(en_t);
    std::shared_ptr<JWT_Helper> jwt_helper = std::make_shared<JWT_Helper>(std::string{secret_key.constData(), static_cast<std::size_t>(secret_key.size())});

    websock_th_ = Websocket_Thread()(
                s, "WebSocket",
                jwt_helper,
                Helpz::Param<quint16>{"Port", 25589},
                Helpz::Param<QString>{"CertPath", QString()},
                Helpz::Param<QString>{"KeyPath", QString()});
    websock_th_->start();

    websock_item.reset(new Websocket_Item(this));
    connect(websock_th_->ptr(), &Network::WebSocket::through_command,
            websock_item.get(), &Websocket_Item::proc_command, Qt::BlockingQueuedConnection);
    connect(websock_item.get(), &Websocket_Item::send, websock_th_->ptr(), &Network::WebSocket::send, Qt::QueuedConnection);
}

void Worker::restart_service_object(uint32_t user_id)
{
    if (restart_timer_started_)
    {
        return;
    }

    restart_timer_started_ = true;

    if (prj())
    {
        if (!stop_scripts(user_id))
        {
            QTimer::singleShot(3000, [this, user_id]()
            {
                restart_timer_started_ = false;
                restart_service_object(user_id);
            });
            return;
        }
    }

    Log_Event_Item event { QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), user_id, false, QtInfoMsg, Service::Log().categoryName(), "The service restarts."};
    add_event_message(std::move(event));
    QTimer::singleShot(50, this, SIGNAL(serviceRestart()));
}

bool Worker::stop_scripts(uint32_t user_id)
{
    bool is_stoped = true;
    QMetaObject::invokeMethod(prj(), "stop", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, is_stoped), Q_ARG(uint32_t, user_id));
    return is_stoped;
}

void Worker::logMessage(QtMsgType type, const Helpz::LogContext &ctx, const QString &str)
{
    if (ctx.category().startsWith("net"))
    {
        return;
    }

    Log_Event_Item event{ QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), 0, false, type, ctx.category(), str };

    static QRegularExpression re("^(\\d+)\\|");
    QRegularExpressionMatch match = re.match(str);
    if (match.hasMatch())
    {
        event.set_user_id(match.captured(1).toUInt());
        event.set_text(str.right(str.size() - (match.capturedEnd(1) + 1)));
    }

    add_event_message(std::move(event));
}

void Worker::add_event_message(Log_Event_Item event)
{
    QMetaObject::invokeMethod(log_timer_thread_->ptr(), "add_log_event_item", Qt::QueuedConnection, Q_ARG(Log_Event_Item, event));

    if (websock_item)
    {
        websock_item->send_event_message(event);
    }
}

void Worker::processCommands(const QStringList &args)
{
    QList<QCommandLineOption> opt{
        { { "c", "console" }, QCoreApplication::translate("main", "Execute command in JS console."), "script" },
        { { "s", "stop_scripts"}, QCoreApplication::translate("main", "Stop scripts")},

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

    if (opt.size() > 0 && parser.isSet(opt.at(0)))
    {
        QMetaObject::invokeMethod(prj(), "console", Qt::QueuedConnection, Q_ARG(uint32_t, 0), Q_ARG(QString, parser.value(opt.at(0))));
    }
    else if (opt.size() > 1 && parser.isSet(opt.at(1)))
    {
        stop_scripts();
    }
    else
        qCInfo(Service::Log) << args << parser.helpText();
}

QString Worker::get_user_devices()
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

QString Worker::get_user_status()
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

void Worker::init_device(const QString &device, const QString &device_name, const QString &device_latin, const QString &device_desc)
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

void Worker::clear_server_config()
{
    save_server_data(QUuid(), QString(), QString());
}

void Worker::save_server_auth_data(const QString &login, const QString &password)
{
    auto proto = net_protocol();
    if (proto)
    {
        save_server_data(proto->auth_info().device_id(), login, password);
    }
}

void Worker::save_server_data(const QUuid &devive_uuid, const QString &login, const QString &password)
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

bool Worker::set_day_time(uint section_id, uint dayStartSecs, uint dayEndSecs)
{
    bool res = false;
    if (Section* sct = prj()->section_by_id(section_id))
    {
        TimeRange tempRange( dayStartSecs, dayEndSecs );
        if (*sct->day_time() != tempRange)
        {
            res = db_mng_->update({Helpz::Database::db_table_name<Section>(), {}, {"dayStart", "dayEnd"}},
                {tempRange.start(), tempRange.end()}, "id=" + QString::number(section_id));
            if (res)
            {
                *sct->day_time() = tempRange;
                prj()->day_time_changed(/*sct*/);
            }
        }
    }
    return res;
}

void Worker::write_to_item(uint32_t user_id, uint32_t item_id, const QVariant &raw_data)
{
    if (DeviceItem* item = prj()->item_by_id(item_id))
    {
        if (item->register_type() == Item_Type::rtFile)
        {
            last_file_item_and_user_id_ = std::make_pair(item_id, user_id);
        }
        QMetaObject::invokeMethod(item->group(), "write_to_control", Qt::QueuedConnection,
                                  Q_ARG(DeviceItem*, item), Q_ARG(QVariant, raw_data), Q_ARG(uint32_t, 0), Q_ARG(uint32_t, user_id) );
    }
    else
    {
        qCWarning(Service::Log).nospace() << user_id << "| item for write not found. item_id: " << item_id;
    }
}

void Worker::write_to_item_file(const QString& file_name)
{
    if (DeviceItem* item = prj()->item_by_id(last_file_item_and_user_id_.first))
    {
        qCDebug(Service::Log) << "write_to_item_file" << file_name;
        QMetaObject::invokeMethod(item->group(), "write_to_control", Qt::QueuedConnection,
                                  Q_ARG(DeviceItem*, item), Q_ARG(QVariant, file_name), Q_ARG(uint32_t, 0), Q_ARG(uint32_t, last_file_item_and_user_id_.second));
    }
    else
    {
        qCWarning(Service::Log).nospace() << last_file_item_and_user_id_.second << "| item for write file not found. item_id: " << last_file_item_and_user_id_.first;
    }
}

bool Worker::set_mode(uint32_t user_id, uint32_t mode_id, uint32_t group_id)
{
    bool res = db_mng_->set_mode(mode_id, group_id);
    if (res)
        QMetaObject::invokeMethod(prj(), "set_mode", Qt::QueuedConnection, Q_ARG(uint32_t, user_id), Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id) );
    return res;
}

void Worker::set_group_param_values(uint32_t user_id, const QVector<Group_Param_Value> &pack)
{
    QMetaObject::invokeMethod(prj(), "set_group_param_values", Qt::QueuedConnection, Q_ARG(uint32_t, user_id), Q_ARG(QVector<Group_Param_Value>, pack));

    QString dbg_msg = "Params changed:";
    for (const Group_Param_Value& item: pack)
    {
        db_mng_->save_group_param_value(item.group_param_id(), item.value());
        dbg_msg += "\n " + QString::number(item.group_param_id()) + ": \"" + item.value().left(16) + "\"";
    }

    Log_Event_Item event { QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), user_id, false, QtDebugMsg, Service::Log().categoryName(), dbg_msg};
    add_event_message(std::move(event));

    emit group_param_values_changed(user_id, pack);
}

QVariant db_get_group_status_item_id(Helpz::Database::Base* db, const QString& table_name, quint32 group_id, quint32 info_id)
{
    auto q = db->select({table_name, {}, {"id"}}, QString("WHERE group_id = %1 AND status_id = %2").arg(group_id).arg(info_id));
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

        QVariant id_value = db_get_group_status_item_id(db, table.name(), group_id, info_id);
        Group_Status_Item item{id_value.toUInt(), group_id, info_id, args};

        if (id_value.isValid())
        {
            if (db->update({table.name(), {}, {"args"}}, {args.join(';')}, "id=" + id_value.toString()))
            {
                structure_sync_->send_status_update(user_id, item);
            }
        }
        else
        {
            table.field_names().removeFirst(); // remove id
            if (db->insert(table, {group_id, info_id, args.join(';')}, &id_value))
            {
                item.set_id(id_value.toUInt());
                structure_sync_->send_status_insert(user_id, item);
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
                structure_sync_->send_status_delete(user_id, id_value.toUInt());
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
    structure_sync_->process_modify_message(0, STRUCT_TYPE_CHECKER_TYPES, ds.device());

    for (Device* dev: prj()->devices())
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

void Worker::new_value(const Log_Value_Item& log_value_item)
{
    emit change(log_value_item, log_value_item.need_to_save());

    if (websock_th_)
    {
        QMetaObject::invokeMethod(websock_th_->ptr(), "sendDeviceItemValues", Qt::QueuedConnection,
                                  Q_ARG(Project_Info, websock_item.get()), Q_ARG(QVector<Log_Value_Item>, QVector<Log_Value_Item>{log_value_item}));
    }
}

void Worker::connection_state_changed(DeviceItem *item, bool value)
{
    Log_Value_Item log_value_item{ QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), 0, false, item->id()};

    if (value)
    {
        log_value_item.set_raw_value(item->raw_value());
        log_value_item.set_value(item->value());
    }

    QMetaObject::invokeMethod(log_timer_thread_->ptr(), "add_log_value_item", Qt::QueuedConnection,
                              Q_ARG(Log_Value_Item, log_value_item));
}

Scripted_Project* Worker::prj()
{
    return project_thread_ ? project_thread_->ptr() : prj_;
}

void Worker::init_dbus(QSettings* s)
{
    dbus_ = Helpz::SettingsHelper(s, "DBus", this,
                Helpz::Param{"Service", DAI_DBUS_DEFAULT_SERVICE_CLIENT},
                Helpz::Param{"Object", DAI_DBUS_DEFAULT_OBJECT}
                ).ptr<Client::Dbus_Object>();
}

} // namespace Dai
