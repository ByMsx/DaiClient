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
                         const QString& service_name = DAI_DBUS_DEFAULT_SERVICE_CLIENT,
                         const QString& object_path = DAI_DBUS_DEFAULT_OBJECT);
    ~Dbus_Object();

public slots:
    void write_item_file(uint32_t item_id, const QString& file_path);
private:
    Dai::Worker* worker_;
    QString service_name_, object_path_;
};

} // namespace Client
} // namespace Dai

#endif // DBUS_OBJECT_H
