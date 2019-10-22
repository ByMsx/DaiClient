#ifndef DAI_CLIENT_WORKER_H
#define DAI_CLIENT_WORKER_H

#include <qglobal.h>

#ifdef QT_DEBUG
#include <QApplication>
typedef QApplication App_Type;
#else
#include <QGuiApplication>
typedef QGuiApplication App_Type;
#endif

#include <Helpz/service.h>
#include <Helpz/settingshelper.h>

#include <Helpz/dtls_client_thread.h>

#include <plus/dai/database.h>

#include "checker.h"
#include "Network/client_protocol_latest.h"
#include "Scripts/scriptedproject.h"

#include "plus/dai/websocket.h"
#include "plus/dai/proj_info.h"

#include "structure_synchronizer.h"
#include "log_value_save_timer.h"

namespace Dai {

class DB_Connection_Info : public Helpz::Database::Connection_Info
{
public:
    DB_Connection_Info(const QString &common_db_name, const QString &db_name, const QString &login, const QString &password,
                   const QString &host = "localhost", int port = -1,
                   const QString &driver_name = "QMYSQL", const QString& connect_options = QString()) :
        Helpz::Database::Connection_Info(db_name, login, password, host, port, driver_name, connect_options),
        common_db_name_(common_db_name)
    {
        if (common_db_name.isEmpty())
        {
            QStringList list = db_name.split('_');
            if (list.size())
                common_db_name_ = list.first();
        }
    }

    QString common_db_name() const { return common_db_name_; }
private:
    QString common_db_name_;
};

class Websocket_Item;

class Worker final : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = 0);
    ~Worker();

    Database::Helper* database() const;
    Helpz::Database::Thread* db_pending();
    const DB_Connection_Info& database_info() const;

    static std::unique_ptr<QSettings> settings();

    std::shared_ptr<Client::Protocol_Latest> net_protocol();

    static void store_connection_id(const QUuid& connection_id);
private:
    void init_logging(QSettings* s);
    void init_database(QSettings *s);
    void init_project(QSettings* s);
    void init_checker(QSettings* s);
    void init_network_client(QSettings* s);
    void init_log_timer();

    std::shared_ptr<Websocket_Item> websock_item;
    void init_websocket_manager(QSettings *s);
signals:
    void serviceRestart();

    // D-BUS Signals
    void started();
    void change(const Log_Value_Item& item, bool immediately);

    void mode_changed(uint32_t user_id, uint32_t mode_id, uint32_t group_id);

    void status_added(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t user_id);
    void status_removed(quint32 group_id, quint32 info_id, uint32_t user_id);

    void group_param_values_changed(uint32_t user_id, const QVector<Group_Param_Value>& pack);

public slots:
    void restart_service_object(uint32_t user_id = 0);
    bool stop_scripts(uint32_t user_id = 0);

    void logMessage(QtMsgType type, const Helpz::LogContext &ctx, const QString &str);
    void add_event_message(Log_Event_Item event);
    void processCommands(const QStringList& args);

    QString get_user_devices();
    QString get_user_status();

    void init_device(const QString& device, const QString& device_name, const QString &device_latin, const QString& device_desc);
    void clear_server_config();
    void save_server_auth_data(const QString& login, const QString& password);
    void save_server_data(const QUuid &devive_uuid, const QString& login, const QString& password);

    bool set_day_time(uint section_id, uint dayStartSecs, uint dayEndSecs);

    void write_to_item(uint32_t user_id, uint32_t item_id, const QVariant &raw_data);
    void write_to_item_file(const QString& file_name);
    bool set_mode(uint32_t user_id, uint32_t mode_id, uint32_t group_id);

    void set_group_param_values(uint32_t user_id, const QVector<Group_Param_Value>& pack);

    void add_status(quint32 group_id, quint32 info_id, const QStringList& args, uint32_t user_id);
    void remove_status(quint32 group_id, quint32 info_id, uint32_t user_id);

    void update_plugin_param_names(const QVector<Plugin_Type>& plugins);
public slots:
    void new_value(DeviceItem* item, uint32_t user_id = 0);
private:
    std::unique_ptr<DB_Connection_Info> db_info_;
    Database::Helper* db_mng_;

    friend class Client::Protocol_Latest;
    friend class Client::Protocol;
    std::shared_ptr<Helpz::DTLS::Client_Thread> net_thread_;
    std::unique_ptr<Helpz::Database::Thread> db_pending_thread_;
    QThread net_protocol_thread_;
    std::unique_ptr<Client::Structure_Synchronizer> structure_sync_;

    using ScriptsThread = Helpz::SettingsThreadHelper<Scripted_Project, Worker*, Helpz::ConsoleReader*, QString, bool>;
    ScriptsThread::Type* project_thread_;
    Scripted_Project* prj();
    Scripted_Project* prj_;

    friend class Checker;
    using CheckerThread = Helpz::SettingsThreadHelper<Checker, Worker*, QStringList>;
    CheckerThread::Type* checker_th = nullptr;

    using WebSocketThread = Helpz::SettingsThreadHelper<Network::WebSocket, std::shared_ptr<JWT_Helper>, quint16, QString, QString>;
    WebSocketThread::Type* websock_th_ = nullptr;
    friend class Websocket_Item;

    using Log_Value_Save_Timer_Thread = Helpz::ParamThread<Log_Value_Save_Timer, Project*, Helpz::Database::Thread*>;
    Log_Value_Save_Timer_Thread* log_timer_thread_ = nullptr;

    bool restart_timer_started_;
    std::map<quint32, std::pair<QVariant, QVariant>> waited_item_values_;
    QTimer item_values_timer_;

    std::vector<Log_Value_Item> to_save_log_value_vect_;
    std::vector<Log_Event_Item> to_save_log_event_vect_;

    std::pair<uint32_t,uint32_t> last_file_item_and_user_id_;
};

typedef Helpz::Service::Impl<Worker, App_Type> Service;

} // namespace Dai

#endif // DAI_CLIENT_WORKER_H
