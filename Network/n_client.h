#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <QLoggingCategory>
#include <QDateTime>
#include <QTimer>
#include <QUuid>

#include <Helpz/simplethread.h>
#include <Helpz/dtlsclient.h>
#include <Helpz/waithelper.h>

#include <Dai/typemanager/typemanager.h>
#include <Dai/param/paramgroup.h>
#include <Dai/deviceitem.h>
#include <plus/dai/network.h>

#include "Database/db_manager.h"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Dai {

Q_DECLARE_LOGGING_CATEGORY(NetClientLog)

class Worker;

namespace Network {

class Client : public Helpz::DTLS::Client
{
    Q_OBJECT
    void sendVersion();
public:
    Client(Worker *worker, const QString& hostname, quint16 port, const QString& login, const QString& password, const QUuid& device, int checkServerInterval);

//    static void packValue(Prt::ValuesPack* pack, uint item_id, const DeviceItem::ValueType &raw, const DeviceItem::ValueType &val, uint time, uint db_id);
    const QUuid& device() const;
    const QString& username() const;

    bool canConnect() const override;
signals:
    void restart();
    void getServerInfo(QDataStream* ds) const;
    void setServerInfo(QDataStream* s, QVector<ParamTypeItem> *param_items_out, bool* = nullptr);
    void saveServerInfo(const QVector<ParamTypeItem>& param_values, Project *proj);

    void setControlState(quint32 section_id, quint32 device_type, const QVariant& raw_data);
    void writeToItem(quint32 item_id, const QVariant& raw_data);
    bool setMode(quint32 mode_id, quint32 group_id);
    bool setCode(const CodeItem&);
    void execScript(const QString& script);

    bool setSettings(quint16 cmd, QDataStream* msg);
    bool structModify(quint16 cmd, QDataStream* msg);
    void setParamValues(const ParamValuesPack& pack);

// -----> Sync database
    QPair<quint32, quint32> getLogRange(quint8 log_type, qint64 date);
    Dai::DBManager::LogDataT getLogData(quint8 log_type, const QPair<quint32, quint32>& range);
// <--------------------

public slots:
    void setDevice(const QUuid& devive_uuid);
    void setLogin(const QString& username);
    void setPassword(const QString& password);
    void setImportFlag(bool import_config);
    void refreshAuth(const QUuid& devive_uuid, const QString& username, const QString& password);
    void importDevice(const QUuid& devive_uuid);
    QUuid createDevice(const QString& name, const QString &latin, const QString& description);

//    void setId(int id);
    void change(const ValuePackItem &item, bool immediately = false);

    void eventLog(const EventPackItem &item);

    void modeChanged(uint mode_id, quint32 group_id);
    void groupStatusChanged(quint32 group_id, quint32 status);

//    void sendLostValues(const QVector<Dai::ValuePackItem>& valuesPack);
//    void sendNotFoundIds(const QVector<quint32> &ids);

    void sendParamValues(const ParamValuesPack& pack);

    QVector<QPair<QUuid, QString>> getUserDevices();
protected:
    void readyWrite() override;
    void proccessMessage(quint16 cmd, QDataStream &msg) override;
private slots:
    void sendPack();

    void filePartTimeout();
private:
    void sendAuthInfo();
    void sendLogData(uint8_t log_type, const QPair<quint32, quint32>& range);

    QString m_login, m_password;
    QUuid m_device;
    bool m_import_config;

    QTimer packTimer;
    QVector<ValuePackItem> value_pack_;
    QVector<EventPackItem> event_pack_;

    std::unique_ptr<ReceiveFileInfo> fileInfo;

    Helpz::Network::WaiterMap wait_map;
    QVector<QPair<QUuid, QString>> lastUserDevices;

    Project* prj;

    Worker *worker;
};

} // namespace Network
} // namespace Dai

#endif // NETWORK_CLIENT_H
