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
#include "tools/severaltimeshelper.h"
#include "tools/inforegisterhelper.h"

#include "worker.h"
#include "Database/db_manager.h"

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(Dai::SectionPtr)
Q_DECLARE_METATYPE(Dai::Type_Managers*)

namespace Dai {

Q_LOGGING_CATEGORY(ScriptLog, "script")
Q_LOGGING_CATEGORY(ScriptEngineLog, "script.engine")
Q_LOGGING_CATEGORY(ScriptDetailLog, "script.detail", QtInfoMsg)

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
    auto ctor = [](QScriptContext* ctx, QScriptEngine* m_script_engine) -> QScriptValue {
        if ((unsigned)ctx->argumentCount() >= sizeof...(Args))
        {
            ArgumentGetter get_arg(ctx);

            auto obj = new T(qscriptvalue_cast<Args>(get_arg())...);
            return m_script_engine->newQObject(obj, QScriptEngine::ScriptOwnership);
        }
        return P<T>()(ctx, m_script_engine);
    };

    auto value = m_script_engine->newQMetaObject(&T::staticMetaObject,
                                     m_script_engine->newFunction(ctor));

    QString name = T::staticMetaObject.className();

    int four_dots = name.lastIndexOf("::");
    if (four_dots >= 0)
        name.remove(0, four_dots + 2);

    m_script_engine->globalObject().setProperty(name, value);

//    qCDebug(ProjectLog) << "Register for script:" << name;
}

ScriptedProject::ScriptedProject(Worker* worker, Helpz::ConsoleReader *consoleReader, const QString &sshHost, bool allow_shell) :
    Project(),
    m_dayTime(this),
    m_uptime(QDateTime::currentMSecsSinceEpoch()),
    ssh_host(sshHost), allow_shell_(allow_shell)
{
    registerTypes();

    m_script_engine->pushContext();
    reinitialization(worker->database_info());

    setSSHHost(sshHost);

    using T = ScriptedProject;
    connect(this, &T::modeChanged, worker, &Worker::modeChanged, Qt::QueuedConnection);
    connect(this, &T::status_added, worker, &Worker::status_added, Qt::QueuedConnection);
    connect(this, &T::status_removed, worker, &Worker::status_removed, Qt::QueuedConnection);
    connect(this, &T::sctItemChanged, worker, &Worker::newValue);
    connect(this, &T::add_event_message, worker, &Worker::add_event_message, Qt::QueuedConnection);
//    connect(worker, &Worker::dumpSectionsInfo, this, &T::dumpInfo, Qt::BlockingQueuedConnection);

    connect(this, &ScriptedProject::dayTimeChanged, &m_dayTime, &DayTimeHelper::init, Qt::QueuedConnection);

    if (consoleReader)
    {
        connect(consoleReader, &Helpz::ConsoleReader::textReceived, [this](const QString& text)
        {
            QMetaObject::invokeMethod(this, "console", Qt::QueuedConnection, Q_ARG(uint32_t, 0), Q_ARG(QString, text));
        });
    }
}

ScriptedProject::~ScriptedProject()
{
//    delete db();
}

void ScriptedProject::setSSHHost(const QString &value) { if (ssh_host != value) ssh_host = value; }

qint64 ScriptedProject::uptime() const { return m_uptime; }

void ScriptedProject::reinitialization(const Helpz::Database::Connection_Info& db_info)
{
    m_script_engine->popContext();
    /*QScriptContext* ctx = */m_script_engine->pushContext();

    std::unique_ptr<Database::Helper> db(new Database::Helper(db_info, "ProjectManager_" + QString::number((quintptr)this)));
    db->fillTypes(this);
    scriptsInitialization();

    m_dayTime.stop();
    qScriptDisconnect(&m_dayTime, SIGNAL(onDayPartChanged(Section*,bool)), QScriptValue(), QScriptValue());
    m_dayTime.disconnect(SIGNAL(onDayPartChanged(Section*,bool)));

    // qScriptConnect(&m_dayTime, SIGNAL(onDayPartChanged(Section*,bool)), QScriptValue(), m_func.at(FUNC_CHANGED_DAY_PART));
    connect(&m_dayTime, &DayTimeHelper::onDayPartChanged, [this](Section* sct, bool is_day)
    {
        callFunction(FUNC_CHANGED_DAY_PART, { m_script_engine->newQObject(sct), is_day });
    });

    db->initProject(this, true);

    if (get_handler(FUNC_CHANGED_DAY_PART).isFunction())
        m_dayTime.init();

    for(Device* dev: devices())
        for (DeviceItem* dev_item: dev->items())
                if (dev_item->needNormalize())
                    connect(dev_item, &DeviceItem::normalize, this, &ScriptedProject::normalize);

    for(Section* sct: sections())
    {
        for (ItemGroup* group: sct->groups())
        {
            connect(group, &ItemGroup::itemChanged, this, &ScriptedProject::itemChanged);
            connect(group, &ItemGroup::itemChanged, this, &ScriptedProject::sctItemChanged);
            connect(group, &ItemGroup::modeChanged, this, &ScriptedProject::groupModeChanged);
            connect(group, &ItemGroup::paramChanged, this, &ScriptedProject::group_param_changed);
            connect(group, &ItemGroup::status_added, this, &ScriptedProject::status_added);
            connect(group, &ItemGroup::status_removed, this, &ScriptedProject::status_removed);

            if (get_handler(FUNC_CONTROL_CHANGE_CHECK).isFunction())
                connect(group, &ItemGroup::controlChangeCheck, this, &ScriptedProject::controlChangeCheck);
        }
    }

    callFunction(FUNC_AFTER_DATABASE_INIT);
}

void ScriptedProject::registerTypes()
{
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<Section*>("Section*");
    qRegisterMetaType<Sections*>("Sections*");
    qRegisterMetaType<ItemGroup*>("ItemGroup*");

    qRegisterMetaType<AutomationHelper*>("AutomationHelper*");

    m_script_engine = new QScriptEngine(this);
    connect(m_script_engine, &QScriptEngine::signalHandlerException,
            this, &ScriptedProject::handlerException);

    addType<QTimer>();
    addType<QProcess>();

    addType<AutomationHelper, TypeDefault, uint>();

    addTypeN<AutomationHelperItem, ItemGroup*>();
    addTypeN<SeveralTimesHelper, ItemGroup*>();
    addTypeN<PIDHelper, ItemGroup*, uint>();
    addTypeN<InfoRegisterHelper, ItemGroup*, uint, uint>();

    addTypeN<Section, uint32_t, QString, uint32_t, uint32_t>();
    addTypeN<ItemGroup, uint32_t, QString, uint32_t, uint32_t, uint32_t>();

    qRegisterMetaType<Sections>("Sections");
    qScriptRegisterSequenceMetaType<Sections>(m_script_engine);

    qRegisterMetaType<Devices>("Devices");
    qScriptRegisterSequenceMetaType<Devices>(m_script_engine);

    qRegisterMetaType<DeviceItem*>("DeviceItem*");
    qRegisterMetaType<DeviceItems>("DeviceItems");
    qScriptRegisterSequenceMetaType<DeviceItems>(m_script_engine);

    qRegisterMetaType<QVector<DeviceItem*>>("QVector<DeviceItem*>"); // REVIEW а разве не тоже самое, что и  qRegisterMetaType<DeviceItems>("DeviceItems");
    qScriptRegisterSequenceMetaType<QVector<DeviceItem*>>(m_script_engine);

    qRegisterMetaType<QVector<ItemGroup*>>("QVector<ItemGroup*>");
    qScriptRegisterSequenceMetaType<QVector<ItemGroup*>>(m_script_engine);

    qRegisterMetaType<AutomationHelperItem*>("AutomationHelperItem*");

//    qScriptRegisterMetaType<SectionPtr>(eng, sharedPtrToScriptValue<SectionPtr>, emptyFromScriptValue<SectionPtr>);
    qScriptRegisterMetaType<std::string>(m_script_engine, stdstringToScriptValue, emptyFromScriptValue<std::string>);
//    qScriptRegisterMetaType<DeviceItem*>(eng, )

//    qScriptRegisterMetaType<DeviceItem::ValueType>(eng, itemValueToScriptValue, itemValueFromScriptValue);
    //    qScriptRegisterSequenceMetaType<DeviceItem::ValueList>(eng);

    //    qScriptRegisterSequenceMetaType<DeviceItem::ValueList>(eng);

    auto paramClass = new ParamGroupClass(m_script_engine);
    m_script_engine->globalObject().setProperty("Params", paramClass->constructor());
//    eng->globalObject().setProperty("ParamElem", paramClass->constructor());
}

template<typename T>
void initTypes(QScriptValue& parent, const QString& prop_name, Database::Base_Type_Manager<T>& type_mng)
{
    QScriptValue types_obj = parent.property(prop_name);
    QString name;
    for (const T& type: type_mng.types())
    {
        name = type.name();
        if (!name.isEmpty())
        {
            types_obj.setProperty(name, type.id());
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

    QScriptValue api = get_api_obj();
    api.setProperty("mng", m_script_engine->newQObject(this));

    QScriptValue statuses = api.property("status");
    QScriptValue common_status = m_script_engine->newObject();
    api.setProperty("common_status", common_status);

    // Многие статусы могут пренадлежать одному типу группы.
    QString status_name;
    QScriptValue status_group_property;
    for (Status_Info& type: *status_mng_.get_types())
    {
        if (type.groupType_id)
        {
            status_name = group_type_mng_.name(type.groupType_id);
            status_group_property = statuses.property(status_name);
            if (!status_group_property.isObject())
            {
                status_group_property = m_script_engine->newObject();
                statuses.setProperty(status_name, status_group_property);
            }
            status_group_property.setProperty(type.name(), type.id());
        }
        else
        {
            common_status.setProperty(type.name(), type.id());
        }
    }

    QScriptValue types = api.property("type");
    initTypes(types, "item", item_type_mng_);
    initTypes(types, "group", group_type_mng_);
    initTypes(types, "mode", mode_type_mng_);
    initTypes(types, "param", param_mng_);

    QString func_name;
    for (const Code_Item& code_item: code_mng_.types())
    {
        if (code_item.id() < FUNC_COUNT)
            func_name = handler_full_name(code_item.id());
        else
        {
            func_name = "Group: ";
            for (const Item_Group_Type& group_type: group_type_mng_.types())
            {
                if (group_type.code_id == code_item.id())
                {
                    func_name += group_type.title();
                    break;
                }
            }
        }

        if (code_item.text.isEmpty())
            qCDebug(ScriptDetailLog) << code_item.id() << code_item.name() << "code empty for function:" << func_name;
        else
            check_error(func_name, m_script_engine->evaluate(code_item.text, func_name));
    }

    QScriptValue group_handler_obj = get_handler("changed", "group"), group_handler;
    for (const Item_Group_Type& type: group_type_mng_.types())
    {
        if (type.name().isEmpty())
            qCDebug(ScriptDetailLog) << "Group type" << type.id() << type.name() << "havent latin name";
        else
        {
            group_handler = group_handler_obj.property(type.name());
            cache_handler_.emplace(FUNC_COUNT + type.id(), group_handler);
            if (!group_handler.isFunction())
            {
                qCDebug(ScriptDetailLog) << "Group type" << type.id() << type.name() << "havent 'changed' function" << group_handler_obj.toString();
            }
        }
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
        qCDebug(ScriptDetailLog) << "Load" << length << "script checkers";
    }
}

Section *ScriptedProject::add_section(Section&& section)
{
    auto sct = Project::add_section(std::move(section));
    connect(sct, &Section::groupInitialized, this, &ScriptedProject::groupInitialized);

    callFunction(FUNC_INIT_SECTION, { m_script_engine->newQObject(sct) });
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
    case QVariant::DateTime:            return m_script_engine->newDate(data.toDateTime());
    case QVariant::RegExp:
    case QVariant::RegularExpression:   return m_script_engine->newRegExp(data.toRegExp());
    default:
        return  m_script_engine->newVariant(data);
    }
}

void ScriptedProject::log(const QString &msg, uint8_t type_id, uint32_t user_id, bool inform_flag)
{
    if (inform_flag)
        type_id |= Log_Event_Item::EVENT_NEED_TO_INFORM;
    Log_Event_Item event{0, 0, user_id, type_id, ScriptLog().categoryName(), msg};
    std::cerr << "[script] " << event.msg().toStdString() << std::endl;
    add_event_message(event);
}

void ScriptedProject::console(uint32_t user_id, const QString &cmd, bool is_function, const QVariantList& arguments)
{
    if (is_function)
    {
        QScriptValueList arg_list;
        for (const QVariant& arg: arguments)
            arg_list.push_back(m_script_engine->newVariant(arg));

        QScriptValue func = m_script_engine->currentContext()->activationObject().property(cmd);
        QScriptValue ret = func.call(QScriptValue(), arg_list);
        check_error( "exec", ret);
        return;
    }

    QString script = cmd.trimmed();
    if (script.isEmpty() || !m_script_engine->canEvaluate(script))
        return;

    QScriptValue res = m_script_engine->evaluate(script, "CONSOLE");

    bool is_error = res.isError();

    if (!res.isUndefined())
    {
        if (!res.isError() && (res.isObject() || res.isArray()))
        {
            QScriptValue check_func = m_script_engine->evaluate(
                "(function(key, value) {"
                "  if ((typeof value === 'object' || typeof value === 'function') && "
                "      value !== null && !Array.isArray(value) && value.toString() !== '[object Object]') {"
                "    return value.toString();"
                "  }"
                "  return value;"
                "})");
            res = m_script_engine->evaluate("JSON.stringify").call(QScriptValue(), {res, check_func, ' '});
        }

        if (res.isError())
        {
            res = res.property("message");
            is_error = true;
        }
    }
    Log_Event_Item event{0, 0, user_id, is_error ? QtCriticalMsg : QtInfoMsg, ScriptLog().categoryName(), "CONSOLE [" + script + "] >" + res.toString()};
    std::cerr << event.msg().toStdString() << std::endl;
    add_event_message(event);
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

QScriptValue ScriptedProject::callFunction(int handler_type, const QScriptValueList& args) const
{
    QScriptValue handler = get_handler(handler_type);
    if (handler.isFunction())
    {
        QScriptValue ret = handler.call(QScriptValue(), args);
        check_error( handler_type, ret);
        return ret;
    }

    return QScriptValue();
}

void ScriptedProject::groupModeChanged(uint32_t user_id, uint32_t mode, uint32_t /*group_id*/)
{
    auto group = static_cast<ItemGroup*>(sender());
    if (!group)
        return;
    QScriptValue groupObj = m_script_engine->newQObject(group);

    callFunction(FUNC_CHANGED_MODE, { groupObj, mode, user_id });
    callFunction(FUNC_COUNT + group->type_id(), { groupObj, QScriptValue(), user_id });
}

void ScriptedProject::group_param_changed(Params, uint32_t user_id)
{
    auto group = static_cast<ItemGroup*>(sender());
    if (!group)
        return;

    callFunction(FUNC_COUNT + group->type_id(), { m_script_engine->newQObject(group), QScriptValue(), user_id });
}

QElapsedTimer t;
void ScriptedProject::itemChanged(DeviceItem *item, uint32_t user_id)
{
    auto group = static_cast<ItemGroup*>(sender());
    if (!group)
        return;

    QScriptValue groupObj = m_script_engine->newQObject(group);
    QScriptValue itemObj = m_script_engine->newQObject(item);

    t.restart();
    callFunction(FUNC_CHANGED_ITEM, { groupObj, itemObj, user_id });

    if (item->isControl())
        callFunction(FUNC_CHANGED_CONTROL, { groupObj, itemObj, user_id });
    else
        callFunction(FUNC_CHANGED_SENSOR, { groupObj, itemObj, user_id });

    callFunction(FUNC_COUNT + group->type_id(), { groupObj, itemObj, user_id });

//    eng->collectGarbage();

    if (t.elapsed() > 500)
        qCWarning(ScriptEngineLog) << "itemChanged timeout" << t.elapsed();

    t.invalidate();
}

void ScriptedProject::afterAllInitialization()
{
    callFunction(FUNC_AFTER_ALL_INITIALIZATION);
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
    QString error;
    int exit_code = 0;
    QVariantMap result;
    if (allow_shell_)
    {
        QStringList args_list;
        for (const QVariant& arg: args)
            args_list.push_back(arg.toString());

        QProcess proc;
        proc.start(programm, args_list);
        if (proc.waitForStarted(timeout_msec) && proc.waitForFinished(timeout_msec) && proc.bytesAvailable())
            result["output"] = QString::fromUtf8(proc.readAllStandardOutput());

        exit_code = proc.exitCode();
        error = QString::fromUtf8(proc.readAllStandardError());
    }

    result["code"] = exit_code;
    result["error"] = error;
    return result;
}

void ScriptedProject::groupInitialized(ItemGroup* group)
{
    QString group_type_name = group_type_mng_.name(group->type_id()),
            func_name = "api.handlers.group.initialized." + group_type_name;
    QScriptValue group_handler_obj = get_handler("initialized", "group");
    QScriptValue group_handler_init = group_handler_obj.property(group_type_name);

    if (group_handler_init.isFunction())
    {
        auto ret = group_handler_init.call(QScriptValue(), QScriptValueList{ m_script_engine->newQObject(group) });
        check_error( func_name, ret);
    }
    else
        qCDebug(ScriptDetailLog) << "Group type" << group->type_id() << group_type_name << "havent init function" << func_name;
}

QVariant ScriptedProject::normalize(const QVariant &val)
{
    return callFunction(FUNC_NORMALIZE, { m_script_engine->newQObject(sender()), valueFromVariant(val) }).toVariant();
}

bool ScriptedProject::controlChangeCheck(DeviceItem *item, const QVariant &raw_data, uint32_t user_id)
{
    auto ret = callFunction(FUNC_CONTROL_CHANGE_CHECK, { m_script_engine->newQObject(sender()), m_script_engine->newQObject(item), valueFromVariant(raw_data), user_id });
    return ret.isBool() && ret.toBool();
}

void ScriptedProject::handlerException(const QScriptValue &exception)
{
    if (m_script_engine->hasUncaughtException())
        qCCritical(ScriptEngineLog) << exception.toString() << m_script_engine->uncaughtExceptionBacktrace();
}

QString ScriptedProject::handler_full_name(int handler_type) const
{
    QPair<QString,QString> names;
    if (handler_type > FUNC_COUNT)
    {
        handler_type -= FUNC_COUNT;
        names.first = "group";
        names.second = "changed." + group_type_mng_.name(handler_type);
    }
    else
        names = handler_name(handler_type);
    QString full_name = "api.handlers.";
    if (!names.first.isEmpty())
        full_name += names.first + '.';
    full_name += names.second;
    return full_name;
}

QPair<QString,QString> ScriptedProject::handler_name(int handler_type) const
{
    static const char* changed = "changed";
    static const char* section = "section";
    static const char* database = "database";
    static const char* initialized = "initialized";

    switch (static_cast<Handler_Type>(handler_type))
    {
    case FUNC_INIT_SECTION:                 return {section, initialized};
    case FUNC_AFTER_ALL_INITIALIZATION:     return {{}, initialized};
    case FUNC_CHANGED_MODE:                 return {changed, "mode"};
    case FUNC_CHANGED_ITEM:                 return {changed, "item"};
    case FUNC_CHANGED_SENSOR:               return {changed, "sensor"};
    case FUNC_CHANGED_CONTROL:              return {changed, "control"};
    case FUNC_CHANGED_DAY_PART:             return {changed, "day_part"};
    case FUNC_CONTROL_CHANGE_CHECK:         return {{}, "control_change_check"};
    case FUNC_NORMALIZE:                    return {{}, "normalize"};
    case FUNC_AFTER_DATABASE_INIT:          return {database, initialized};
    case FUNC_CHECK_VALUE:                  return {{}, "check_value"};
    case FUNC_GROUP_STATUS:                 return {{}, "group_status"};
    default:
        break;
    }
    return {};
}

QScriptValue ScriptedProject::get_api_obj() const
{
    return m_script_engine->currentContext()->activationObject().property("api");
}

QScriptValue ScriptedProject::get_handler(int handler_type) const
{
    auto it = cache_handler_.find(handler_type);
    if (it != cache_handler_.cend())
    {
        return it->second;
    }
    auto name_pair = handler_name(handler_type);
    QScriptValue handler = get_handler(name_pair.second, name_pair.first);
    cache_handler_.emplace(handler_type, handler);
    if (!handler.isFunction())
    {
        qCDebug(ScriptDetailLog) << "Can not find:" << handler_full_name(handler_type);
    }
    return handler;
}

QScriptValue ScriptedProject::get_handler(const QString& name, const QString& parent_name) const
{
    QScriptValue obj = get_api_obj().property("handlers");
    if (!parent_name.isEmpty())
    {
        obj = obj.property(parent_name);
        if (!obj.isValid())
        {
            qCDebug(ScriptDetailLog) << "Can not find property" << parent_name << "in api.handlers";
            return obj;
        }
    }
    return obj.property(name);
}

void ScriptedProject::check_error(int handler_type, const QScriptValue& result) const
{
    if (result.isError())
        check_error(handler_full_name(handler_type), result);
}

void ScriptedProject::check_error(const QString& str, const QScriptValue& result) const
{
    if (result.isError())
    {
        qCCritical(ScriptEngineLog) << QString::fromLatin1("%0 %1(%2): %3")
                        .arg(str)
                        .arg(result.property("fileName").toString())
                        .arg(result.property("lineNumber").toInt32())
                        .arg(result.toString());
    }
}

void ScriptedProject::check_error(const QString &name, const QString &code) const
{
    check_error(name, m_script_engine->evaluate(code, name));
}

} // namespace Dai
