#ifndef DBUS_OBJECT_H
#define DBUS_OBJECT_H

#include <memory>

#include <QDBusContext>

#include <Dai/db/group_status_item.h>
#include <Dai/db/group_param_value.h>
#include <Dai/log/log_pack.h>
#include <plus/dai/proj_info.h>

#include <dbus/dbus_common.h>

namespace Dai {
class Worker;
namespace Client {

class Dbus_Object : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", DAI_DBUS_DEFAULT_INTERFACE)
public:
    explicit Dbus_Object(Dai::Worker* worker,
                         const QString& service_name = DAI_DBUS_DEFAULT_SERVICE,
                         const QString& object_path = DAI_DBUS_DEFAULT_OBJECT);
    ~Dbus_Object();

//    std::shared_ptr<Helpz::DTLS::Server_Node> find_client(uint32_t proj_id) const;
//signals:
//    void connection_state_changed(const Project_Info& proj, uint8_t state);
//    void process_statuses(const Project_Info& proj, const QVector<Group_Status_Item>& add, const QVector<Group_Status_Item>& remove, const QString& db_name);

//    void device_item_values_available(const Project_Info& proj, const QVector<Log_Value_Item>& pack);
//    void event_message_available(const Project_Info& proj, const QVector<Log_Event_Item>& event_pack);
//    void time_info(const Project_Info& proj, const QTimeZone& tz, qint64 time_offset);

//    void structure_changed(const Project_Info& proj, const QByteArray& data);
//    void group_param_values_changed(const Project_Info& proj, const QVector<Group_Param_Value> &pack);
//    void group_mode_changed(const Project_Info& proj, quint32 mode_id, quint32 group_id);

//    void status_inserted(const Project_Info& proj, quint32 group_id, quint32 info_id, const QStringList& args);
//    void status_removed(const Project_Info& proj, quint32 group_id, quint32 info_id);

public slots:
//    bool is_connected(uint32_t proj_id);
//    uint8_t get_project_connection_state(const std::set<uint32_t> &team_set, uint32_t proj_id);
//    uint8_t get_project_connection_state2(uint32_t proj_id);
//    void send_message_to_project(uint32_t proj_id, uint16_t cmd, uint32_t user_id, const QByteArray& data);
//    QString get_ip_address(uint32_t proj_id) const;
    void write_item_file(uint32_t item_id, const QString& file_path);
private:
    QString service_name_, object_path_;
    Dai::Worker* worker_;
};

} // namespace Client
} // namespace Dai

#endif // DBUS_OBJECT_H
