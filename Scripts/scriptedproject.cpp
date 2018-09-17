#include <QTimer>
#include <QElapsedTimer>

#include <QFile>
#include <QTextStream>
#include <QDirIterator>
#include <QDebug>
#include <QMetaEnum>
#include <QProcess>

#include <Helpz/consolereader.h>

#include "scriptedproject.h"
#include "paramgroupclass.h"

#include "tools/automationhelper.h"
#include "tools/pidhelper.h"
#include "tools/resthelper.h"
#include "tools/severaltimeshelper.h"
#include "tools/inforegisterhelper.h"

#include "worker.h"
#include "Database/db_manager.h"

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(Dai::SectionPtr)
Q_DECLARE_METATYPE(Dai::TypeManagers*)

namespace Dai {

Q_LOGGING_CATEGORY(ScriptLog, "script")

template<class T>
QScriptValue sharedPtrToScriptValue(QScriptEngine *eng, const T &obj) {
    return eng->newQObject(obj.get());
}
template<class T>
void emptyFromScriptValue(const QScriptValue &, T &) {}

QScriptValue stdstringToScriptValue(QScriptEngine *, const std::string& str)
{ return QString::fromStdString(str); }

// ------------------------------------------------------------------------------------

template<class T>
QScriptValue ptrToScriptValue(QScriptEngine *eng, const T& obj) {
    return eng->newQObject(obj);
}

struct ArgumentGetter {
    ArgumentGetter(QScriptContext* _ctx) : ctx(_ctx) {}
    QScriptContext* ctx;
    int arg_idx = 0;
    QScriptValue operator() () {
        return ctx->argument(arg_idx++);
    }
};

template<typename T, template<typename> class P, typename... Args>
void ScriptedProject::addType()
{
    auto ctor = [](QScriptContext* ctx, QScriptEngine* eng) -> QScriptValue {
        if ((unsigned)ctx->argumentCount() >= sizeof...(Args))
        {
            ArgumentGetter get_arg(ctx);

            auto obj = new T(qscriptvalue_cast<Args>(get_arg())...);
            return eng->newQObject(obj, QScriptEngine::ScriptOwnership);
        }
        return P<T>()(ctx, eng);
    };

    auto value = eng->newQMetaObject(&T::staticMetaObject,
                                     eng->newFunction(ctor));

    QString name = T::staticMetaObject.className();

    int four_dots = name.lastIndexOf("::");
    if (four_dots >= 0)
        name.remove(0, four_dots + 2);

    eng->globalObject().setProperty(name, value);

//    qCDebug(ProjectLog) << "Register for script:" << name;
}

ScriptedProject::ScriptedProject(Worker* worker, Helpz::ConsoleReader *consoleReader, const QString &sshHost) :
    Project(), //(worker->database()->clone<DBManager>("ScriptSql")),
    m_func(fAutomation), m_dayTime(this),
    m_uptime(QDateTime::currentMSecsSinceEpoch())
{
//    qDebug() << "Copy scr " << db()->db().databaseName() << db()->db().password() << worker->database()->db().password();

//    db()->createConnection();
//    db()->setTypeManager(this);
    registerTypes();

    eng->pushContext();
    reinitialization(worker->database_info());

    setSSHHost(sshHost);

    using T = ScriptedProject;
    connect(this, &T::modeChanged, worker, &Worker::modeChanged, Qt::QueuedConnection);
    connect(this, &T::sctItemChanged, worker, &Worker::newValue);
    connect(this, &T::groupStatusChanged, worker, &Worker::groupStatusChanged, Qt::QueuedConnection);
//    connect(worker, &Worker::dumpSectionsInfo, this, &T::dumpInfo, Qt::BlockingQueuedConnection);

    connect(this, &ScriptedProject::dayTimeChanged, &m_dayTime, &DayTimeHelper::init, Qt::QueuedConnection);

    if (consoleReader)
        connect(consoleReader, &Helpz::ConsoleReader::textReceived, this, &T::console);
}

ScriptedProject::~ScriptedProject()
{
//    delete db();
}

void ScriptedProject::setSSHHost(const QString &value) { ssh_host = value; }

qint64 ScriptedProject::uptime() const { return m_uptime; }

void ScriptedProject::reinitialization(const Helpz::Database::ConnectionInfo& db_info)
{
    eng->popContext();
    QScriptContext* ctx = eng->pushContext();
    QScriptValue obj = ctx->activationObject();

    m_func.resize(fAutomation);

    std::unique_ptr<Database> db(new Database(db_info, "ProjectManager_" + QString::number((quintptr)this)));
    db->fillTypes(this);
    scriptsInitialization();

    m_func[fInitSection] = obj.property("initSection");
    m_func[fAfterAllInitialization] = obj.property("afterAllInitialization");
    m_func[fModeChanged] = obj.property("modeChanged");
    if (!m_func[fModeChanged].isFunction())
        m_func[fModeChanged] = obj.property("autoChanged");

    m_func[fItemChanged] = obj.property("itemChanged");
    m_func[fSensorChanged] = obj.property("sensorChanged");
    m_func[fControlChanged] = obj.property("controlChanged");
    m_func[fDayPartChanged] = obj.property("dayPartChanged");
    m_func[fControlChangeCheck] = obj.property("controlChangeCheck");
    m_func[fNormalize] = obj.property("normalize");
    m_func[fAfterDatabaseInit] = obj.property("afterDatabaseInit");
    m_func[fCheckValue] = obj.property("checkValue");
    m_func[fGroupStatus] = obj.property("groupStatus");

    m_dayTime.stop();
    qScriptDisconnect(&m_dayTime, SIGNAL(onDayPartChanged(Section*,bool)), QScriptValue(), QScriptValue());
    m_dayTime.disconnect(SIGNAL(onDayPartChanged(Section*,bool)));
    qScriptConnect(&m_dayTime, SIGNAL(onDayPartChanged(Section*,bool)), QScriptValue(), m_func.at(fDayPartChanged));

    initFromDatabase(db.get(), true);

    if (m_func.at(fDayPartChanged).isFunction())
        m_dayTime.init();

    for(Device* dev: devices())
        for (DeviceItem* dev_item: dev->items())
                if (dev_item->needNormalize())
                    connect(dev_item, &DeviceItem::normalize, this, &ScriptedProject::normalize);

    for(Section* sct: sections())
        for (ItemGroup* group: sct->groups())
        {
            connect(group, &ItemGroup::getStatus, this, &ScriptedProject::groupStatus);
            connect(group, &ItemGroup::checkValue, this, &ScriptedProject::checkValue);
            connect(group, &ItemGroup::statusChanged, this, &ScriptedProject::statusChanged);
            connect(group, &ItemGroup::itemChanged, this, &ScriptedProject::itemChanged);
            connect(group, &ItemGroup::itemChanged, this, &ScriptedProject::sctItemChanged);
            connect(group, &ItemGroup::modeChanged, this, &ScriptedProject::groupModeChanged);

            if (m_func.at(fControlChangeCheck).isFunction())
                connect(group, &ItemGroup::controlChangeCheck, this, &ScriptedProject::controlChangeCheck);
        }

    QScriptValue afterDatabaseInit = obj.property("afterDatabaseInit");
    if (afterDatabaseInit.isFunction())
        afterDatabaseInit.call();
}

void ScriptedProject::registerTypes()
{
    qRegisterMetaType<Section*>("Section*");
    qRegisterMetaType<Sections*>("Sections*");
    qRegisterMetaType<ItemGroup*>("ItemGroup*");

    qRegisterMetaType<AutomationHelper*>("AutomationHelper*");

    eng = new QScriptEngine(this);
    connect(eng, &QScriptEngine::signalHandlerException,
            this, &ScriptedProject::handlerException);

    addType<QTimer>();
    addType<QProcess>();

    addType<AutomationHelper, TypeDefault, uint>();

    addTypeN<AutomationHelperItem, ItemGroup*>();
    addTypeN<SeveralTimesHelper, ItemGroup*>();
    addTypeN<RestHelper, ItemGroup*>();
    addTypeN<PIDHelper, ItemGroup*, uint>();
    addTypeN<InfoRegisterHelper, ItemGroup*, uint, uint>();

    addTypeN<Section, TypeManagers*, quint32, QString, TimeRange, QObject*>();
    addTypeN<ItemGroup, uint, uint, Section*>();

    qRegisterMetaType<Sections>("Sections");
    qScriptRegisterSequenceMetaType<Sections>(eng);

    qRegisterMetaType<Devices>("Devices");
    qScriptRegisterSequenceMetaType<Devices>(eng);

    qRegisterMetaType<DeviceItems>("DeviceItems");
    qScriptRegisterSequenceMetaType<DeviceItems>(eng);

    qRegisterMetaType<QVector<DeviceItem*>>("QVector<DeviceItem*>");
    qScriptRegisterSequenceMetaType<QVector<DeviceItem*>>(eng);

    qRegisterMetaType<AutomationHelperItem*>("AutomationHelperItem*");

    qScriptRegisterMetaType<ItemGroup::ValueType>(eng, sharedPtrToScriptValue<ItemGroup::ValueType>, emptyFromScriptValue<ItemGroup::ValueType>);
//    qScriptRegisterMetaType<SectionPtr>(eng, sharedPtrToScriptValue<SectionPtr>, emptyFromScriptValue<SectionPtr>);
    qScriptRegisterMetaType<std::string>(eng, stdstringToScriptValue, emptyFromScriptValue<std::string>);

//    qScriptRegisterMetaType<DeviceItem::ValueType>(eng, itemValueToScriptValue, itemValueFromScriptValue);
    //    qScriptRegisterSequenceMetaType<DeviceItem::ValueList>(eng);

    //    qScriptRegisterSequenceMetaType<DeviceItem::ValueList>(eng);

    auto paramClass = new ParamGroupClass(eng);
    eng->globalObject().setProperty("Params", paramClass->constructor());
//    eng->globalObject().setProperty("ParamElem", paramClass->constructor());
}

template<typename T>
void initTypes(QScriptValue& parent, const QString& prop_name, BaseTypeManager<T>& type_mng) {
    QScriptValue deprecated_types_node = parent.engine()->currentContext()->activationObject().property("api");

    QScriptValue types_obj = parent.property(prop_name);
    QString name;
    for (const T& type: type_mng.types()) {
        name = type.name();
        if (!name.isEmpty()) {
            types_obj.setProperty(name, type.id());

            // Deprecated:
            QString suffix = prop_name;
            suffix[0] = suffix.at(0).toUpper();
            name[0] = name.at(0).toUpper();
            deprecated_types_node.setProperty(name + suffix, type.id());
        }
    }
}

void ScriptedProject::scriptsInitialization()
{
    auto readScriptFile = [](const QString& fileBase) -> QString
    {
        QString content;

        QFile scriptFile(":/Scripts/js/" + fileBase + ".js");
        if (scriptFile.exists())
        {
            scriptFile.open(QIODevice::ReadOnly | QFile::Text);
            QTextStream stream(&scriptFile);
            content = stream.readAll();
            scriptFile.close();
        }

        return content;
    };

    check_error( "API", readScriptFile("api") );

    QScriptValue api = eng->currentContext()->activationObject().property("api");
    api.setProperty("mng", eng->newQObject(this));

    QScriptValue statuses = api.property("status");

    std::map<uint, std::vector<GroupStatus*>> statusMap;
    for (GroupStatus& type: *StatusMng.getTypes())
        statusMap[type.groupType_id].push_back(&type);

    QString status_name;
    for (auto it: statusMap)
    {
        status_name = GroupTypeMng.name(it.first);
        statuses.setProperty(status_name, eng->newObject());
        auto statusGroup = statuses.property(status_name);
        quint32 multiVal = ItemGroup::valUser;
        for (GroupStatus* type: it.second)
        {
            if (!type->isMultiValue)
                statusGroup.setProperty(type->name(), type->value);
            else
            {
                statusGroup.setProperty(type->name(), multiVal);
                multiVal <<= 1;
            }
        }
    }

    QScriptValue types = api.property("type");
    initTypes(types, "item", ItemTypeMng);
    initTypes(types, "group", GroupTypeMng);
    initTypes(types, "mode", ModeTypeMng);
    initTypes(types, "param", ParamMng);

    QString code, func_name;
    QMetaEnum metaEnum = QMetaEnum::fromType<ScriptFunction>();
    for (uint i = fOtherScripts; i < fAutomation; ++i)
    {
        code = CodeMng.type( i ).text;
        if (!code.isEmpty())
        {
            func_name = metaEnum.valueToKey(static_cast<ScriptFunction>(i));
            check_error( func_name, code );
        }
    }

    for (const ItemGroupType& type: GroupTypeMng.types())
    {
        if (type.code)
        {
            code = CodeMng.type( type.code ).text;
            if (!code.isEmpty())
                check_error( type.name(), code );
            else
                qCDebug(ScriptLog) << type.id() << type.name() << "code empty.";
        }

        if (!type.name().isEmpty())
        {
            func_name = type.name() + "Changed";
            QScriptValue js_func = eng->currentContext()->activationObject().property(func_name);

            if (!js_func.isFunction())
                js_func = eng->currentContext()->activationObject().property(type.name() + "Automation");

            if (js_func.isFunction())
            {
                m_automation[ type.id() ] = m_func.size();
                m_func.push_back(js_func);
            }
            else
                qCDebug(ScriptLog) << "Group type" << type.id() << type.name() << "havent automation function" << func_name;
        }
        else
            qCDebug(ScriptLog) << "Group type" << type.id() << type.name() << "havent latin name";
    }

//    QDir scripts(":/Scripts/js");
//    auto js_list = scripts.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
//    for (auto fileInfo: js_list)
//        if (fileInfo.suffix() == "js" && fileInfo.baseName() != "api")
//            evaluateFile( fileInfo.filePath() );

    QScriptValue checker_list = api.property("checker");
    if (checker_list.isArray())
    {
        int length = checker_list.property("length").toInt32();
        for (int i = 0; i < length; ++i)
        {
            // TODO: Load Script checkers
        }
        qCDebug(ScriptLog) << "Load" << length << "script checkers";
    }
}

Section *ScriptedProject::addSection(quint32 id, const QString &name, const TimeRange &dayTime)
{
    auto sct = Project::addSection(id, name, dayTime);
    connect(sct, &Section::groupInitialized, this, &ScriptedProject::groupInitialized);
//    connect(sct, SIGNAL(autoChanged(uint,bool)), SLOT(autoChanged(uint,bool)));
//    connect(sct, SIGNAL(itemChanged(DeviceItem*)), SLOT(itemChanged(DeviceItem*)));
//    connect(sct, SIGNAL(itemChanged(DeviceItem*)), SIGNAL(sctItemChanged(DeviceItem*)));
//    connect(sct, &Section::autoChanged, [this](uint type, bool isAuto) { });

    callFunction(fInitSection, { eng->newQObject(sct) });
    return sct;
}

QScriptValue ScriptedProject::valueFromVariant(const QVariant &data) const
{
    switch (data.type()) {
    case QVariant::Bool:                return data.toBool();
    case QVariant::String:              return data.toString();
    case QVariant::Int:                 return data.toInt();
    case QVariant::UInt:                return data.toUInt();
    case QVariant::LongLong:
    case QVariant::ULongLong:
    case QVariant::Double:              return data.toDouble();
    case QVariant::Date:
    case QVariant::Time:
    case QVariant::DateTime:            return eng->newDate(data.toDateTime());
    case QVariant::RegExp:
    case QVariant::RegularExpression:   return eng->newRegExp(data.toRegExp());
    default:
        return  eng->newVariant(data);
    }
}

void ScriptedProject::log(const QString &msg, uint type)
{
    switch (type) {
    case QtCriticalMsg: qCCritical(ScriptLog).noquote() << msg; break;
    case QtWarningMsg: qCWarning(ScriptLog).noquote() << msg; break;
    case QtInfoMsg: qCInfo(ScriptLog).noquote() << msg; break;
    case QtDebugMsg:
    default:
        qCDebug(ScriptLog).noquote() << msg;
        break;
    }
}

void ScriptedProject::console(const QString &cmd)
{
    QString script = cmd.trimmed();
    if (script.isEmpty() || !eng->canEvaluate(script))
        return;

    auto res = eng->evaluate(script, "CONSOLE");
    bool is_error = res.isError();

    if (!res.isUndefined())
    {
        if (!res.isError() && (res.isObject() || res.isArray()))
        {
            QScriptValue check_func = eng->evaluate("(function(key, value) {"
                                                      "if ((typeof value === 'object' || typeof value === 'function') && value !== null && !Array.isArray(value) && value.toString() !== '[object Object]') {"
                                                        "return value.toString();"
                                                      "}"
                                                      "return value;"
                                                    "})");
            res = eng->evaluate("JSON.stringify").call(QScriptValue(), {res, check_func, ' '});
        }

        if (res.isError())
        {
            res = res.property("message");
            is_error = true;
        }
    }

    (is_error ? qCritical(ScriptLog) : qInfo(ScriptLog)).noquote() << "CONSOLE ["<< script << "] >" << res.toString();
}

//void ScriptedProject::evaluateFile(const QString& fileName)
//{
//    QFile scriptFile(fileName);
//    scriptFile.open(QIODevice::ReadOnly);
//    QTextStream stream(&scriptFile);
//    QString contents = stream.readAll();
//    scriptFile.close();

//    check_error( fileName, eng->evaluate(contents, fileName) );
//}

QScriptValue ScriptedProject::callFunction(uint func_idx, const QScriptValueList& args) const
{
    if (func_idx < m_func.size())
    {
        auto func = m_func.at(func_idx);
        if (func.isFunction())
        {
            QScriptValue ret = func.call(QScriptValue(), args);
            check_error( func.property("name").toString(), ret);
            return ret;
        }
    }
    return QScriptValue();
}

void ScriptedProject::run_automation(ItemGroup* group, const QScriptValue& groupObj, const QScriptValue &itemObj)
{
    auto automation = m_automation.find(group->type());
    if (automation != m_automation.cend())
        callFunction(automation->second, { (groupObj.isValid() ? groupObj : eng->newQObject(group)), itemObj });
}

void ScriptedProject::groupModeChanged(uint mode, quint32 /*group_id*/)
{
    auto group = static_cast<ItemGroup*>(sender());
    if (!group)
        return;
    QScriptValue groupObj = eng->newQObject(group);

    callFunction(fModeChanged, { groupObj, mode });
    run_automation(group, groupObj);
}

QElapsedTimer t;
void ScriptedProject::itemChanged(DeviceItem *item)
{
    t.restart();

    auto group = static_cast<ItemGroup*>(sender());

    QScriptValue groupObj = eng->newQObject(group);
    QScriptValue itemObj = eng->newQObject(item);

    callFunction(fItemChanged, { groupObj, itemObj });

    if (item->isControl())
        callFunction(fControlChanged, { groupObj, itemObj });
    else
        callFunction(fSensorChanged, { groupObj, itemObj });

    run_automation(group, groupObj, itemObj);

//    eng->collectGarbage();

    if (t.elapsed() > 500)
        qCWarning(ProjectLog) << "itemChanged timeout" << t.elapsed();

    t.invalidate();
}

void ScriptedProject::statusChanged(quint32 status)
{
    groupStatusChanged(static_cast<ItemGroup*>(sender())->id(), status);
}

void ScriptedProject::afterAllInitialization()
{
    callFunction(fAfterAllInitialization);
}

void ScriptedProject::ssh(quint16 port, quint32 remote_port)
{
    if (pid > 0)
        QProcess::startDetached("kill", { QString::number(pid) });
    // ssh -o StrictHostKeyChecking=no -N -R 0.0.0.0:12345:localhost:22 -p 15666 root@deviceaccess.ru
    QStringList args{"-o", "StrictHostKeyChecking=no", "-N", "-R", QString("0.0.0.0:%1:localhost:22").arg(remote_port), "-p", QString::number(port), "root@" + ssh_host };
    QProcess::startDetached("ssh", args, QString(), &pid);

    qCInfo(ProjectLog) << "SSH Client started" << pid;
}

QVariantMap ScriptedProject::run_command(const QString &programm, const QVariantList &args, int timeout_msec) const
{
    QStringList args_list;
    for (const QVariant& arg: args)
        args_list.push_back(arg.toString());

    QProcess proc;
    proc.start(programm, args_list);

    QVariantMap result;
    if (proc.waitForStarted(timeout_msec) && proc.waitForFinished(timeout_msec) && proc.bytesAvailable())
        result["output"] = QString::fromUtf8(proc.readAllStandardOutput());

    result["error"] = QString::fromUtf8(proc.readAllStandardError());
    result["code"] = proc.exitCode();
    return result;
}

void ScriptedProject::groupInitialized(ItemGroup* group)
{
    QString func_name = GroupTypeMng.name(group->type()) + "Initialized";
    auto func = eng->currentContext()->activationObject().property(func_name);
    if (func.isFunction())
    {
        auto ret = func.call(QScriptValue(), QScriptValueList{ eng->newQObject(group) });
        check_error( func.property("name").toString(), ret);
    }
    else
        qCDebug(ScriptLog) << "Group type" << group->type() << GroupTypeMng.name(group->type()) << "havent init function" << func_name;
}

QVariant ScriptedProject::normalize(const QVariant &val)
{
    return callFunction(fNormalize, { eng->newQObject(sender()), valueFromVariant(val) }).toVariant();
}

bool ScriptedProject::controlChangeCheck(DeviceItem *item, const QVariant &raw_data)
{
    auto ret = callFunction(fControlChangeCheck, { eng->newQObject(sender()), eng->newQObject(item), valueFromVariant(raw_data) });
    return ret.isBool() && ret.toBool();
}

bool ScriptedProject::checkValue(DeviceItem* item) const
{
    auto ret = callFunction(fCheckValue, { eng->newQObject(sender()),
                                           valueFromVariant(item->getValue()),
                                           eng->newQObject(item) });
    return ret.isBool() && ret.toBool();
}

quint32 ScriptedProject::groupStatus(ItemGroup::ValueType val) const
{
    auto ret = callFunction(fGroupStatus, { eng->newQObject(sender()), eng->toScriptValue(val) });
    return ret.isNumber() ? ret.toUInt32() : val->status();
}

void ScriptedProject::handlerException(const QScriptValue &exception)
{
    if (eng->hasUncaughtException())
        qCCritical(ProjectLog) << exception.toString() << eng->uncaughtExceptionBacktrace();
}

void ScriptedProject::check_error(const QString& str, const QScriptValue& result) const
{
    if (result.isError()) {
        qCCritical(ProjectLog) << QString::fromLatin1("%0 %1(%2): %3")
                        .arg(str)
                        .arg(result.property("fileName").toString())
                        .arg(result.property("lineNumber").toInt32())
                        .arg(result.toString());
    }
}

void ScriptedProject::check_error(const QString &name, const QString &code) const
{
    check_error(name, eng->evaluate(code, name));
}

} // namespace Dai
