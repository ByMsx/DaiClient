#ifndef DAI_CHECKER_H
#define DAI_CHECKER_H

#include <QThread>
#include <QTimer>
#include <QLoggingCategory>
#include <QMutexLocker>

#include <map>

#include <Helpz/simplethread.h>

#include "Dai/project.h"

namespace Dai {

Q_DECLARE_LOGGING_CATEGORY(CheckerLog)

class Worker;
typedef std::map<DeviceItem*, QVariant> ChangesList;

class Checker : public QObject
{
    Q_OBJECT
public:
    explicit Checker(Worker* worker, int interval = 1500, const QString& pluginstr = {}, QObject *parent = 0);
    ~Checker();
    void loadPlugins(const QStringList& allowed_plugins);

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

    Project* prj;
//    SerialPort::Manager sp_mng;

    std::map<DeviceItem*, QVariant> m_writeCache;


    bool b_break;

    std::shared_ptr<PluginTypeManager> PluginTypeMng;
//    friend class ModbusThread;
//    friend struct ModbusReadHelper;
};

} // namespace Dai

#endif // DAI_CHECKER_H
