#ifndef WORKER_H
#define WORKER_H

#include <QTimer>
#include <QDBusVariant>

#include <Helpz/service.h>
#include <Helpz/settingshelper.h>

#include "DBus/dbus_param.h"
#include "DBus/message.h"

#include "Database/db_manager.h"
#include "checker.h"
#include "DBus/d_iface.h"
#include "Network/n_client.h"
#include "Scripts/scriptsectionmanager.h"

#include "../server/django/djangohelper.h"
#include "../server/websocket/websocket.h"

namespace Dai {
class Worker;
}

namespace dai {
class WebSockItem : public QObject, public dai::project::base
{
    Q_OBJECT
public:
    WebSockItem(Dai::Worker* obj);
    ~WebSockItem();

    // base interface
public:
    void send_value(quint32 item_id, const QVariant &raw_data) override;
    void send_mode(quint32 mode_id, quint32 group_id) override;
    void send_param_values(const QByteArray &msg_buff) override;
    void send_code(quint32 code_id, const QString &text) override;
    void send_script(const QString &script) override;
    void send_restart() override;
public slots:
    void modeChanged(uint mode_id, uint group_id);
private:
    Dai::Worker* w;
};
}

namespace Dai {

class Worker final : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = 0);
    ~Worker();

    static dai::DjangoHelper* django;
    static dai::Network::WebSocket* webSock;

    const Helpz::Database::ConnectionInfo& database_info() const;

    static std::unique_ptr<QSettings> settings();
private:
    void init_DBus(QSettings* s);
    void init_Database(QSettings *s);
    void init_SectionManager(QSettings* s);
    void init_Checker(QSettings* s);
    void init_GlobalClient(QSettings* s);
    void init_LogTimer(int period);

    static void* django_handle;
    void initDjangoLib(QSettings *s);

    static void* websock_handle;
    std::shared_ptr<dai::WebSockItem> websock_item;
    void initWebSocketManagerLib(QSettings *s);
    std::shared_ptr<dai::project::base> proj_in_team_by_id(uint32_t team_id, uint32_t proj_id);
signals:
    void serviceRestart();

    // D-BUS Signals
    void started();
    void change(const Dai::ValuePackItem& item, bool immediately);

    void modeChanged(uint mode_id, uint group_id);
    void groupStatusChanged(quint32 group_id, quint32 status);

    void paramValuesChanged(const ParamValuesPack& pack);


//    std::shared_ptr<Dai::Prt::ServerInfo> dumpSectionsInfo() const;
public slots:
    void logMessage(QtMsgType type, const Helpz::LogContext &ctx, const QString &str);
    void processCommands(const QStringList& args);

    QString getUserDevices();
    QString getUserStatus();

    void initDevice(const QString& device, const QString& device_name, const QString &device_latin, const QString& device_desc);
    void clearServerConfig();
    void saveServerAuthData(const QString& login, const QString& password);
    void saveServerData(const QUuid &devive_uuid, const QString& login, const QString& password);

    QByteArray sections();
    bool setDayTime(uint section_id, uint dayStartSecs, uint dayEndSecs);

    void setControlState(quint32 section_id, quint32 item_type, const Dai::Variant& raw_data);
    void writeToItem(quint32 item_id, const QVariant &raw_data);
    bool setMode(uint mode_id, quint32 group_id);
    bool setCode(const CodeItem &item);

    void setParamValues(const ParamValuesPack& pack);
    bool setSettings(quint16 cmd, QDataStream* msg);
//    bool setSettings(uchar stType, google::protobuf::Message* msg);
public slots:
    void newValue(DeviceItem* item);

//    std::shared_ptr<Prt::ServerInfo> serverInfo() const;

//    void sendLostValues(const QVector<quint32> &ids);
private:
    DBManager* database() const;
    std::unique_ptr<Helpz::Database::ConnectionInfo> db_info_;
    DBManager* db_mng;
//    DBus::PegasIface dbus;

    friend class Network::Client;
    using NetworkClientThread = Helpz::SettingsThreadHelper<Network::Client, Worker*, QString, quint16, QString, QString, QUuid, int>;
    NetworkClientThread::Type* g_mng_th;

    using ScriptsThread = Helpz::SettingsThreadHelper<ScriptSectionManager, Worker*, Helpz::ConsoleReader*, QString>;
    ScriptsThread::Type* sct_mng;
//    RealSectionManager* sct_mng;

    friend class Checker;
    using CheckerThread = Helpz::SettingsThreadHelper<Checker, Worker*, int, QStringList>;
    CheckerThread::Type* checker_th;
//    ScriptsThread* scripts;

    QTimer logTimer;

    std::map<quint32, std::pair<QVariant, QVariant>> waited_item_values;
    QTimer item_values_timer;
};

typedef Helpz::Service::Impl<Worker> Service;

} // namespace Dai

#endif // WORKER_H
