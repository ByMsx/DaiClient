#ifndef WORKER_H
#define WORKER_H

#include <QTimer>

#include <Helpz/service.h>
#include <Helpz/settingshelper.h>

#include "Database/db_manager.h"
#include "checker.h"
#include "Network/n_client.h"
#include "Scripts/scriptedproject.h"

#include "plus/dai/djangohelper.h"
#include "plus/dai/websocket.h"
#include "plus/dai/proj_info.h"

namespace Dai {
class Worker;

class WebSockItem : public QObject, public project::base
{
    Q_OBJECT
public:
    WebSockItem(Worker* obj);
    ~WebSockItem();

signals:
    void send(const ProjInfo &proj, const QByteArray& data) const;
public slots:
    void modeChanged(uint mode_id, uint group_id);
    void procCommand(quint32 user_team_id, quint32 proj_id, quint8 cmd, const QByteArray& data);
private:
    Worker* w;
};

class Worker final : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = 0);
    ~Worker();

    const Helpz::Database::ConnectionInfo& database_info() const;

    static std::unique_ptr<QSettings> settings();
private:
    int init_logging(QSettings* s);
    void init_Database(QSettings *s);
    void init_Project(QSettings* s);
    void init_Checker(QSettings* s);
    void init_GlobalClient(QSettings* s);
    void init_LogTimer(int period);

    void initDjango(QSettings *s);

    std::shared_ptr<WebSockItem> websock_item;
    void initWebSocketManager(QSettings *s);
//    std::shared_ptr<dai::project::base> proj_in_team_by_id(uint32_t team_id, uint32_t proj_id);
signals:
    void serviceRestart();

    // D-BUS Signals
    void started();
    void change(const Dai::ValuePackItem& item, bool immediately);

    void modeChanged(uint mode_id, uint group_id);
    void groupStatusChanged(quint32 group_id, quint32 status);

    void statusAdded(quint32 group_id, quint32 info_id, const QStringList& args);
    void statusRemoved(quint32 group_id, quint32 info_id);

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

    void setControlState(quint32 section_id, quint32 item_type, const QVariant &raw_data);
    void writeToItem(quint32 item_id, const QVariant &raw_data);
    bool setMode(uint mode_id, quint32 group_id);
    bool setCode(const CodeItem &item);

    void setParamValues(const ParamValuesPack& pack);
    bool applyStructModify(quint8 structType, QDataStream* msg);

//    bool setSettings(uchar stType, google::protobuf::Message* msg);
public slots:
    void newValue(DeviceItem* item);

//    std::shared_ptr<Prt::ServerInfo> serverInfo() const;

//    void sendLostValues(const QVector<quint32> &ids);
private:
    DBManager* database() const;
    std::unique_ptr<Helpz::Database::ConnectionInfo> db_info_;
    DBManager* db_mng;

    friend class Network::Client;
    using NetworkClientThread = Helpz::SettingsThreadHelper<Network::Client, Worker*, QString, quint16, QString, QString, QUuid, int>;
    NetworkClientThread::Type* g_mng_th;

    using ScriptsThread = Helpz::SettingsThreadHelper<ScriptedProject, Worker*, Helpz::ConsoleReader*, QString, bool>;
    ScriptsThread::Type* prj;

    friend class Checker;
    using CheckerThread = Helpz::SettingsThreadHelper<Checker, Worker*, int, QString>;
    CheckerThread::Type* checker_th;

    using DjangoThread = Helpz::SettingsThreadHelper<DjangoHelper, QString>;
    DjangoThread::Type* django_th = nullptr;

    using WebSocketThread = Helpz::SettingsThreadHelper<Network::WebSocket, QString, QString, quint16>;
    WebSocketThread::Type* webSock_th = nullptr;
    friend class WebSockItem;

    QTimer logTimer;

    std::map<quint32, std::pair<QVariant, QVariant>> waited_item_values;
    QTimer item_values_timer;
};

typedef Helpz::Service::Impl<Worker> Service;

} // namespace Dai

#endif // WORKER_H
