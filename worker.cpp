
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

#include "worker.h"

namespace Dai {

WebSockItem::WebSockItem(Worker *obj) :
    QObject(), Project_Info(),
    w(obj)
{
    set_id(1);
    set_teams({1});
    connect(w, &Worker::modeChanged, this, &WebSockItem::modeChanged, Qt::QueuedConnection);
    connect(this, &WebSockItem::applyStructModify, &w->structure_sync_, &Client::Structure_Synchronizer::modify, Qt::BlockingQueuedConnection);
}

WebSockItem::~WebSockItem() {
    disconnect(this);
}

void WebSockItem::send_event_message(const EventPackItem& event)
{
    QMetaObject::invokeMethod(w->webSock_th->ptr(), "sendEventMessage", Qt::QueuedConnection,
                              Q_ARG(Project_Info, this), Q_ARG(QVector<EventPackItem>, QVector<EventPackItem>{event}));
}

void WebSockItem::modeChanged(uint mode_id, uint group_id) {

    QMetaObject::invokeMethod(w->webSock_th->ptr(), "sendModeChanged", Qt::QueuedConnection,
                              Q_ARG(Project_Info, this), Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id));
}

void WebSockItem::procCommand(uint32_t user_id, quint32 user_team_id, quint32 proj_id, quint8 cmd, const QByteArray &raw_data)
{
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

        case wsRestart:             Helpz::apply_parse(ds, &Worker::restart_service_object, w); break;
        case wsWriteToDevItem:      Helpz::apply_parse(ds, &Worker::writeToItem, w); break;
        case wsChangeGroupMode:     Helpz::apply_parse(ds, &Worker::setMode, w); break;
        case wsChangeParamValues:   Helpz::apply_parse(ds, &Worker::setParamValues, w); break;
        case wsStructModify:        Helpz::apply_parse(ds, &WebSockItem::applyStructModify, this, ds.device()); break;
        case wsExecScript:          Helpz::apply_parse(ds, &ScriptedProject::console, w->prj->ptr()); break;

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

namespace Z = Helpz;
using namespace std::placeholders;

Worker::Worker(QObject *parent) :
    QObject(parent)
{
    auto s = settings();

    qRegisterMetaType<ValuePackItem>("ValuePackItem");
    qRegisterMetaType<EventPackItem>("EventPackItem");

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

Worker::~Worker()
{
    if (webSock_th)
        webSock_th->quit();
    django_th->quit();

    logTimer.stop();

    checker_th->ptr()->breakChecking();
    checker_th->quit();

    prj->quit();

    net_protocol_thread_.quit();
    net_thread_.reset();

    if (webSock_th && !webSock_th->wait(15000))
        webSock_th->terminate();
    if (!django_th->wait(15000))
        django_th->terminate();
    if (!checker_th->wait(15000))
        checker_th->terminate();
    if (!prj->wait(15000))
        prj->terminate();

    net_protocol_thread_.wait();

    if (webSock_th)
        delete webSock_th;
    delete django_th;
    delete checker_th;
    delete prj;

    delete db_mng;
}

DBManager* Worker::database() const { return db_mng; }
const Helpz::Database::Connection_Info& Worker::database_info() const { return *db_info_; }

std::unique_ptr<QSettings> Worker::settings()
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

    db_mng = new DBManager(*db_info_, "Worker_" + QString::number((quintptr)this));
    connect(this, &Worker::statusAdded, db_mng, &DBManager::addStatus, Qt::QueuedConnection);
    connect(this, &Worker::statusRemoved, db_mng, &DBManager::removeStatus, Qt::QueuedConnection);
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
//    while (!prj->ptr() && !prj->wait(5));
}

void Worker::init_Checker(QSettings* s)
{
    qRegisterMetaType<Device*>("Device*");

    checker_th = CheckerThread()(s, "Checker", this, Z::Param<int>{"Interval", 1500}, Z::Param<QString>{"Plugins", "ModbusPlugin,WiringPiPlugin"} );
    checker_th->start();
}

void Worker::init_network_client(QSettings* s)
{
    net_protocol_thread_.start();
    structure_sync_.moveToThread(&net_protocol_thread_);
    structure_sync_.set_project(prj->ptr());

    qRegisterMetaType<ValuePackItem>("ValuePackItem");
    qRegisterMetaType<QVector<ValuePackItem>>("QVector<ValuePackItem>");
    qRegisterMetaType<QVector<EventPackItem>>("QVector<EventPackItem>");
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

    Helpz::DTLS::Create_Client_Protocol_Func_T func = [this, auth_info](const std::string& app_protocol) -> std::shared_ptr<Helpz::Network::Protocol>
    {
        return std::shared_ptr<Helpz::Network::Protocol>(new Client::Protocol_2_0{this, &structure_sync_, auth_info});
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
        std::map<quint32, std::pair<QVariant, QVariant>> values = std::move(waited_item_values);
        for (auto it: values)
            if (!db_mng->setDevItemValue(it.first, it.second.first, it.second.second))
            {
                // TODO: Do something
            }
    });
    item_values_timer.setInterval(5000);
    item_values_timer.setSingleShot(true);
    item_values_timer.start();

    connect(&logTimer, &QTimer::timeout, [this, period]()
    {
        if (logTimer.interval() != period * 1000)
            logTimer.setInterval(  period * 1000 );

        ValuePackItem pack_item;
        pack_item.user_id = 0;
        {
            QDateTime cur_date = QDateTime::currentDateTime().toUTC();
            cur_date.setTime(QTime(cur_date.time().hour(), cur_date.time().minute(), 0));
            pack_item.time_msecs = cur_date.toMSecsSinceEpoch();
        }

        ItemTypeManager* typeMng = &prj->ptr()->ItemTypeMng;

        static std::map<quint32, QVariant> cachedValues;

        for (Device* dev: prj->ptr()->devices())
            for (DeviceItem* dev_item: dev->items())
            {
                if (typeMng->saveAlgorithm(dev_item->type()) != ItemType::saSaveByTimer)
                    continue;

                auto val_it = cachedValues.find(dev_item->id());

                if (val_it == cachedValues.cend())
                    cachedValues.emplace(dev_item->id(), dev_item->getRawValue());
                else if (val_it->second != dev_item->getRawValue())
                    val_it->second = dev_item->getRawValue();
                else
                    continue;

                pack_item.db_id = 0;
                pack_item.item_id = dev_item->id();
                pack_item.raw_value = dev_item->getRawValue();
                pack_item.display_value = dev_item->getValue();

                if (db_mng->logChanges(pack_item))
                {
                    auto proto = net_protocol();
                    if (proto)
                    {
                        QMetaObject::invokeMethod(proto.get(), "change", Qt::QueuedConnection,
                                              Q_ARG(ValuePackItem, pack_item), Q_ARG(bool, false));
                    }
                }
                else
                {
                    // TODO: Error event
                    qCWarning(Service::Log) << "Failed change log with device item" << dev_item->id() << dev_item->getValue();
                }
            }
    });

    const auto cur_time = QDateTime::currentDateTime();
    const uint need2add = period - (cur_time.toTime_t() % period);
    uint interval = cur_time.secsTo(cur_time.addSecs(need2add));

    logTimer.setInterval(interval * 1000);
    logTimer.setTimerType(Qt::VeryCoarseTimer);
    logTimer.start();
}

void Worker::initDjango(QSettings *s)
{
    django_th = DjangoThread()(s, "Django",
                             Helpz::Param<QString>{"manage", "/var/www/dai/manage.py"});
    django_th->start();
    while (!django_th->ptr() && !django_th->wait(5));
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

    while (!webSock_th->ptr() && !webSock_th->wait(5));
    connect(webSock_th->ptr(), &Network::WebSocket::checkAuth,
                     django_th->ptr(), &DjangoHelper::checkToken, Qt::BlockingQueuedConnection);


    websock_item.reset(new WebSockItem(this));
    connect(this, &Worker::event_message, websock_item.get(), &WebSockItem::send_event_message, Qt::DirectConnection);
    connect(webSock_th->ptr(), &Network::WebSocket::throughCommand,
            websock_item.get(), &WebSockItem::procCommand, Qt::BlockingQueuedConnection);
    connect(websock_item.get(), &WebSockItem::send, webSock_th->ptr(), &Network::WebSocket::send, Qt::QueuedConnection);
//    webSock_th->ptr()->get_proj_in_team_by_id.connect(
    //                std::bind(&Worker::proj_in_team_by_id, this, std::placeholders::_1, std::placeholders::_2));
}

void Worker::restart_service_object(uint32_t user_id)
{
    EventPackItem event {0, user_id, QtInfoMsg, 0, Service::Log().categoryName(), "The service restarts."};
    add_event_message(event);
    QTimer::singleShot(50, this, SIGNAL(serviceRestart()));
}

//std::shared_ptr<dai::project::base> Worker::proj_in_team_by_id(uint32_t team_id, uint32_t proj_id) {
    // TODO: Check team_id valid
//    return std::static_pointer_cast<dai::project::base>(websock_item);
//}

void Worker::logMessage(QtMsgType type, const Helpz::LogContext &ctx, const QString &str)
{
    if (qstrcmp(ctx->category, Helpz::Network::DetailLog().categoryName()) == 0 ||
            qstrncmp(ctx->category, "net", 3) == 0)
    {
        return;
    }
    EventPackItem event{0, 0, type, 0, ctx->category, str};

    static QRegularExpression re("^(\\d+)\\|");
    QRegularExpressionMatch match = re.match(str);
    if (match.hasMatch())
    {
        event.user_id = match.captured(1).toUInt();
        event.text = str.right(str.size() - (match.capturedEnd(1) + 1));
    }

    add_event_message(event);
}

void Worker::add_event_message(const EventPackItem& event)
{
    if (db_mng->eventLog(const_cast<EventPackItem&>(event)))
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

QByteArray Worker::sections()
{
    while (!prj->ptr() && !prj->wait(5));

    QByteArray buff;
    {
        QDataStream ds(&buff, QIODevice::WriteOnly);
        ds.setVersion(QDataStream::Qt_5_7);
        prj->ptr()->dumpInfoToStream(&ds);
    }
    return buff;

//    std::shared_ptr<Prt::ServerInfo> info = prj->ptr()->dumpInfoToStream();
//    return serialize( info.get() );
}

bool Worker::setDayTime(uint section_id, uint dayStartSecs, uint dayEndSecs)
{
    bool res = false;
    if (Section* sct = prj->ptr()->sectionById( section_id ))
    {
        TimeRange tempRange( dayStartSecs, dayEndSecs );
        if (*sct->dayTime() != tempRange)
            if (res = db_mng->setDayTime( section_id, tempRange ), res)
            {
                *sct->dayTime() = tempRange;
                prj->ptr()->dayTimeChanged(/*sct*/);
            }
    }
    return res;
}

void Worker::writeToItem(uint32_t user_id, uint32_t item_id, const QVariant &raw_data)
{
    if (DeviceItem* item = prj->ptr()->itemById( item_id ))
        QMetaObject::invokeMethod(item->group(), "writeToControl", Qt::QueuedConnection,
                                  Q_ARG(DeviceItem*, item), Q_ARG(QVariant, raw_data), Q_ARG(uint32_t, 0), Q_ARG(uint32_t, user_id) );
}

bool Worker::setMode(uint32_t user_id, uint32_t mode_id, uint32_t group_id)
{
    bool res = db_mng->setMode(mode_id, group_id);
    if (res)
        QMetaObject::invokeMethod(prj->ptr(), "setMode", Qt::QueuedConnection, Q_ARG(uint32_t, user_id), Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id) );
    return res;
}

struct ServiceRestarter {
    ServiceRestarter(Worker* worker) : w(worker) {}
    Worker* w;
    bool isOk = true;
    ~ServiceRestarter() {
        if (isOk)
            QTimer::singleShot(500, w, SIGNAL(serviceRestart()));
    }
};

bool Worker::setCode(const CodeItem& item)
{
    if (!item.id()) {
        qCWarning(Service::Log) << "Attempt to save zero code";
        return false;
    }

    CodeManager& CodeMng = prj->ptr()->CodeMng;
    CodeItem* code = CodeMng.getType(item.id());

    qDebug() << "SetCode" << item.id() << item.text.length() << code->id();
    if (code->id())
        *code = item;
    else
        CodeMng.add(item);
    return db_mng->setCodes(&CodeMng);
}

void Worker::setParamValues(uint32_t user_id, const ParamValuesPack &pack)
{
    QMetaObject::invokeMethod(prj->ptr(), "setParamValues", Qt::QueuedConnection, Q_ARG(uint32_t, user_id), Q_ARG(ParamValuesPack, pack));

    QString dbg_msg = "Params changed:";
    for (const ParamValueItem& item: pack)
    {
        db_mng->saveParamValue(item.first, item.second);
        dbg_msg += "\n " + QString::number(item.first) + ": \"" + item.second + "\"";
    }

    EventPackItem event {0, user_id, QtDebugMsg, 0, Service::Log().categoryName(), dbg_msg};
    add_event_message(event);

    emit paramValuesChanged(user_id, pack);
}

void Worker::newValue(DeviceItem *item, uint32_t user_id)
{
    waited_item_values[item->id()] = std::make_pair(item->getRawValue(), item->getValue());
    if (!item_values_timer.isActive())
        item_values_timer.start();

    ValuePackItem pack_item{0, user_id, item->id(), 0, item->getRawValue(), item->getValue()};

    bool immediately = prj->ptr()->ItemTypeMng.saveAlgorithm(item->type()) == ItemType::saSaveImmediately;
    if (immediately && !db_mng->logChanges(pack_item))
    {
        qWarning(Service::Log).nospace() << user_id << "|Упущенное значение:" << item->toString() << item->getValue().toString();
        // TODO: Error event
    } else if (prj->ptr()->ItemTypeMng.saveAlgorithm(item->type()) == ItemType::saInvalid)
        qWarning(Service::Log).nospace() << user_id << "|Неправильный параметр сохранения: " << item->toString();

    emit change(pack_item, immediately);

    if (webSock_th) {
        QVector<ValuePackItem> pack{pack_item};
        QMetaObject::invokeMethod(webSock_th->ptr(), "sendDeviceItemValues", Qt::QueuedConnection,
                                  Q_ARG(Project_Info, websock_item.get()), Q_ARG(QVector<ValuePackItem>, pack));
    }
}

/*void Worker::sendLostValues(const QVector<quint32> &ids)
{
    QVector<ValuePackItem> pack;

    QVector<quint32> found, not_found;
    db_mng->getListValues(ids, found, pack);

    QMetaObject::invokeMethod(g_mng_th->ptr(), "sendLostValues", Qt::QueuedConnection, Q_ARG(QVector<ValuePackItem>, pack));

    std::set_difference(
        ids.cbegin(), ids.cend(),
        found.cbegin(), found.cend(),
        std::back_inserter( not_found )
    );

    if (not_found.size())
        QMetaObject::invokeMethod(g_mng_th->ptr(), "sendNotFoundIds", Qt::QueuedConnection, Q_ARG(const QVector<quint32>&, not_found));
}*/

} // namespace Dai
