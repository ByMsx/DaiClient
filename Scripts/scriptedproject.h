#ifndef SCRIPTEDPROJECT_H
#define SCRIPTEDPROJECT_H

#include <QScriptEngine>
#include <QtSerialBus/qmodbusdataunit.h>
//#include <QQmlEngine>

#include <Helpz/simplethread.h>
#include <Helpz/db_connectioninfo.h>

#include <Dai/project.h>

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

class ScriptedProject : public Project
{
    Q_OBJECT
    Q_PROPERTY(qint64 uptime READ uptime)
public:
    enum ScriptFunction {
        fZero = 0,
        fOtherScripts,
        fInitSection,
        fAfterAllInitialization,
        fModeChanged,
        fItemChanged,
        fSensorChanged,
        fControlChanged,
        fDayPartChanged,
        fControlChangeCheck,
        fNormalize,
        fAfterDatabaseInit,
        fCheckValue,
        fGroupStatus,
        fAutomation
    };
    Q_ENUM(ScriptFunction)

    ScriptedProject(Worker* worker, Helpz::ConsoleReader* consoleReader, const QString &sshHost, bool allow_shell);
    ~ScriptedProject();

    void setSSHHost(const QString &value);

    qint64 uptime() const;
    Section *addSection(quint32 id, const QString &name, const TimeRange &dayTime) override;

    QScriptValue valueFromVariant(const QVariant& data) const;
signals:
    void sctItemChanged(DeviceItem*);
    void groupStatusChanged(quint32 group_id, quint32 status);

    void modbusStop();
    void modbusStart();
//    QVariantList modbusRead(int serverAddress, uchar registerType = QModbusDataUnit::InputRegisters,
//                                                 int startAddress = 0, quint16 unitCount = 1);
//    void modbusWrite(int server, uchar registerType, int unit, quint16 state);

    void dayTimeChanged(/*Section* sct*/);
public slots:
    void log(const QString& msg, uint type);
    void console(const QString& cmd);
    void reinitialization(const Helpz::Database::ConnectionInfo &db_info);
    void afterAllInitialization();

    void ssh(quint16 port = 22, quint32 remote_port = 25589);
    QVariantMap run_command(const QString& programm, const QVariantList& args = QVariantList(), int timeout_msec = 5000) const;
private slots:
    void groupInitialized(ItemGroup* group);
    QVariant normalize(const QVariant& val);
//    void dayTimeChanged(Section* sct);

    bool controlChangeCheck(DeviceItem* item, const QVariant& raw_data);
    bool checkValue(DeviceItem* item) const;
    quint32 groupStatus(ItemGroup::ValueType val) const;

    void groupModeChanged(uint mode, quint32 group_id);
    void itemChanged(DeviceItem* item);
    void statusChanged(quint32 status);
    void handlerException(const QScriptValue &exception);
private:
    void run_automation(ItemGroup *group, const QScriptValue &groupObj, const QScriptValue& itemObj = QScriptValue());
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
    QScriptValue callFunction(uint func_idx, const QScriptValueList& args = QScriptValueList()) const;

    QScriptEngine *eng;

    std::vector<QScriptValue> m_func;

    std::map<uint, uint> m_automation;

    DayTimeHelper m_dayTime;

    qint64 m_uptime;

    bool allow_shell_;
    QString ssh_host;
    qint64 pid = -1;
};

} // namespace Dai

#endif // SCRIPTEDPROJECT_H
