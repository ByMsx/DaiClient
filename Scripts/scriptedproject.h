#ifndef DAI_Scripted_Project_H
#define DAI_Scripted_Project_H

#include <QScriptEngine>
#include <QtSerialBus/qmodbusdataunit.h>
//#include <QQmlEngine>

#include <Helpz/simplethread.h>
#include <Helpz/db_connection_info.h>

#include <Dai/project.h>
#include <Dai/log/log_pack.h>
#include <Dai/db/group_status_item.h>

#include "tools/daytimehelper.h"
#include "tools/automationhelper.h"

QT_FORWARD_DECLARE_CLASS(QScriptEngineDebugger)

class QScriptEngine;

namespace Helpz {
class ConsoleReader;
}

namespace Dai {

class Worker;

class AutomationHelper;
class DayTimeHelper;

class Scripted_Project final : public Project
{
    Q_OBJECT
    Q_PROPERTY(qint64 uptime READ uptime)
public:
    enum Handler_Type {
        FUNC_UNKNOWN = 0,
        FUNC_OTHER_SCRIPTS,
        FUNC_INIT_SECTION,
        FUNC_AFTER_ALL_INITIALIZATION,
        FUNC_CHANGED_MODE,
        FUNC_CHANGED_ITEM,
        FUNC_CHANGED_SENSOR,
        FUNC_CHANGED_CONTROL,
        FUNC_CHANGED_DAY_PART,
        FUNC_CONTROL_CHANGE_CHECK,
        FUNC_NORMALIZE,
        FUNC_AFTER_DATABASE_INIT,
        FUNC_CHECK_VALUE,
        FUNC_GROUP_STATUS,

        FUNC_COUNT
    };
    Q_ENUM(Handler_Type)

    Scripted_Project(Worker* worker, Helpz::ConsoleReader* consoleReader, const QString &sshHost, bool allow_shell);
    ~Scripted_Project();

    void set_ssh_host(const QString &value);

    qint64 uptime() const;
    Section *add_section(Section&& section) override;

    QScriptValue value_from_variant(const QVariant& data) const;
signals:
    void log_item_available(const Log_Value_Item& log_value_item);
    void sct_connection_state_change(DeviceItem*, bool value);

    void status_added(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t user_id);
    void status_removed(quint32 group_id, quint32 info_id, uint32_t user_id);

    void checker_stop();
    void checker_start();

    void add_event_message(Log_Event_Item event);
//    QVariantList modbusRead(int serverAddress, uchar registerType = QModbusDataUnit::InputRegisters,
//                                                 int startAddress = 0, quint16 unitCount = 1);
//    void modbusWrite(int server, uchar registerType, int unit, quint16 state);

    void day_time_changed(/*Section* sct*/);
public slots:
    bool stop(uint32_t user_id = 0);

    QStringList backtrace() const;
    void log(const QString& msg, uint8_t type_id, uint32_t user_id = 0, bool inform_flag = false, bool print_backtrace = false);
    void console(uint32_t user_id, const QString& cmd, bool is_function = false, const QVariantList& arguments = {});
    void reinitialization(const Helpz::Database::Connection_Info &db_info);
    void after_all_initialization();

    void ssh(quint16 port = 22, quint32 remote_port = 25589);
    QVariantMap run_command(const QString& programm, const QVariantList& args = QVariantList(), int timeout_msec = 5000) const;

    void connect_group_is_can_change(ItemGroup *group, const QScriptValue& obj, const QScriptValue& func);
    void connect_item_raw_to_display(DeviceItem *item, const QScriptValue& obj, const QScriptValue& func);
    void connect_item_display_to_raw(DeviceItem *item, const QScriptValue& obj, const QScriptValue& func);

    QVector<Group_Status_Item> get_group_statuses() const;
private slots:
    void group_initialized(ItemGroup* group);
    QVariant normalize(const QVariant& val);
//    void dayTimeChanged(Section* sct);

    bool control_change_check(DeviceItem* item, const QVariant& raw_data, uint32_t user_id);

    void group_mode_changed(uint32_t user_id, uint32_t mode, uint32_t group_id);
    void group_param_changed(Param *param, uint32_t user_id = 0);
    void item_changed(DeviceItem* item, uint32_t user_id, const QVariant& old_raw_value);
    void handler_exception(const QScriptValue &exception);
private:
    QString handler_full_name(int handler_type) const;
    QPair<QString, QString> handler_name(int handler_type) const;
    QScriptValue get_api_obj() const;
    QScriptValue get_handler(int handler_type) const;
    QScriptValue get_handler(const QString& name, const QString& parent_name) const;
    void check_error(int handler_type, const QScriptValue &result) const;
    void check_error(const QString &str, const QScriptValue &result) const;
    void check_error(const QString &name, const QString &code) const;

    template<typename T>
    struct Type_Empty {
        QScriptValue operator() (QScriptContext*, QScriptEngine*) {
            return QScriptValue();
        }
    };
    template<typename T>
    struct Type_Default {
        QScriptValue operator() (QScriptContext*, QScriptEngine* eng) {
            return eng->newQObject(new T, QScriptEngine::ScriptOwnership);
        }
    };

    template<typename T, template<typename> class P = Type_Empty, typename... Args>
    void add_type();

    template<typename T, typename... Args>
    void add_type_n() { add_type<T, Type_Empty, Args...>(); }

    void register_types();
    void scripts_initialization();
    QScriptValue call_function(int handler_type, const QScriptValueList& args = QScriptValueList()) const;

    QScriptEngine *script_engine_;

    DayTimeHelper day_time_;

    qint64 uptime_;

    mutable std::map<uint32_t, QScriptValue> cache_handler_;

    bool allow_shell_;
    QString ssh_host_;
    qint64 pid = -1;

#ifdef QT_DEBUG
    QScriptEngineDebugger* debugger_;
#endif
};

} // namespace Dai

#endif // DAI_Scripted_Project_H
