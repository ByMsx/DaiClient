#include <QTimer>
#include <QElapsedTimer>

#include <QFile>
#include <QTextStream>
#include <QDirIterator>
#include <QDebug>
#include <QMetaEnum>
#include <QProcess>

#ifdef QT_DEBUG
#include <QScriptEngineDebugger>
#include <QMainWindow>
#endif

#include <Helpz/consolereader.h>

#include "scriptedproject.h"
#include "paramgroupclass.h"

#include "tools/automationhelper.h"
#include "tools/pidhelper.h"
#include "tools/severaltimeshelper.h"
#include "tools/inforegisterhelper.h"

#include "worker.h"

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
void Scripted_Project::add_type()
{
    auto ctor = [](QScriptContext* ctx, QScriptEngine* script_engine_) -> QScriptValue {
        if ((unsigned)ctx->argumentCount() >= sizeof...(Args))
        {
            ArgumentGetter get_arg(ctx);

            auto obj = new T(qscriptvalue_cast<Args>(get_arg())...);
            return script_engine_->newQObject(obj, QScriptEngine::ScriptOwnership);
        }
        return P<T>()(ctx, script_engine_);
    };

    auto value = script_engine_->newQMetaObject(&T::staticMetaObject,
                                     script_engine_->newFunction(ctor));

    QString name = T::staticMetaObject.className();

    int four_dots = name.lastIndexOf("::");
    if (four_dots >= 0)
        name.remove(0, four_dots + 2);

    script_engine_->globalObject().setProperty(name, value);

//    qCDebug(ProjectLog) << "Register for script:" << name;
}

Scripted_Project::Scripted_Project(Worker* worker, Helpz::ConsoleReader *consoleReader, const QString &sshHost, bool allow_shell) :
    Project(),
    day_time_(this),
    uptime_(QDateTime::currentMSecsSinceEpoch()),
    allow_shell_(allow_shell), ssh_host_(sshHost)
#ifdef QT_DEBUG
    , debugger_(nullptr)
#endif
{
    register_types();

    script_engine_->pushContext();
    reinitialization(worker->database_info());

    set_ssh_host(sshHost);

    using T = Scripted_Project;
    connect(this, &T::mode_changed, worker, &Worker::mode_changed, Qt::QueuedConnection);
    connect(this, &T::status_added, worker, &Worker::status_added, Qt::QueuedConnection);
    connect(this, &T::status_removed, worker, &Worker::status_removed, Qt::QueuedConnection);
    connect(this, &T::sct_item_changed, worker, &Worker::new_value, Qt::QueuedConnection);
    connect(this, &T::add_event_message, worker, &Worker::add_event_message, Qt::QueuedConnection);
//    connect(worker, &Worker::dumpSectionsInfo, this, &T::dumpInfo, Qt::BlockingQueuedConnection);

    connect(this, &Scripted_Project::day_time_changed, &day_time_, &DayTimeHelper::init, Qt::QueuedConnection);
    connect(this, &T::sct_connection_state_change, worker, &Worker::connection_state_changed, Qt::QueuedConnection);

    if (consoleReader)
    {
        connect(consoleReader, &Helpz::ConsoleReader::textReceived, [this](const QString& text)
        {
            QMetaObject::invokeMethod(this, "console", Qt::QueuedConnection, Q_ARG(uint32_t, 0), Q_ARG(QString, text));
        });
    }
}

Scripted_Project::~Scripted_Project()
{
#ifdef QT_DEBUG
    if (debugger_)
    {
        debugger_->standardWindow()->close();
        debugger_->detach();
        delete debugger_;
    }
#endif
//    delete db();
}

void Scripted_Project::set_ssh_host(const QString &value) { if (ssh_host_ != value) ssh_host_ = value; }

qint64 Scripted_Project::uptime() const { return uptime_; }

void Scripted_Project::reinitialization(const Helpz::Database::Connection_Info& db_info)
{
    script_engine_->popContext();
    /*QScriptContext* ctx = */script_engine_->pushContext();

    std::unique_ptr<Database::Helper> db(new Database::Helper(db_info, "ProjectManager_" + QString::number((quintptr)this)));
    db->fill_types(this);
    scripts_initialization();

    day_time_.stop();
    qScriptDisconnect(&day_time_, SIGNAL(onDayPartChanged(Section*,bool)), QScriptValue(), QScriptValue());
    day_time_.disconnect(SIGNAL(onDayPartChanged(Section*,bool)));

    // qScriptConnect(&m_dayTime, SIGNAL(onDayPartChanged(Section*,bool)), QScriptValue(), m_func.at(FUNC_CHANGED_DAY_PART));
    connect(&day_time_, &DayTimeHelper::onDayPartChanged, [this](Section* sct, bool is_day)
    {
        call_function(FUNC_CHANGED_DAY_PART, { script_engine_->newQObject(sct), is_day });
    });

    db->init_project(this, true);

    if (get_handler(FUNC_CHANGED_DAY_PART).isFunction())
        day_time_.init();

    for(Device* dev: devices())
        for (DeviceItem* dev_item: dev->items())
                if (dev_item->need_normalize())
                    connect(dev_item, &DeviceItem::normalize, this, &Scripted_Project::normalize);

    for(Section* sct: sections())
    {
        for (ItemGroup* group: sct->groups())
        {
            connect(group, &ItemGroup::item_changed, this, &Scripted_Project::item_changed);
            connect(group, &ItemGroup::item_changed, this, &Scripted_Project::sct_item_changed);
            connect(group, &ItemGroup::mode_changed, this, &Scripted_Project::group_mode_changed);
            connect(group, &ItemGroup::param_changed, this, &Scripted_Project::group_param_changed);
            connect(group, &ItemGroup::status_added, this, &Scripted_Project::status_added);
            connect(group, &ItemGroup::status_removed, this, &Scripted_Project::status_removed);

            connect(group, &ItemGroup::connection_state_change, this, &Scripted_Project::sct_connection_state_change);

            if (get_handler(FUNC_CONTROL_CHANGE_CHECK).isFunction())
                connect(group, &ItemGroup::control_change_check, this, &Scripted_Project::control_change_check);
        }
    }

    call_function(FUNC_AFTER_DATABASE_INIT);
}

void Scripted_Project::register_types()
{
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<Section*>("Section*");
    qRegisterMetaType<Sections*>("Sections*");
    qRegisterMetaType<ItemGroup*>("ItemGroup*");

    qRegisterMetaType<AutomationHelper*>("AutomationHelper*");

    script_engine_ = new QScriptEngine(this);
#ifdef QT_DEBUG
    if (qApp->thread() == thread())
    {
        debugger_ = new QScriptEngineDebugger(this);
        debugger_->attachTo(script_engine_);
        debugger_->standardWindow()->show();
    }
#endif

    connect(script_engine_, &QScriptEngine::signalHandlerException,
            this, &Scripted_Project::handler_exception);

    add_type<QTimer>();
    add_type<QProcess>();

    add_type<AutomationHelper, Type_Default, uint>();

    add_type_n<AutomationHelperItem, ItemGroup*>();
    add_type_n<SeveralTimesHelper, ItemGroup*>();
    add_type_n<PIDHelper, ItemGroup*, uint>();
    add_type_n<InfoRegisterHelper, ItemGroup*, uint, uint>();

    add_type_n<Section, uint32_t, QString, uint32_t, uint32_t>();
    add_type_n<ItemGroup, uint32_t, QString, uint32_t, uint32_t, uint32_t>();

    qRegisterMetaType<Sections>("Sections");
    qScriptRegisterSequenceMetaType<Sections>(script_engine_);

    qRegisterMetaType<Devices>("Devices");
    qScriptRegisterSequenceMetaType<Devices>(script_engine_);

    qRegisterMetaType<DeviceItem*>("DeviceItem*");
    qRegisterMetaType<DeviceItems>("DeviceItems");
    qScriptRegisterSequenceMetaType<DeviceItems>(script_engine_);

    qRegisterMetaType<QVector<DeviceItem*>>("QVector<DeviceItem*>"); // REVIEW а разве не тоже самое, что и  qRegisterMetaType<DeviceItems>("DeviceItems");
    qScriptRegisterSequenceMetaType<QVector<DeviceItem*>>(script_engine_);

    qRegisterMetaType<QVector<ItemGroup*>>("QVector<ItemGroup*>");
    qScriptRegisterSequenceMetaType<QVector<ItemGroup*>>(script_engine_);

    qRegisterMetaType<AutomationHelperItem*>("AutomationHelperItem*");

//    qScriptRegisterMetaType<SectionPtr>(eng, sharedPtrToScriptValue<SectionPtr>, emptyFromScriptValue<SectionPtr>);
    qScriptRegisterMetaType<std::string>(script_engine_, stdstringToScriptValue, emptyFromScriptValue<std::string>);
//    qScriptRegisterMetaType<DeviceItem*>(eng, )

//    qScriptRegisterMetaType<DeviceItem::ValueType>(eng, itemValueToScriptValue, itemValueFromScriptValue);
    //    qScriptRegisterSequenceMetaType<DeviceItem::ValueList>(eng);

    //    qScriptRegisterSequenceMetaType<DeviceItem::ValueList>(eng);

    auto paramClass = new ParamGroupClass(script_engine_);
    script_engine_->globalObject().setProperty("Params", paramClass->constructor());

//    eng->globalObject().setProperty("ParamElem", paramClass->constructor());
}

template<typename T>
void init_types(QScriptValue& parent, const QString& prop_name, Database::Base_Type_Manager<T>& type_mng)
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

void Scripted_Project::scripts_initialization()
{
    auto read_script_file = [this](const QString& file_base, const QString& name = QString(), const QString& base_path = ":/Scripts/js")
    {
        QFile script_file(base_path + '/' + file_base + ".js");
        if (script_file.exists())
        {
            script_file.open(QIODevice::ReadOnly | QFile::Text);
            QTextStream stream(&script_file);
            check_error(name.isEmpty() ? file_base : name, stream.readAll());
            script_file.close();
        }
    };

    read_script_file("api", "API");

    QScriptValue api = get_api_obj();
    api.setProperty("mng", script_engine_->newQObject(this));

    QScriptValue statuses = api.property("status");
    QScriptValue common_status = script_engine_->newObject();
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
                status_group_property = script_engine_->newObject();
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
    init_types(types, "item", item_type_mng_);
    init_types(types, "group", group_type_mng_);
    init_types(types, "mode", mode_type_mng_);
    init_types(types, "param", param_mng_);

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
            check_error(func_name, script_engine_->evaluate(code_item.text, func_name));
    }

    QScriptValue group_handler_obj = get_handler("changed", "group"), group_handler;
    for (const Item_Group_Type& type: group_type_mng_.types())
    {
        if (type.name().isEmpty())
            qCDebug(ScriptDetailLog) << "Group type" << type.id() << type.name() << "havent latin name";
        else
        {
            group_handler = group_handler_obj.property(type.name());
            if (group_handler.isFunction())
                cache_handler_.emplace(FUNC_COUNT + type.id(), group_handler);
            else
                qCDebug(ScriptDetailLog) << "Group type" << type.id() << type.name() << "havent 'changed' function" << group_handler_obj.toString();
        }
    }

    QDir scripts(qApp->applicationDirPath() + "/scripts/");
    QFileInfoList js_list = scripts.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo& fileInfo: js_list)
        if (fileInfo.suffix() == "js" && fileInfo.baseName() != "api")
            read_script_file( fileInfo.baseName(), QString(), fileInfo.absolutePath() );

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

Section *Scripted_Project::add_section(Section&& section)
{
    auto sct = Project::add_section(std::move(section));
    connect(sct, &Section::group_initialized, this, &Scripted_Project::group_initialized);

    call_function(FUNC_INIT_SECTION, { script_engine_->newQObject(sct) });
    return sct;
}

QScriptValue Scripted_Project::value_from_variant(const QVariant &data) const
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
    case QVariant::DateTime:            return script_engine_->newDate(data.toDateTime());
    case QVariant::RegExp:
    case QVariant::RegularExpression:   return script_engine_->newRegExp(data.toRegExp());
    default:
        return  script_engine_->newVariant(data);
    }
}

bool Scripted_Project::stop(uint32_t user_id)
{
    QScriptValue can_restart = get_api_obj().property("handlers").property("can_restart");
    if (can_restart.isFunction())
    {
        QScriptValue ret = can_restart.call(QScriptValue(), QScriptValueList{user_id});
        if (ret.isBool() && !ret.toBool())
        {
            return false;
        }
    }

    checker_stop();
    return true;
}

QStringList Scripted_Project::backtrace() const
{
    return script_engine_->currentContext()->backtrace();
}

void Scripted_Project::log(const QString &msg, uint8_t type_id, uint32_t user_id, bool inform_flag, bool print_backtrace)
{
    Log_Event_Item event{ QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), user_id, inform_flag, type_id, ScriptLog().categoryName(), msg };
    if (print_backtrace)
        event.set_text(event.text() + "\n\n" + script_engine_->currentContext()->backtrace().join('\n'));
    std::cerr << "[script] " << event.text().toStdString() << std::endl;
    add_event_message(std::move(event));
}

void Scripted_Project::console(uint32_t user_id, const QString &cmd, bool is_function, const QVariantList& arguments)
{
    if (is_function)
    {
        QScriptValueList arg_list;
        for (const QVariant& arg: arguments)
            arg_list.push_back(script_engine_->newVariant(arg));

        QScriptValue func = script_engine_->currentContext()->activationObject().property(cmd);
        QScriptValue ret = func.call(QScriptValue(), arg_list);
        check_error( "exec", ret);
        return;
    }

    QString script = cmd.trimmed();
    if (script.isEmpty() || !script_engine_->canEvaluate(script))
        return;

    QScriptValue res = script_engine_->evaluate(script, "CONSOLE");

    bool is_error = res.isError();

    if (!res.isUndefined())
    {
        if (!res.isError() && (res.isObject() || res.isArray()))
        {
            QScriptValue check_func = script_engine_->evaluate(
                "(function(key, value) {"
                "  if ((typeof value === 'object' || typeof value === 'function') && "
                "      value !== null && !Array.isArray(value) && value.toString() !== '[object Object]') {"
                "    return value.toString();"
                "  }"
                "  return value;"
                "})");
            res = script_engine_->evaluate("JSON.stringify").call(QScriptValue(), {res, check_func, ' '});
        }

        if (res.isError())
        {
            res = res.property("message");
            is_error = true;
        }
    }
    Log_Event_Item event{ QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), user_id, false, is_error ? QtCriticalMsg : QtInfoMsg,
                ScriptLog().categoryName(), "CONSOLE [" + script + "] >" + res.toString() };
    std::cerr << event.text().toStdString() << std::endl;
    add_event_message(std::move(event));
}

QScriptValue Scripted_Project::call_function(int handler_type, const QScriptValueList& args) const
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

void Scripted_Project::group_mode_changed(uint32_t user_id, uint32_t mode, uint32_t /*group_id*/)
{
    auto group = static_cast<ItemGroup*>(sender());
    if (!group)
        return;
    QScriptValue groupObj = script_engine_->newQObject(group);

    call_function(FUNC_CHANGED_MODE, { groupObj, mode, user_id });
    call_function(FUNC_COUNT + group->type_id(), { groupObj, QScriptValue(), user_id });
}

void Scripted_Project::group_param_changed(Param* /*param*/, uint32_t user_id)
{
    auto group = static_cast<ItemGroup*>(sender());
    if (!group)
        return;

    call_function(FUNC_COUNT + group->type_id(), { script_engine_->newQObject(group), QScriptValue(), user_id });
}

QElapsedTimer t;
void Scripted_Project::item_changed(DeviceItem *item, uint32_t user_id, const QVariant& old_raw_value)
{
    auto group = static_cast<ItemGroup*>(sender());
    if (!group)
        return;

    QScriptValue groupObj = script_engine_->newQObject(group);
    QScriptValue itemObj = script_engine_->newQObject(item);

    QScriptValueList args { groupObj, itemObj, user_id, value_from_variant(old_raw_value) };

    t.restart();
    call_function(FUNC_CHANGED_ITEM, args);

    if (item->is_control())
        call_function(FUNC_CHANGED_CONTROL, args);
    else
        call_function(FUNC_CHANGED_SENSOR, args);

    call_function(FUNC_COUNT + group->type_id(), args);

//    eng->collectGarbage();

    if (t.elapsed() > 500)
        qCWarning(ScriptEngineLog) << "itemChanged timeout" << t.elapsed();

    t.invalidate();
}

void Scripted_Project::after_all_initialization()
{
    call_function(FUNC_AFTER_ALL_INITIALIZATION);
}

void Scripted_Project::ssh(quint16 port, quint32 remote_port)
{
    if (pid > 0)
        QProcess::startDetached("kill", { QString::number(pid) });
    // ssh -o StrictHostKeyChecking=no -N -R 0.0.0.0:12345:localhost:22 -p 15666 root@deviceaccess.ru
    QStringList args{"-o", "StrictHostKeyChecking=no", "-N", "-R", QString("0.0.0.0:%1:localhost:22").arg(remote_port), "-p", QString::number(port), "root@" + ssh_host_ };
    QProcess::startDetached("ssh", args, QString(), &pid);

    qCInfo(ProjectLog) << "SSH Client started" << pid;
}

QVariantMap Scripted_Project::run_command(const QString &programm, const QVariantList &args, int timeout_msec) const
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

void Scripted_Project::connect_group_is_can_change(ItemGroup* group, const QScriptValue& obj, const QScriptValue& func)
{
    if (group && func.isFunction())
    {
        connect(group, &ItemGroup::is_can_change, [this, obj, func](DeviceItem* item, const QVariant& raw_data, uint32_t user_id) -> bool
        {
            QScriptValue f = func;
            QScriptValue res = f.call(obj, QScriptValueList{ script_engine_->newQObject(item), value_from_variant(raw_data), user_id });
            return res.toBool();
        });
    }
}

void Scripted_Project::group_initialized(ItemGroup* group)
{
    QString group_type_name = group_type_mng_.name(group->type_id()),
            func_name = "api.handlers.group.initialized." + group_type_name;
    QScriptValue group_handler_obj = get_handler("initialized", "group");
    QScriptValue group_handler_init = group_handler_obj.property(group_type_name);

    if (group_handler_init.isFunction())
    {
        auto ret = group_handler_init.call(QScriptValue(), QScriptValueList{ script_engine_->newQObject(group) });
        check_error( func_name, ret);
    }
    else
        qCDebug(ScriptDetailLog) << "Group type" << group->type_id() << group_type_name << "havent init function" << func_name;
}

QVariant Scripted_Project::normalize(const QVariant &val)
{
    return call_function(FUNC_NORMALIZE, { script_engine_->newQObject(sender()), value_from_variant(val) }).toVariant();
}

bool Scripted_Project::control_change_check(DeviceItem *item, const QVariant &raw_data, uint32_t user_id)
{
    auto ret = call_function(FUNC_CONTROL_CHANGE_CHECK, { script_engine_->newQObject(sender()), script_engine_->newQObject(item), value_from_variant(raw_data), user_id });
    return ret.isBool() && ret.toBool();
}

void Scripted_Project::handler_exception(const QScriptValue &exception)
{
    if (script_engine_->hasUncaughtException())
        qCCritical(ScriptEngineLog) << exception.toString() << script_engine_->uncaughtExceptionBacktrace();
}

QString Scripted_Project::handler_full_name(int handler_type) const
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

QPair<QString,QString> Scripted_Project::handler_name(int handler_type) const
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

QScriptValue Scripted_Project::get_api_obj() const
{
    return script_engine_->currentContext()->activationObject().property("api");
}

QScriptValue Scripted_Project::get_handler(int handler_type) const
{
    auto it = cache_handler_.find(handler_type);
    if (it != cache_handler_.cend())
    {
        return it->second;
    }
    auto name_pair = handler_name(handler_type);
    QScriptValue handler = get_handler(name_pair.second, name_pair.first);
    if (handler.isFunction())
        cache_handler_.emplace(handler_type, handler);
    else
        qCDebug(ScriptDetailLog) << "Can not find:" << handler_full_name(handler_type);
    return handler;
}

QScriptValue Scripted_Project::get_handler(const QString& name, const QString& parent_name) const
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

void Scripted_Project::check_error(int handler_type, const QScriptValue& result) const
{
    if (result.isError())
    {
        check_error(handler_full_name(handler_type), result);
        cache_handler_.erase(handler_type);
    }
}

void Scripted_Project::check_error(const QString& str, const QScriptValue& result) const
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

void Scripted_Project::check_error(const QString &name, const QString &code) const
{
    check_error(name, script_engine_->evaluate(code, name));
}

} // namespace Dai
