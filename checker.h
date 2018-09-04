#ifndef GREENHOUSE_CHECKER_H
#define GREENHOUSE_CHECKER_H

#include <QThread>
#include <QTimer>
#include <QLoggingCategory>
#include <QMutexLocker>

#include <map>

#include <Helpz/simplethread.h>

#include "Dai/sectionmanager.h"

namespace Dai {

Q_DECLARE_LOGGING_CATEGORY(CheckerLog)

class Worker;
typedef std::map<DeviceItem*, QVariant> ChangesList;

class Checker : public QObject
{
    Q_OBJECT
public:
    explicit Checker(Worker* worker, int interval = 1500, QObject *parent = 0);
    ~Checker();
    void loadPlugins();

    void breakChecking();
public slots:
    void stop();
    void start();
private:
private slots:
    void checkDevices();
    void write_data(DeviceItem* item, const QVariant& raw_data);
    void writeCache();
private:
    void writeItem(DeviceItem* item, const QVariant& raw_data);

    QTimer check_timer, write_timer;

    SectionManager* sct_mng;
//    SerialPort::Manager sp_mng;

    std::map<DeviceItem*, QVariant> m_writeCache;


    bool b_break;

    std::shared_ptr<PluginTypeManager> PluginTypeMng;

//    friend class ModbusThread;
//    friend struct ModbusReadHelper;
};

/*class ModbusThread : public Helpz::ParamThread<Modbus, SectionManager*, SerialPort::Conf, int, int, int, int>
{
    void started() override
    {
        ptr()->checkDevices(); // Первый опрос контроллеров
        m_checkedcomplite = true;
    }
    bool m_checkedcomplite = false;
public:
    using Helpz::ParamThread<Modbus, SectionManager*, SerialPort::Conf, int, int, int, int>::ParamThread;
    bool firstChecked() const { return m_checkedcomplite; }
};*/

} // namespace Dai

#endif // GREENHOUSE_CHECKER_H
