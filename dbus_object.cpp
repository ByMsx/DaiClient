#include <QtDBus>

#include "dbus_object.h"
#include "worker.h"
#include <QFileInfo>

namespace Dai {
namespace Client {

Dbus_Object::Dbus_Object(Dai::Worker* worker, const QString& service_name, const QString& object_path) :
    QObject(),
    worker_(worker),
    service_name_(service_name), object_path_(object_path)
{
    DBus::register_dbus_types();

    if (!QDBusConnection::systemBus().isConnected())
    {
        qCritical() << "DBus не доступен";
    }
    else if (!QDBusConnection::systemBus().registerService(service_name))
    {
        qCritical() << "DBus служба уже зарегистрированна" << service_name;
    }
    else if (!QDBusConnection::systemBus().registerObject(object_path, this, QDBusConnection::ExportAllContents))
    {
        qCritical() << "DBus объект уже зарегистрирован" << object_path;
    }
    else
    {
        qInfo() << "Dbus служба успешно зарегистрированна" << service_name << object_path;
    }
}

Dbus_Object::~Dbus_Object()
{
    if (QDBusConnection::systemBus().isConnected())
    {
        QDBusConnection::systemBus().unregisterObject(object_path_);
        QDBusConnection::systemBus().unregisterService(service_name_);
    }
}

void Dbus_Object::write_item_file(uint32_t item_id, const QString &file_path)
{
    qInfo() << "Dbus_Object::write_item_file" << "item_id" << item_id << "file_path" << file_path;
    std::shared_ptr<QFile> device(new QFile(file_path));
    QString file_name = QFileInfo(device.get()->fileName()).fileName();

    if (!device->open(QIODevice::ReadOnly))
    {
        qWarning().noquote() << "Can't open file:" << device->errorString();
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(device.get()))
    {
        qWarning().noquote() << "Can't get file hash";
        return;
    }

    QByteArray item_value_data;
    QDataStream ds(&item_value_data, QIODevice::WriteOnly);
    ds << file_name << hash.result();

    qDebug().noquote() << "send file" << file_name << "for devitem" << item_id
                       << "file_path" << file_path << "hash sha1" << hash.result().toHex().constData();

    QVariant raw_data = qVariantFromValue(item_value_data);
    worker_->write_to_item(0, item_id, raw_data);
    worker_->write_to_item_file(file_path);

}

} // namespace Client
} // namespace Dai
