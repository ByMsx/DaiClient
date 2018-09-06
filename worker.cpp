
#include <dlfcn.h>

#include <QDebug>
#include <QSettings>

#include <Helpz/consolereader.h>
#include <Dai/checkerinterface.h>

#include "dai_adaptor.h"
#include "DBus/d_iface.h"

#include "worker.h"

namespace dai {

WebSockItem::WebSockItem(Dai::Worker *obj) : w(obj) {
    connect(w, &Dai::Worker::modeChanged, this, &WebSockItem::modeChanged, Qt::QueuedConnection);
}

WebSockItem::~WebSockItem() {
    disconnect(this);
}

void WebSockItem::send_value(quint32 item_id, const QVariant &raw_data) {
    w->writeToItem(item_id, raw_data);
}

void WebSockItem::send_mode(quint32 mode_id, quint32 group_id) {
    w->setMode(mode_id, group_id);
}

void WebSockItem::send_param_values(const QByteArray &msg_buff)
{
    QDataStream msg(msg_buff);
    Helpz::applyParse(&Dai::Worker::setParamValues, w, msg);
}

void WebSockItem::send_code(quint32 code_id, const QString &text) {
    qCritical() << "Attempt to set code from local:" << code_id << text.left(32);
}

void WebSockItem::send_script(const QString &script) {
    qCritical() << "Attempt to exec script from local:" << script.left(32);
}

void WebSockItem::send_restart() { w->serviceRestart(); }

void WebSockItem::modeChanged(uint mode_id, uint group_id) {
    QMetaObject::invokeMethod(Dai::Worker::webSock, "sendModeChanged", Qt::QueuedConnection,
                              Q_ARG(ProjInfo, this), Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id));
}

} // namespace dai

namespace Dai {

namespace Z = Helpz;
using namespace std::placeholders;

Worker::Worker(QObject *parent) :
    QObject(parent)
{
    auto s = settings();

    auto [log_debug, log_syslog, log_period] = Helpz::SettingsHelper(
            s.get(), "Log",
            Z::Param{"Debug", false},
        Z::Param{"Syslog", true},
        Z::Param{"Period", 60 * 30} // 30 минут
    )();

    logg().debug = log_debug;
#ifdef Q_OS_UNIX
    logg().syslog = log_syslog;
#endif
    QMetaObject::invokeMethod(&logg(), "init", Qt::QueuedConnection);

    init_Database(s.get());
    init_SectionManager(s.get());
    init_Checker(s.get());
    init_GlobalClient(s.get());
    init_LogTimer(log_period);

    try {
        initDjangoLib(s.get());
        initWebSocketManagerLib(s.get());
    }
    catch(const std::runtime_error& e ) {
        qCCritical(Service::Log) << e.what();
    }

    init_DBus(s.get());
    emit started();

//    QTimer::singleShot(15000, thread(), SLOT(quit())); // check delete error
}

Worker::~Worker()
{
    if (webSock)
    {
        webSock->get_proj_in_team_by_id.disconnect_all_slots();
        webSock = nullptr;
        dlclose(websock_handle);
        websock_handle = nullptr;
    }

    if (django) {
        django = nullptr;
        dlclose(django_handle);
        django_handle = nullptr;
    }


    logTimer.stop();

    checker_th->ptr()->breakChecking();
    checker_th->quit();

    g_mng_th->quit();
    sct_mng->quit();

    if (!g_mng_th->wait(15000))
        g_mng_th->terminate();
    if (!checker_th->wait(15000))
        checker_th->terminate();
    if (!sct_mng->wait(15000))
        sct_mng->terminate();

    delete g_mng_th;
    delete checker_th;
    delete sct_mng;

    delete db_mng;
}

DBManager* Worker::database() const { return db_mng; }

std::unique_ptr<QSettings> Worker::settings()
{
    QString configFileName = QCoreApplication::applicationDirPath() + QDir::separator() + QCoreApplication::applicationName() + ".conf";
    return std::unique_ptr<QSettings>(new QSettings(configFileName, QSettings::NativeFormat));
}

void Worker::init_DBus(QSettings* s)
{
    new ClientAdaptor( this );
    DBus::init(s, "ru.deviceaccess.Dai.Client", this);
}

void Worker::init_Database(QSettings* s)
{
    auto info = Helpz::SettingsHelper(
                s, "Database",
                Z::Param{"Name", "deviceaccess_local"},
                Z::Param{"User", "DaiUser"},
                Z::Param{"Password", ""},
                Z::Param{"Host", "localhost"},
                Z::Param{"Port", -1},
                Z::Param{"Driver", "QMYSQL"},
                Z::Param{"ConnectOptions", QString()}
    ).unique_ptr<Helpz::Database::ConnectionInfo>();

    db_mng = new DBManager(*info);
}

void Worker::init_SectionManager(QSettings* s)
{
    Helpz::ConsoleReader* cr = nullptr;
    if (Service::instance().isImmediately())
        cr = new Helpz::ConsoleReader(this);

//    qRegisterMetaType<std::shared_ptr<Dai::Prt::ServerInfo>>("std::shared_ptr<Dai::Prt::ServerInfo>");

    sct_mng = ScriptsThread()(s, "Server", this, cr, Z::Param{"SSHHost", "80.89.129.98"});
    sct_mng->start(QThread::HighPriority);
//    while (!sct_mng->ptr() && !sct_mng->wait(5));
}

void Worker::init_Checker(QSettings* s)
{
    qRegisterMetaType<Device*>("Device*");

    checker_th = CheckerThread()(s, "Checker", this, Z::Param{"Interval", 1500} );
    checker_th->start();
}

void Worker::init_GlobalClient(QSettings* s)
{
    qRegisterMetaType<QVector<Dai::ValuePackItem>>("QVector<Dai::ValuePackItem>");
    qRegisterMetaType<QVector<Dai::EventPackItem>>("QVector<Dai::EventPackItem>");
    qRegisterMetaType<QVector<quint32>>("QVector<quint32>");

    g_mng_th = NetworkClientThread()(
              s, "RemoteServer",
              this,
              Z::Param{"Host",                 "deviceaccess.ru"},
              Z::Param{"Port",                 (quint16)25588},
              Z::Param{"Login",                QString()},
              Z::Param{"Password",             QString()},
              Z::Param{"Device",               QUuid()},
              Z::Param{"CheckServerInterval",  15000}
            );

    g_mng_th->start();
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

        QDateTime cur_date = QDateTime::currentDateTime();
        cur_date.setTime(QTime(cur_date.time().hour(), cur_date.time().minute(), 0));

        QVariant db_id;

        ItemTypeManager* typeMng = &sct_mng->ptr()->ItemTypeMng;

        static std::map<quint32, QVariant> cachedValues;

        for (Device* dev: sct_mng->ptr()->devices())
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

                db_id.clear();
                if (db_mng->logChanges(dev_item, cur_date, &db_id))
                {
                    ValuePackItem packItem{ db_id.toUInt(), dev_item->id(), cur_date.toMSecsSinceEpoch(),
                                    dev_item->getRawValue(), dev_item->getValue() };
                    QMetaObject::invokeMethod(g_mng_th->ptr(), "change", Qt::QueuedConnection,
                                              Q_ARG(Dai::ValuePackItem, packItem), Q_ARG(bool, false));
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

/*static*/ dai::DjangoHelper* Worker::django = nullptr;
/*static*/ void* Worker::django_handle = nullptr;
void Worker::initDjangoLib(QSettings *s)
{
    django_handle = dlopen("./plugins/libdjango.so", RTLD_LAZY);
    if (!django_handle)
        throw std::runtime_error(dlerror());

    typedef dai::DjangoHelper* (*DjangoGetFunc)(QSettings *);

    DjangoGetFunc get_django = (DjangoGetFunc)dlsym(django_handle, "get_django");
    if (!get_django)
        throw std::runtime_error(dlerror());

    django = get_django(s);
}

/*static*/ dai::Network::WebSocket* Worker::webSock = nullptr;
/*static*/ void* Worker::websock_handle = nullptr;
void Worker::initWebSocketManagerLib(QSettings *s)
{
    websock_handle = dlopen("./plugins/libwebsocket.so", RTLD_LAZY);
    if (!websock_handle)
        throw std::runtime_error(dlerror());

    typedef dai::Network::WebSocket* (*WebSockGetFunc)(QSettings *, dai::DjangoHelper *);

    WebSockGetFunc get_websock = (WebSockGetFunc)dlsym(websock_handle, "get_websocket");
    if (!get_websock)
        throw std::runtime_error(dlerror());

    webSock = get_websock(s, django);
    websock_item.reset(new dai::WebSockItem(this));
    webSock->get_proj_in_team_by_id.connect(
                std::bind(&Worker::proj_in_team_by_id, this, std::placeholders::_1, std::placeholders::_2));
}

std::shared_ptr<dai::project::base> Worker::proj_in_team_by_id(uint32_t team_id, uint32_t proj_id) {
    // TODO: Check team_id valid
    return std::static_pointer_cast<dai::project::base>(websock_item);
}

void Worker::logMessage(QtMsgType type, const Helpz::LogContext &ctx, const QString &str)
{
    if (qstrcmp(ctx->category, Helpz::Network::DetailLog().categoryName()) == 0)
        return;

    QDateTime cur_date = QDateTime::currentDateTime();
    QVariant db_id;
    if (db_mng->eventLog(type, ctx->category, str, cur_date, &db_id)) {
        EventPackItem item{db_id.toUInt(), type, cur_date.toMSecsSinceEpoch(), ctx->category, str};
        QMetaObject::invokeMethod(g_mng_th->ptr(), "eventLog", Qt::QueuedConnection, Q_ARG(EventPackItem, item));
        if (webSock && websock_item)
            QMetaObject::invokeMethod(webSock, "sendEventMessage", Qt::QueuedConnection,
                                      QArgument<dai::project::info>("ProjInfo", websock_item.get()), Q_ARG(quint32, db_id.toUInt()), Q_ARG(quint32, item.type_id),
                                      Q_ARG(QString, item.category), Q_ARG(QString, item.text), Q_ARG(QDateTime, cur_date));
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
        QMetaObject::invokeMethod(sct_mng->ptr(), "console", Qt::QueuedConnection,
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
    QMetaObject::invokeMethod(g_mng_th->ptr(), "getUserDevices", Qt::BlockingQueuedConnection,
                              QReturnArgument<QVector<QPair<QUuid, QString>>>("QVector<QPair<QUuid, QString>>", devices));

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
    json["user"] = g_mng_th->ptr()->username();
    json["device"] = g_mng_th->ptr()->device().toString();
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

    Network::Client* c = g_mng_th->ptr();
    if (!devive_uuid.isNull())
        QMetaObject::invokeMethod(c, "importDevice", Qt::BlockingQueuedConnection, Q_ARG(QUuid, devive_uuid));

    QUuid new_uuid;
    if (!device_name.isEmpty())
        QMetaObject::invokeMethod(c, "createDevice", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QUuid, new_uuid),
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
    saveServerData(g_mng_th->ptr()->device(), login, password);
}

void Worker::saveServerData(const QUuid &devive_uuid, const QString &login, const QString &password)
{
    auto s = settings();
    s->beginGroup("RemoteServer");
    s->setValue("Device", devive_uuid.toString());
    s->setValue("Login", login);
    s->setValue("Password", password);
    s->endGroup();

    QMetaObject::invokeMethod(g_mng_th->ptr(), "refreshAuth", Qt::QueuedConnection,
                              Q_ARG(QUuid, devive_uuid), Q_ARG(QString, login), Q_ARG(QString, password));
}

QByteArray Worker::sections()
{
    while (!sct_mng->ptr() && !sct_mng->wait(5));

    QByteArray buff;
    {
        QDataStream ds(&buff, QIODevice::WriteOnly);
        ds.setVersion(QDataStream::Qt_5_7);
        sct_mng->ptr()->dumpInfoToStream(&ds);
    }
    return buff;

//    std::shared_ptr<Prt::ServerInfo> info = sct_mng->ptr()->dumpInfoToStream();
//    return serialize( info.get() );
}

bool Worker::setDayTime(uint section_id, uint dayStartSecs, uint dayEndSecs)
{
    bool res = false;
    if (Section* sct = sct_mng->ptr()->sectionById( section_id ))
    {
        TimeRange tempRange( dayStartSecs, dayEndSecs );
        if (*sct->dayTime() != tempRange)
            if (res = db_mng->setDayTime( section_id, tempRange ), res)
            {
                *sct->dayTime() = tempRange;
                sct_mng->ptr()->dayTimeChanged(/*sct*/);
            }
    }
    return res;
}

void Worker::setControlState(quint32 section_id, quint32 item_type, const Variant &raw_data)
{
    if (auto sct = sct_mng->ptr()->sectionById( section_id ))
        QMetaObject::invokeMethod(sct, "setControlState", Qt::QueuedConnection,
                                  Q_ARG(uint, item_type), Q_ARG(QVariant, raw_data.m_var) );
}

void Worker::writeToItem(quint32 item_id, const QVariant &raw_data)
{
    if (DeviceItem* item = sct_mng->ptr()->itemById( item_id ))
        QMetaObject::invokeMethod(item->group(), "writeToControl", Qt::QueuedConnection,
                                  Q_ARG(DeviceItem*, item), Q_ARG(QVariant, raw_data) );
}

bool Worker::setMode(uint mode_id, quint32 group_id)
{
    bool res = db_mng->setMode(mode_id, group_id);
    if (res)
        QMetaObject::invokeMethod(sct_mng->ptr(), "setMode", Qt::QueuedConnection, Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id) );
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

    CodeManager& CodeMng = sct_mng->ptr()->CodeMng;
    CodeItem* code = CodeMng.getType(item.id());

    qDebug() << "SetCode" << item.id() << item.text.length() << code->id();
    if (code->id())
        *code = item;
    else
        CodeMng.add(item);
    return db_mng->setCodes(&CodeMng);
}

void Worker::setParamValues(const ParamValuesPack &pack)
{
    qCDebug(Service::Log) << "setParamValues" << pack.size() << pack;

    ParamValuesPack params = pack;
    Param* p;

    for(Section* sct: sct_mng->ptr()->sections())
        for (ItemGroup* group: sct->groups())
        {
            params.erase(std::remove_if(params.begin(), params.end(), [&](const ParamValueItem& param)
            {
                if (p = group->params()->getById(param.first), p)
                {
                    p->setValueFromString(param.second);
                    db_mng->saveParamValue(param.first, p->valueToString());
                    return true;
                }
                return false;
            }), params.end());
        }

    if (params.size() != 0)
        qCWarning(Service::Log) << "Failed to set param values" << params;

    emit paramValuesChanged(pack);
}

template<typename T>
bool saveSettings(QDataStream& msg, Database* db_mng, bool (Database::*saveFunc)(T*))
{
    T manager; msg >> manager;
    if (msg.status() != QDataStream::Ok)
        return false;

    return (db_mng->*saveFunc)(&manager);
}

bool Worker::setSettings(quint16 cmd, QDataStream *msg)
{
    using namespace Network;
    qCDebug(Service::Log) << "setSettings" << (Cmd::Commands)cmd;

    ServiceRestarter restarter(this);

    try {
        switch ((Cmd::Commands)cmd) {
        case Cmd::SetSignTypes:
            if (saveSettings(*msg, db_mng, &Database::setSignTypes))
                return true;
            break;
        case Cmd::SetItemTypes:
            if (saveSettings(*msg, db_mng, &Database::setItemTypes))
                return true;
            break;
        case Cmd::SetGroupTypes:
            if (saveSettings(*msg, db_mng, &Database::setGroupTypes))
                return true;
            break;
        case Cmd::SetStatus:
            if (saveSettings(*msg, db_mng, &Database::setStatuses))
                return true;
            break;
        case Cmd::SetStatusTypes:
            if (saveSettings(*msg, db_mng, &Database::setStatusTypes))
                return true;
            break;
        case Cmd::SetParamTypes:
            if (saveSettings(*msg, db_mng, &Database::setParamTypes))
                return true;
            break;
        case Cmd::SetCodes:
            if (saveSettings(*msg, db_mng, &Database::setCodes))
                return true;
            break;
        case Cmd::SetParamValues2:
            if (Helpz::applyParse(&Database::setParamValues, db_mng, *msg))
                return true;
            break;
        case Cmd::SetGroups:
            if (Helpz::applyParse(&Database::setSimpleGroups, db_mng, *msg))
                return true;
            break;
        case Cmd::SetSections:
            if (Helpz::applyParse(&Database::setSimpleSections, db_mng, *msg))
                return true;
            break;
        case Cmd::SetDeviceItems:
            if (Helpz::applyParse(&Database::setSimpleDeviceItems, db_mng, *msg))
                return true;
            break;
        case Cmd::SetDevices:
            if (Helpz::applyParse(&Database::setSimpleDevices, db_mng, *msg))
                return true;
            break;

        default: break;
        }
    } catch(const std::exception& e) {
        qCritical() << "EXCEPTION: setSettings" << (Cmd::Commands)cmd << e.what();
    }
    restarter.isOk = false;
    return false;
}

/*bool Worker::setSettings(uchar stType, google::protobuf::Message *msg)
{
    qCDebug(Helpz::ServiceLog) << "setSettings" << (Network::SettingsType)stType;

    ServiceRestarter restarter(this);

    switch ((Network::SettingsType)stType) {
    case Network::stSignType:
        if (auto info = static_cast<Prt::TypeInfoList*>(msg))
        {
            SignManager signs;
            for (const Prt::TypeInfo& it: info->items())
                signs.add(it.id(), QString::fromStdString(it.name()));

            if (db_mng->setSignTypes(&signs))
            {
                sct_mng->ptr()->SignMng = signs;
                return true;
            }
            qCDebug(Helpz::ServiceLog) << "setSignTypes" << info->items_size();
        }
        break;
    case Network::stItemType:
        if (auto info = static_cast<Prt::TypesInfo*>(msg))
        {
            ItemTypeManager itemTypes;
            sct_mng->ptr()->fillItemTypes(&itemTypes, info);

            if (db_mng->setItemTypes(&itemTypes))
            {
                sct_mng->ptr()->ItemTypeMng = itemTypes;
                return true;
            }
        }
        break;
    case Network::stGroupType:
        if (auto info = static_cast<Prt::TypesInfo*>(msg))
        {
            ItemGroupTypeManager groupTypes;
            sct_mng->ptr()->fillGroupTypes(&groupTypes, info);

            if (db_mng->setGroupTypes(&groupTypes))
            {
                sct_mng->ptr()->GroupTypeMng = groupTypes;
                return true;
            }
        }
        break;
    case Network::stGroup:
        if (auto info = static_cast<Prt::GroupList*>(msg))
        {
            qCDebug(Helpz::ServiceLog) << "Try to set Groups";
            std::vector<const Prt::GroupObject*> groups;
            for (const Prt::GroupObject& group: info->items())
                groups.push_back(&group);
            if (db_mng->setGroups(groups))
            {
                qCDebug(Helpz::ServiceLog) << "Set Groups complite";
//                sct_mng->ptr()->GroupTypeMng = groupTypes;
                return true;
            }
        }
        break;
    case Network::stSection:
        if (auto info = static_cast<Prt::SectionList*>(msg))
        {
            Sections sctList;
            for(auto& sct: info->items())
                sctList.push_back(std::make_shared<Section>(sct, sct_mng->ptr()));
            if (db_mng->setSections([&sctList]() -> const Sections& { return sctList; }))
            {
//                sct_mng->ptr()->GroupTypeMng = groupTypes;
                return true;
            }
        }
        break;
    case Network::stDeviceItem:
        if (auto info = static_cast<Prt::DeviceItemList*>(msg))
        {
            std::vector<Prt::DeviceItem*> dev_items;
            for(auto& item: *info->mutable_items())
                dev_items.push_back(&item);

            if (db_mng->setDeviceItems(dev_items))
            {
//                sct_mng->ptr()->GroupTypeMng = groupTypes;
                return true;
            }
        }
        break;
    case Network::stDevice:
        if (auto info = static_cast<Prt::DeviceList*>(msg))
        {
            Devices devList;
            for(auto& dev: info->items())
                devList.push_back(std::make_shared<Device>(dev));

            if (db_mng->setDevices([&devList]() -> const Devices& { return devList; }))
            {
//                sct_mng->ptr()->GroupTypeMng = groupTypes;
                return true;
            }
        }
        break;

    case Network::stStatus:
        if (auto info = static_cast<Prt::TypesInfo*>(msg))
        {
            StatusManager status_mng;
            sct_mng->ptr()->fillStatuses(&status_mng, info);

            if (db_mng->setStatuses(&status_mng))
            {
                sct_mng->ptr()->StatusMng = status_mng;
                return true;
            }
        }
        break;
    case Network::stStatusType:
        if (auto info = static_cast<Prt::TypesInfo*>(msg))
        {
            StatusTypeManager statusType_mng;
            sct_mng->ptr()->fillStatusTypes(&statusType_mng, info);

            if (db_mng->setStatusTypes(&statusType_mng))
            {
                sct_mng->ptr()->StatusTypeMng = statusType_mng;
                return true;
            }
        }
        break;
    case Network::stParamTypes:
        if (auto info = static_cast<Prt::TypesInfo*>(msg))
        {
            ParamTypeManager paramType_mng;
            sct_mng->ptr()->fillParamTypes(&paramType_mng, info);

            if (db_mng->setParamTypes(&paramType_mng))
            {
                sct_mng->ptr()->ParamMng = paramType_mng;
                return true;
            }
        }
        break;
    case Network::stParamValues:
        if (auto info = static_cast<Prt::TypesInfo*>(msg))
        {
//            StatusTypeManager statusType_mng;
//            sct_mng->ptr()->fillStatusTypes(&statusType_mng, info);

            if (db_mng->setParamValues(info->paramvalues()))
            {
//                sct_mng->ptr()->StatusTypeMng = statusType_mng;
                return true;
            }
        }
        break;
    case Network::stCodes:
        if (auto info = static_cast<Prt::TypesInfo*>(msg))
        {
            CodeManager code_mng;
            sct_mng->ptr()->fillCodes(&code_mng, info);

            if (db_mng->setCodes(&code_mng))
            {
                sct_mng->ptr()->CodeMng = code_mng;
                return true;
            }
        }
        break;
    default: break;
    }
    restarter.isOk = false;
    return false;
}*/

void Worker::newValue(DeviceItem *item)
{
    auto cur_date = QDateTime::currentDateTime();

    waited_item_values[item->id()] = std::make_pair(item->getRawValue(), item->getValue());
    if (!item_values_timer.isActive())
        item_values_timer.start();

    QVariant db_id;
    bool immediately = sct_mng->ptr()->ItemTypeMng.saveAlgorithm(item->type()) == ItemType::saSaveImmediately;
    if (immediately && !db_mng->logChanges(item, cur_date, &db_id))
    {
        qWarning(Service::Log) << "Упущенное значение";
        // TODO: Error event
    }

    ValuePackItem pack_item{db_id.toUInt(), item->id(), cur_date.toMSecsSinceEpoch(), item->getRawValue(), item->getValue()};
    emit change(pack_item, immediately);

    if (webSock && websock_item) {
        QVector<Dai::ValuePackItem> pack{pack_item};
        QMetaObject::invokeMethod(webSock, "sendDeviceItemValues", Qt::QueuedConnection,
                                  QArgument<dai::project::info>("ProjInfo", websock_item.get()), Q_ARG(QVector<Dai::ValuePackItem>, pack));
    }
}

/*void Worker::sendLostValues(const QVector<quint32> &ids)
{
    QVector<ValuePackItem> pack;

    QVector<quint32> found, not_found;
    db_mng->getListValues(ids, found, pack);

    QMetaObject::invokeMethod(g_mng_th->ptr(), "sendLostValues", Qt::QueuedConnection, Q_ARG(QVector<Dai::ValuePackItem>, pack));

    std::set_difference(
        ids.cbegin(), ids.cend(),
        found.cbegin(), found.cend(),
        std::back_inserter( not_found )
    );

    if (not_found.size())
        QMetaObject::invokeMethod(g_mng_th->ptr(), "sendNotFoundIds", Qt::QueuedConnection, Q_ARG(const QVector<quint32>&, not_found));
}*/

} // namespace Dai
