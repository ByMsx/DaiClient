#ifndef SCRIPTEDPROJECT_H
#define SCRIPTEDPROJECT_H

#include <QScriptEngine>
#include <QtSerialBus/qmodbusdataunit.h>
//#include <QQmlEngine>

#include <Helpz/simplethread.h>
#include <Helpz/db_connection_info.h>

#include <Dai/project.h>
#include <Dai/log/log_pack.h>

#include "tools/daytimehelper.h"
#include "tools/automationhelper.h"

class QScriptEngine;

namespace Helpz {
class ConsoleReader;
}

namespace Dai {

class Worker;
class DBManager;

class AutomationHelper;
class DayTimeHelper;

class ScriptedProject final : public Project
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

    ScriptedProject(Worker* worker, Helpz::ConsoleReader* consoleReader, const QString &sshHost, bool allow_shell);
    ~ScriptedProject();

    void setSSHHost(const QString &value);

    qint64 uptime() const;
    Section *add_section(Section&& section) override;

    QScriptValue valueFromVariant(const QVariant& data) const;
signals:
    void sctItemChanged(DeviceItem*, uint32_t user_id);

    void status_added(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t user_id);
    void status_removed(quint32 group_id, quint32 info_id, uint32_t user_id);

    void modbusStop();
    void modbusStart();

    void add_event_message(const Log_Event_Item& event);
//    QVariantList modbusRead(int serverAddress, uchar registerType = QModbusDataUnit::InputRegisters,
//                                                 int startAddress = 0, quint16 unitCount = 1);
//    void modbusWrite(int server, uchar registerType, int unit, quint16 state);

    void dayTimeChanged(/*Section* sct*/);
public slots:
    void log(const QString& msg, uint type, uint32_t user_id = 0);
    void console(uint32_t user_id, const QString& cmd);
    void reinitialization(const Helpz::Database::Connection_Info &db_info);
    void afterAllInitialization();

    void ssh(quint16 port = 22, quint32 remote_port = 25589);
    QVariantMap run_command(const QString& programm, const QVariantList& args = QVariantList(), int timeout_msec = 5000) const;
private slots:
    void groupInitialized(ItemGroup* group);
    QVariant normalize(const QVariant& val);
//    void dayTimeChanged(Section* sct);

    bool controlChangeCheck(DeviceItem* item, const QVariant& raw_data, uint32_t user_id);
    bool checkValue(DeviceItem* item) const;

    void groupModeChanged(uint32_t user_id, uint32_t mode, uint32_t group_id);
    void itemChanged(DeviceItem* item, uint32_t user_id);
    void handlerException(const QScriptValue &exception);
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
    struct TypeEmpty {
        QScriptValue operator() (QScriptContext*, QScriptEngine*) {
            return QScriptValue();
        }
    };
    template<typename T>
    struct TypeDefault {
        QScriptValue operator() (QScriptContext*, QScriptEngine* eng) {
            return eng->newQObject(new T, QScriptEngine::ScriptOwnership);
        }
    };

    template<typename T, template<typename> class P = TypeEmpty, typename... Args>
    void addType();

    template<typename T, typename... Args>
    void addTypeN() { addType<T, TypeEmpty, Args...>(); }

    void registerTypes();
    void scriptsInitialization();
//    void evaluateFile(const QString &fileName);
    QScriptValue callFunction(int handler_type, const QScriptValueList& args = QScriptValueList()) const;

    QScriptEngine *m_script_engine;

    DayTimeHelper m_dayTime;

    qint64 m_uptime;

    mutable std::map<uint32_t, QScriptValue> cache_handler_;

    bool allow_shell_;
    QString ssh_host;
    qint64 pid = -1;
};

} // namespace Dai

#endif // SCRIPTEDPROJECT_H
