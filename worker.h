#ifndef WORKER_H
#define WORKER_H

#include <QTimer>

#include <Helpz/service.h>
#include <Helpz/settingshelper.h>

#include <Helpz/dtls_client_thread.h>

#include "Database/db_manager.h"
#include "checker.h"
#include "Network/client_protocol_2_0.h"
#include "Scripts/scriptedproject.h"

#include "plus/dai/djangohelper.h"
#include "plus/dai/websocket.h"
#include "plus/dai/proj_info.h"

#include "structure_synchronizer.h"

namespace Dai {
class Worker;

class WebSockItem : public QObject, public Project_Info
{
    Q_OBJECT
public:
    WebSockItem(Worker* obj);
    ~WebSockItem();

signals:
    void send(const Project_Info &proj, const QByteArray& data) const;
public slots:
    void send_event_message(const Log_Event_Item& event);

    void modeChanged(uint mode_id, uint group_id);
    void procCommand(uint32_t user_id, quint32 user_team_id, quint32 proj_id, quint8 cmd, const QByteArray& raw_data);
private:
    Worker* w;
};

class Worker final : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = 0);
    ~Worker();

    DBManager* database() const;
    Helpz::Database::Thread* db_pending();
    const Helpz::Database::Connection_Info& database_info() const;

    static std::unique_ptr<QSettings> settings();

    std::shared_ptr<Client::Protocol_2_0> net_protocol();

private:
    int init_logging(QSettings* s);
    void init_Database(QSettings *s);
    void init_Project(QSettings* s);
    void init_Checker(QSettings* s);
    void init_network_client(QSettings* s);
    void init_LogTimer(int period);

    void initDjango(QSettings *s);

    std::shared_ptr<WebSockItem> websock_item;
    void initWebSocketManager(QSettings *s);
signals:
    void serviceRestart();

    // D-BUS Signals
    void started();
    void change(const Log_Value_Item& item, bool immediately);

    void modeChanged(uint32_t user_id, uint32_t mode_id, uint32_t group_id);

    void status_added(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t user_id);
    void status_removed(quint32 group_id, quint32 info_id, uint32_t user_id);

    void paramValuesChanged(uint32_t user_id, const ParamValuesPack& pack);

    void event_message(const Log_Event_Item&);
public slots:
    void restart_service_object(uint32_t user_id);
    void logMessage(QtMsgType type, const Helpz::LogContext &ctx, const QString &str);
    void add_event_message(const Log_Event_Item& event);
    void processCommands(const QStringList& args);

    QString getUserDevices();
    QString getUserStatus();

    void initDevice(const QString& device, const QString& device_name, const QString &device_latin, const QString& device_desc);
    void clearServerConfig();
    void saveServerAuthData(const QString& login, const QString& password);
    void saveServerData(const QUuid &devive_uuid, const QString& login, const QString& password);

    bool setDayTime(uint section_id, uint dayStartSecs, uint dayEndSecs);

    void writeToItem(uint32_t user_id, uint32_t item_id, const QVariant &raw_data);
    void write_to_item_file(const QString& file_name);
    bool setMode(uint32_t user_id, uint32_t mode_id, uint32_t group_id);

    void setParamValues(uint32_t user_id, const ParamValuesPack& pack);

    void add_status(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t user_id);
    void remove_status(quint32 group_id, quint32 info_id, uint32_t user_id);
public slots:
    void newValue(DeviceItem* item, uint32_t user_id = 0);
private:
    std::unique_ptr<Helpz::Database::Connection_Info> db_info_;
    DBManager* db_mng;

    friend class Client::Protocol_2_0;
    friend class Client::Protocol;
    std::shared_ptr<Helpz::DTLS::Client_Thread> net_thread_;
    std::unique_ptr<Helpz::Database::Thread> db_pending_thread_;
    QThread net_protocol_thread_;
    Client::Structure_Synchronizer structure_sync_;

    using ScriptsThread = Helpz::SettingsThreadHelper<ScriptedProject, Worker*, Helpz::ConsoleReader*, QString, bool>;
    ScriptsThread::Type* prj;

    friend class Checker;
    using CheckerThread = Helpz::SettingsThreadHelper<Checker, Worker*, int, QString>;
    CheckerThread::Type* checker_th;

    using DjangoThread = Helpz::SettingsThreadHelper<DjangoHelper, QString>;
    DjangoThread::Type* django_th = nullptr;

    using WebSocketThread = Helpz::SettingsThreadHelper<Network::WebSocket, quint16, QString, QString>;
    WebSocketThread::Type* webSock_th = nullptr;
    friend class WebSockItem;

    QTimer logTimer;

    std::map<quint32, std::pair<QVariant, QVariant>> waited_item_values;
    QTimer item_values_timer;

    uint32_t last_file_item_id_;
};

typedef Helpz::Service::Impl<Worker> Service;

} // namespace Dai

#endif // WORKER_H
