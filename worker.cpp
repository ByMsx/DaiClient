
#include <QDebug>
#include <QDir>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>

#include <Helpz/consolereader.h>

#include <Dai/commands.h>
#include <Dai/checkerinterface.h>

#include "worker.h"

namespace dai {

WebSockItem::WebSockItem(Dai::Worker *obj) :
    QObject(), dai::project::base(),
    w(obj)
{
    set_id(1);
    set_title("localhost");
    set_teams({1});
    connect(w, &Dai::Worker::modeChanged, this, &WebSockItem::modeChanged, Qt::QueuedConnection);

}

WebSockItem::~WebSockItem() {
    disconnect(this);
}

void WebSockItem::modeChanged(uint mode_id, uint group_id) {

    QMetaObject::invokeMethod(w->webSock_th->ptr(), "sendModeChanged", Qt::QueuedConnection,
                              Q_ARG(ProjInfo, this), Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id));
}

void WebSockItem::procCommand(quint32 user_team_id, quint32 proj_id, quint8 cmd, const QByteArray &data)
{
    QDataStream ds(data);

    try {
        switch (cmd) {
        case Dai::wsConnectInfo:
            send(this, w->webSock_th->ptr()->getConnectState(id(), "127.0.0.1", QDateTime::currentDateTime().timeZone(), 0));
            break;

        case Dai::wsWriteToDevItem: Helpz::applyParse(&Dai::Worker::writeToItem, w, ds); break;
        case Dai::wsChangeGroupMode: Helpz::applyParse(&Dai::Worker::setMode, w, ds); break;
        case Dai::wsChangeParamValues: Helpz::applyParse(&Dai::Worker::setParamValues, w, ds); break;
        case Dai::wsRestart: w->serviceRestart(); break;
        case Dai::wsChangeCode:
        case Dai::wsExecScript:
            qCritical() << "Attempt to do something bad from local";
            break;

        default:
            qWarning() << "Unknown WebSocket Message:" << (Dai::WebSockCmd)cmd;
    //        pClient->sendBinaryMessage(message);
            break;
        }
    } catch(const std::exception& e) {
        qCritical() << "WebSock:" << e.what();
    } catch(...) {
        qCritical() << "WebSock unknown exception";
    }
}

} // namespace dai

namespace Dai {

namespace Z = Helpz;
using namespace std::placeholders;

Worker::Worker(QObject *parent) :
    QObject(parent)
{
    auto s = settings();

    int log_period = init_logging(s.get());
    init_Database(s.get());
    init_Project(s.get());
    init_Checker(s.get());
    init_GlobalClient(s.get());
    init_LogTimer(log_period);

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

    g_mng_th->quit();
    prj->quit();

    if (webSock_th && !webSock_th->wait(15000))
        webSock_th->terminate();
    if (!django_th->wait(15000))
        django_th->terminate();
    if (!g_mng_th->wait(15000))
        g_mng_th->terminate();
    if (!checker_th->wait(15000))
        checker_th->terminate();
    if (!prj->wait(15000))
        prj->terminate();

    if (webSock_th)
        delete webSock_th;
    delete django_th;
    delete g_mng_th;
    delete checker_th;
    delete prj;

    delete db_mng;
}

DBManager* Worker::database() const { return db_mng; }
const Helpz::Database::ConnectionInfo &Worker::database_info() const { return *db_info_; }

std::unique_ptr<QSettings> Worker::settings()
{
    QString configFileName = QCoreApplication::applicationDirPath() + QDir::separator() + QCoreApplication::applicationName() + ".conf";
    return std::unique_ptr<QSettings>(new QSettings(configFileName, QSettings::NativeFormat));
}

int Worker::init_logging(QSettings *s)
{
    auto [log_debug, log_syslog, log_period] = Helpz::SettingsHelper(
            s, "Log",
            Z::Param{"Debug", false},
        Z::Param{"Syslog", true},
        Z::Param{"Period", 60 * 30} // 30 минут
    )();

    logg().debug = log_debug;
#ifdef Q_OS_UNIX
    logg().syslog = log_syslog;
#endif
    QMetaObject::invokeMethod(&logg(), "init", Qt::QueuedConnection);
    return log_period;
}

void Worker::init_Database(QSettings* s)
{
    db_info_ = Helpz::SettingsHelper(
                s, "Database",
                Z::Param{"Name", "deviceaccess_local"},
                Z::Param{"User", "DaiUser"},
                Z::Param{"Password", ""},
                Z::Param{"Host", "localhost"},
                Z::Param{"Port", -1},
                Z::Param{"Driver", "QMYSQL"},
                Z::Param{"ConnectOptions", QString()}
    ).unique_ptr<Helpz::Database::ConnectionInfo>();
    if (!db_info_)
        throw std::runtime_error("Failed get database config");

    db_mng = new DBManager(*db_info_, "Worker_" + QString::number((quintptr)this));
}

void Worker::init_Project(QSettings* s)
{
    Helpz::ConsoleReader* cr = nullptr;
    if (Service::instance().isImmediately())
        cr = new Helpz::ConsoleReader(this);

//    qRegisterMetaType<std::shared_ptr<Dai::Prt::ServerInfo>>("std::shared_ptr<Dai::Prt::ServerInfo>");

    prj = ScriptsThread()(s, "Server", this,
                          cr,
                          Z::Param{"SSHHost", "80.89.129.98"},
                          Z::Param{"AllowShell", false}
                          );
    prj->start(QThread::HighPriority);
//    while (!prj->ptr() && !prj->wait(5));
}

void Worker::init_Checker(QSettings* s)
{
    qRegisterMetaType<Device*>("Device*");

    checker_th = CheckerThread()(s, "Checker", this, Z::Param{"Interval", 1500}, Z::Param<QStringList>{"Plugins", {"ModbusPlugin"}} );
    checker_th->start();
}

void Worker::init_GlobalClient(QSettings* s)
{
    qRegisterMetaType<ValuePackItem>("ValuePackItem");
    qRegisterMetaType<QVector<ValuePackItem>>("QVector<Dai::ValuePackItem>");
    qRegisterMetaType<QVector<EventPackItem>>("QVector<Dai::EventPackItem>");
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

                db_id.clear();
                if (db_mng->logChanges(dev_item, cur_date, &db_id))
                {
                    ValuePackItem packItem{ db_id.toUInt(), dev_item->id(), cur_date.toMSecsSinceEpoch(),
                                    dev_item->getRawValue(), dev_item->getValue() };
                    QMetaObject::invokeMethod(g_mng_th->ptr(), "change", Qt::QueuedConnection,
                                              Q_ARG(ValuePackItem, packItem), Q_ARG(bool, false));
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
                             Helpz::Param{"manage", "/var/www/dai/manage.py"});
    django_th->start();
    while (!django_th->ptr() && !django_th->wait(5));
}

void Worker::initWebSocketManager(QSettings *s)
{
    std::tuple<bool> en_t = Helpz::SettingsHelper(s, "WebSocket", Helpz::Param{"Enabled", true})();
    if (!std::get<0>(en_t))
        return;

    webSock_th = WebSocketThread()(
                s, "WebSocket",
                Helpz::Param{"CertPath", QString()},
                Helpz::Param{"KeyPath", QString()},
                Helpz::Param<quint16>{"Port", 25589});
    webSock_th->start();

    while (!webSock_th->ptr() && !webSock_th->wait(5));
    connect(webSock_th->ptr(), &dai::Network::WebSocket::checkAuth,
                     django_th->ptr(), &dai::DjangoHelper::checkToken, Qt::BlockingQueuedConnection);

    websock_item.reset(new dai::WebSockItem(this));
    connect(webSock_th->ptr(), &dai::Network::WebSocket::throughCommand,
            websock_item.get(), &dai::WebSockItem::procCommand, Qt::BlockingQueuedConnection);
    connect(websock_item.get(), &dai::WebSockItem::send, webSock_th->ptr(), &dai::Network::WebSocket::send, Qt::QueuedConnection);
//    webSock_th->ptr()->get_proj_in_team_by_id.connect(
//                std::bind(&Worker::proj_in_team_by_id, this, std::placeholders::_1, std::placeholders::_2));
}

//std::shared_ptr<dai::project::base> Worker::proj_in_team_by_id(uint32_t team_id, uint32_t proj_id) {
    // TODO: Check team_id valid
//    return std::static_pointer_cast<dai::project::base>(websock_item);
//}

void Worker::logMessage(QtMsgType type, const Helpz::LogContext &ctx, const QString &str)
{
    if (qstrcmp(ctx->category, Helpz::Network::DetailLog().categoryName()) == 0 ||
            qstrncmp(ctx->category, "net", 3) == 0)
        return;

    QDateTime cur_date = QDateTime::currentDateTime();
    QVariant db_id;
    if (db_mng->eventLog(type, ctx->category, str, cur_date, &db_id)) {
        EventPackItem item{db_id.toUInt(), type, cur_date.toMSecsSinceEpoch(), ctx->category, str};
        QMetaObject::invokeMethod(g_mng_th->ptr(), "eventLog", Qt::QueuedConnection, Q_ARG(EventPackItem, item));
        if (webSock_th)
            QMetaObject::invokeMethod(webSock_th->ptr(), "sendEventMessage", Qt::QueuedConnection,
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

void Worker::setControlState(quint32 section_id, quint32 item_type, const QVariant &raw_data)
{
    if (auto sct = prj->ptr()->sectionById( section_id ))
        QMetaObject::invokeMethod(sct, "setControlState", Qt::QueuedConnection,
                                  Q_ARG(uint, item_type), Q_ARG(QVariant, raw_data) );
}

void Worker::writeToItem(quint32 item_id, const QVariant &raw_data)
{
    if (DeviceItem* item = prj->ptr()->itemById( item_id ))
        QMetaObject::invokeMethod(item->group(), "writeToControl", Qt::QueuedConnection,
                                  Q_ARG(DeviceItem*, item), Q_ARG(QVariant, raw_data) );
}

bool Worker::setMode(uint mode_id, quint32 group_id)
{
    bool res = db_mng->setMode(mode_id, group_id);
    if (res)
        QMetaObject::invokeMethod(prj->ptr(), "setMode", Qt::QueuedConnection, Q_ARG(quint32, mode_id), Q_ARG(quint32, group_id) );
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

void Worker::setParamValues(const ParamValuesPack &pack)
{
    qCDebug(Service::Log) << "setParamValues" << pack.size() << pack;

    ParamValuesPack params = pack;
    Param* p;

    for(Section* sct: prj->ptr()->sections())
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

bool Worker::applyStructModify(quint8 structType, QDataStream *msg)
{
    using namespace Network;
    qCDebug(Service::Log) << "applyStructModify" << (StructureType)structType;

    try {
        switch ((StructureType)structType) {
        case stDevices:
            return Helpz::applyParse(&Database::applyModifyDevices, db_mng, *msg);
        case stCheckerType:
            return Helpz::applyParse(&Database::applyModifyCheckerTypes, db_mng, *msg);
        case stDeviceItems:
            return Helpz::applyParse(&Database::applyModifyDeviceItems, db_mng, *msg);
        case stDeviceItemTypes:
            return Helpz::applyParse(&Database::applyModifyDeviceItemTypes, db_mng, *msg);
        case stSections:
            return Helpz::applyParse(&Database::applyModifySections, db_mng, *msg);
        case stGroups:
            return Helpz::applyParse(&Database::applyModifyGroups, db_mng, *msg);
        case stGroupTypes:
            return Helpz::applyParse(&Database::applyModifyGroupTypes, db_mng, *msg);
        case stGroupParams:
            return Helpz::applyParse(&Database::applyModifyGroupParams, db_mng, *msg);
        case stGroupParamTypes:
            return Helpz::applyParse(&Database::applyModifyGroupParamTypes, db_mng, *msg);
        case stGroupStatuses:
            return Helpz::applyParse(&Database::applyModifyGroupStatuses, db_mng, *msg);
        case stGroupStatusTypes:
            return Helpz::applyParse(&Database::applyModifyGroupStatusTypes, db_mng, *msg);
        case stSigns:
            return Helpz::applyParse(&Database::applyModifySigns, db_mng, *msg);
        case stScripts:
            return Helpz::applyParse(&Database::applyModifyScripts, db_mng, *msg);

        default: return false;
        }
    } catch(const std::exception& e) {
        qCritical() << "EXCEPTION: applyStructModify" << (StructureType)structType << e.what();
    }
    return false;
}

void Worker::newValue(DeviceItem *item)
{
    auto cur_date = QDateTime::currentDateTime();

    waited_item_values[item->id()] = std::make_pair(item->getRawValue(), item->getValue());
    if (!item_values_timer.isActive())
        item_values_timer.start();

    QVariant db_id;
    bool immediately = prj->ptr()->ItemTypeMng.saveAlgorithm(item->type()) == ItemType::saSaveImmediately;
    if (immediately && !db_mng->logChanges(item, cur_date, &db_id))
    {
        qWarning(Service::Log) << "Упущенное значение:" << item->toString() << item->getValue().toString();
        // TODO: Error event
    } else if (prj->ptr()->ItemTypeMng.saveAlgorithm(item->type()) == ItemType::saInvalid)
        qWarning(Service::Log) << "Неправильный параметр сохранения" << item->toString();

    ValuePackItem pack_item{db_id.toUInt(), item->id(), cur_date.toMSecsSinceEpoch(), item->getRawValue(), item->getValue()};
    emit change(pack_item, immediately);

    if (webSock_th) {
        QVector<Dai::ValuePackItem> pack{pack_item};
        QMetaObject::invokeMethod(webSock_th->ptr(), "sendDeviceItemValues", Qt::QueuedConnection,
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
