#include <queue>
#include <vector>
#include <iterator>
#include <type_traits>

#include <QDebug>
#include <QSettings>
#include <QFile>

#ifdef QT_DEBUG
#include <QtDBus>
#endif

#include <Helpz/settingshelper.h>
#include <Helpz/simplethread.h>

#include <Dai/db/item_type.h>
#include <Dai/deviceitem.h>
#include <Dai/device.h>

#include "modbus_plugin_base.h"

namespace Dai {
namespace Modbus {

Q_LOGGING_CATEGORY(ModbusLog, "modbus")

template <typename T>
struct Modbus_Pack_Item_Cast {
    static inline DeviceItem* run(T item) { return item; }
};

template <>
struct Modbus_Pack_Item_Cast<Write_Cache_Item> {
    static inline DeviceItem* run(const Write_Cache_Item& item) { return item.dev_item_; }
};

template<typename T>
struct Modbus_Pack
{
    Modbus_Pack(Modbus_Pack<T>&& o) = default;
    Modbus_Pack(const Modbus_Pack<T>& o) = default;
    Modbus_Pack<T>& operator =(Modbus_Pack<T>&& o) = default;
    Modbus_Pack<T>& operator =(const Modbus_Pack<T>& o) = default;
    Modbus_Pack(T&& item) :
        reply_(nullptr)
    {
        init(Modbus_Pack_Item_Cast<T>::run(item), std::is_same<Write_Cache_Item, T>::value);
        items_.push_back(std::move(item));
    }

    void init(DeviceItem* dev_item, bool is_write)
    {
        bool ok;
        server_address_ = Modbus_Plugin_Base::address(dev_item->device(), &ok);
        if (ok && server_address_ > 0)
        {
            start_address_ = Modbus_Plugin_Base::unit(dev_item, &ok);
            if (ok && start_address_ >= 0)
            {
                if (dev_item->register_type() > QModbusDataUnit::Invalid && dev_item->register_type() <= QModbusDataUnit::HoldingRegisters &&
                        (!is_write || (dev_item->register_type() == QModbusDataUnit::Coils || dev_item->register_type() != QModbusDataUnit::HoldingRegisters)))
                {
                    register_type_ = static_cast<QModbusDataUnit::RegisterType>(dev_item->register_type());
                }
                else
                    register_type_ = QModbusDataUnit::Invalid;
            }
            else
                start_address_ = -1;
        }
        else
            server_address_ = -1;
    }

    bool is_valid() const
    {
        return server_address_ > 0 && start_address_ >= 0 && register_type_ != QModbusDataUnit::Invalid;
    }

    bool add_next(T&& item)
    {
        DeviceItem* dev_item = Modbus_Pack_Item_Cast<T>::run(item);
        if (register_type_ == dev_item->register_type() &&
            server_address_ == Modbus_Plugin_Base::address(dev_item->device()))
        {
            int unit = Modbus_Plugin_Base::unit(dev_item);
            if (unit == (start_address_ + static_cast<int>(items_.size())))
            {
                items_.push_back(std::move(item));
                return true;
            }
        }
        return false;
    }

    bool assign(Modbus_Pack<T>& pack)
    {
        if (register_type_ == pack.register_type_ &&
            server_address_ == pack.server_address_ &&
            (start_address_ + static_cast<int>(items_.size())) == pack.start_address_)
        {
            std::copy( std::make_move_iterator(pack.items_.begin()),
                       std::make_move_iterator(pack.items_.end()),
                       std::back_inserter(items_) );
            return true;
        }
        return false;
    }

    bool operator <(DeviceItem* dev_item) const
    {
        return register_type_ < dev_item->register_type() ||
               server_address_ < Modbus_Plugin_Base::address(dev_item->device()) ||
               (start_address_ + static_cast<int>(items_.size())) < Modbus_Plugin_Base::unit(dev_item);
    }

    int server_address_;
    int start_address_;
    QModbusDataUnit::RegisterType register_type_;
    QModbusReply* reply_;
    std::vector<T> items_;
};

template<typename T, typename Container> struct Input_Container_Item_Type { typedef T& type; };
template<typename T, typename Container> struct Input_Container_Item_Type<T, const Container> { typedef T type; };

template<typename T>
class Modbus_Pack_Builder
{
public:
    template<typename Input_Container>
    Modbus_Pack_Builder(Input_Container& items)
    {
        typename std::vector<Modbus_Pack<T>>::iterator it;

        for (typename Input_Container_Item_Type<T, Input_Container>::type item: items)
        {
            insert(std::move(item));
        }
    }

    std::vector<Modbus_Pack<T>> container_;
private:
    void insert(T&& item)
    {
        typename std::vector<Modbus_Pack<T>>::iterator it = container_.begin();
        for (; it != container_.end(); ++it)
        {
            if (*it < Modbus_Pack_Item_Cast<T>::run(item))
            {
                continue;
            }
            else if (it->add_next(std::move(item)))
            {
                assign_next(it);
                return;
            }
            else
            {
                create(it, std::move(item));
                return;
            }
        }

        if (it == container_.end())
        {
            create(it, std::move(item));
        }
    }

    void create(typename std::vector<Modbus_Pack<T>>::iterator it, T&& item)
    {
        Modbus_Pack pack(std::move(item));
        if (pack.is_valid())
        {
            it = container_.insert(it, std::move(pack));
            assign_next(it);
        }
    }

    void assign_next(typename std::vector<Modbus_Pack<T>>::iterator it)
    {
        if (it != container_.end())
        {
            typename std::vector<Modbus_Pack<T>>::iterator old_it = it;
            it++;
            if (it != container_.end())
            {
                if (old_it->assign(*it))
                {
                    container_.erase(it);
                }
            }
        }
    }
};

// -----------------------------------------------------------------

struct Modbus_Queue
{
    std::queue<Modbus_Pack<Write_Cache_Item>> write_;
    std::queue<Modbus_Pack<DeviceItem*>> read_;

    bool is_active() const
    {
        return (write_.size() && write_.front().reply_) ||
                (read_.size() && read_.front().reply_);
    }

    void clear()
    {
        if (write_.size())
        {
            std::queue<Modbus_Pack<Write_Cache_Item>> empty;
            std::swap( write_, empty );
        }

        if (read_.size())
        {
            std::queue<Modbus_Pack<DeviceItem*>> empty;
            std::swap( read_, empty );
        }
    }
};

// -----------------------------------------------------------------

Modbus_Plugin_Base::Modbus_Plugin_Base() :
    QModbusRtuSerialMaster(),
    b_break(false)
{
    queue_ = new Modbus_Queue;
}

Modbus_Plugin_Base::~Modbus_Plugin_Base()
{
    delete queue_;
}

bool Modbus_Plugin_Base::check_break_flag() const
{
    return b_break;
}

void Modbus_Plugin_Base::clear_break_flag()
{
    if (b_break)
        b_break = false;
}

const Config& Modbus_Plugin_Base::config() const
{
    return config_;
}

bool Modbus_Plugin_Base::checkConnect()
{
    if (state() == ConnectedState || connectDevice())
    {
        return true;
    }

    print_cached(0, QModbusDataUnit::Invalid, ConnectionError, tr("Connect failed: ") + errorString());
    return false;
}

void Modbus_Plugin_Base::configure(QSettings *settings, Project *)
{
    using Helpz::Param;

    config_ = Helpz::SettingsHelper
        #if (__cplusplus < 201402L) || (defined(__GNUC__) && (__GNUC__ < 7))
            <Param<QString>,Param<QSerialPort::BaudRate>,Param<QSerialPort::DataBits>,
                            Param<QSerialPort::Parity>,Param<QSerialPort::StopBits>,Param<QSerialPort::FlowControl>,Param<int>,Param<int>,Param<int>>
        #endif
            (
                settings, "Modbus",
                Param<QString>{"Port", "ttyUSB0"},
                Param<QSerialPort::BaudRate>{"BaudRate", QSerialPort::Baud9600},
                Param<QSerialPort::DataBits>{"DataBits", QSerialPort::Data8},
                Param<QSerialPort::Parity>{"Parity", QSerialPort::NoParity},
                Param<QSerialPort::StopBits>{"StopBits", QSerialPort::OneStop},
                Param<QSerialPort::FlowControl>{"FlowControl", QSerialPort::NoFlowControl},
                Param<int>{"ModbusTimeout", timeout()},
                Param<int>{"ModbusNumberOfRetries", numberOfRetries()},
                Param<int>{"InterFrameDelay", interFrameDelay()}
    ).obj<Config>();

#if defined(QT_DEBUG) && defined(Q_OS_UNIX)
    if (QDBusConnection::sessionBus().isConnected())
    {
        QDBusInterface iface("ru.deviceaccess.Dai.Emulator", "/", "", QDBusConnection::sessionBus());
        if (iface.isValid())
        {
            QDBusReply<QString> tty_path = iface.call("ttyPath");
            if (tty_path.isValid())
                config_.name = tty_path.value(); // "/dev/ttyUSB0";
        }
        else
        {
            QString overTCP = "/home/kirill/vmodem0";
            config_.name = QFile::exists(overTCP) ? overTCP : "";//"/dev/pts/10";
        }
    }
#endif

    if (config_.name.isEmpty())
        config_.name = Config::getUSBSerial();

    qCDebug(ModbusLog).noquote() << "Used as serial port:" << config_.name << "available:" << config_.available_ports().join(", ");

    connect(this, &QModbusClient::errorOccurred, [this](Error e)
    {
//        qCCritical(ModbusLog).noquote() << "Occurred:" << e << errorString();
        if (e == ConnectionError)
            disconnectDevice();
    });

    Config::set(config_, this);
}

bool Modbus_Plugin_Base::check(Device* dev)
{
    if (!checkConnect())
        return false;

    clear_break_flag();

    read(dev->items());
    return true;
}

void Modbus_Plugin_Base::stop()
{
    b_break = true;
}

void Modbus_Plugin_Base::write(std::vector<Write_Cache_Item>& items)
{
    if (!checkConnect() || !items.size() || b_break)
        return;

    Modbus_Pack_Builder<Write_Cache_Item> pack_builder(items);
    for (Modbus_Pack<Write_Cache_Item>& pack: pack_builder.container_)
    {
        queue_->write_.push(std::move(pack));
    }
    process_queue();
}

QStringList Modbus_Plugin_Base::available_ports() const
{
    return Config::available_ports();
}

void Modbus_Plugin_Base::clear_status_cache()
{
    dev_status_cache_.clear();
}

void Modbus_Plugin_Base::write_finished_slot()
{
    write_finished(qobject_cast<QModbusReply*>(sender()));
}

void Modbus_Plugin_Base::read_finished_slot()
{
    read_finished(qobject_cast<QModbusReply*>(sender()));
}

void Modbus_Plugin_Base::print_cached(int server_address, QModbusDataUnit::RegisterType register_type, QModbusDevice::Error value, const QString& text)
{
    auto request_pair = std::make_pair(server_address, register_type);
    auto status_it = dev_status_cache_.find(request_pair);

    if (status_it == dev_status_cache_.end() || status_it->second != value)
    {
        qCWarning(ModbusLog).noquote() << text;

        if (status_it == dev_status_cache_.end())
            dev_status_cache_[request_pair] = value;
        else
            status_it->second = value;
    }
}

bool Modbus_Plugin_Base::reconnect()
{
    disconnectDevice();

    config_.name = Config::getUSBSerial();
    if (!config_.name.isEmpty())
    {
        setConnectionParameter(SerialPortNameParameter, config_.name);

        if (connectDevice())
        {
            return true;
        }
        else
        {
            print_cached(0, QModbusDataUnit::Invalid, error(), tr("Connect to port %1 fail: %2").arg(config_.name).arg(errorString()));
        }
    }
    else
    {
        print_cached(0, QModbusDataUnit::Invalid, ConnectionError, tr("USB Serial not found"));
    }
    return false;
}

void Modbus_Plugin_Base::read(const QVector<DeviceItem*>& dev_items)
{
    Modbus_Pack_Builder<DeviceItem*> pack_builder(dev_items);
    for (Modbus_Pack<DeviceItem*>& pack: pack_builder.container_)
    {
        queue_->read_.push(std::move(pack));
    }

    process_queue();
}

void Modbus_Plugin_Base::process_queue()
{
    if (!b_break && !queue_->is_active())
    {
        if (state() != ConnectedState && !reconnect())
        {
            queue_->clear();
            return;
        }
        else
        {
            auto status_it = dev_status_cache_.find(std::make_pair(0, QModbusDataUnit::Invalid));
            if (status_it != dev_status_cache_.end())
            {
                qCDebug(ModbusLog) << "Modbus device opened";
                dev_status_cache_.erase(status_it);
            }
        }

        if (queue_->write_.size())
        {
            Modbus_Pack<Write_Cache_Item>& pack = queue_->write_.front();
            write_pack(pack.server_address_, pack.register_type_, pack.start_address_, pack.items_, &pack.reply_);
            if (!pack.reply_)
            {
                queue_->write_.pop();
                process_queue();
            }
        }
        else if (queue_->read_.size())
        {
            Modbus_Pack<DeviceItem*>& pack = queue_->read_.front();
            read_pack(pack.server_address_, pack.register_type_, pack.start_address_, pack.items_, &pack.reply_);
            if (!pack.reply_)
            {
                queue_->read_.pop();
                process_queue();
            }
        }
    }
    else if (b_break)
        queue_->clear();
}

QVector<quint16> Modbus_Plugin_Base::cache_items_to_values(const std::vector<Write_Cache_Item>& items) const
{
    QVector<quint16> values;
    quint16 write_data;
    for (const Write_Cache_Item& item: items)
    {
        if (item.raw_data_.type() == QVariant::Bool)
            write_data = item.raw_data_.toBool() ? 1 : 0;
        else
            write_data = item.raw_data_.toUInt();
        values.push_back(write_data);

    }
    return values;
}

void Modbus_Plugin_Base::write_pack(int server_address, QModbusDataUnit::RegisterType register_type, int start_address, const std::vector<Write_Cache_Item>& items, QModbusReply** reply)
{
    QVector<quint16> values = cache_items_to_values(items);
    qCDebug(ModbusLog).noquote().nospace() << items.front().user_id_ << "|WRITE " << values.size() << " START " << start_address
                                 << " TO ADR " << server_address
                                 << " REGISTER " << (register_type == QModbusDataUnit::Coils ? "Coils" : "HoldingRegisters")
                                 << " DATA " << values;

    QModbusDataUnit write_unit(register_type, start_address, values);
    *reply = sendWriteRequest(write_unit, server_address);

    if (*reply)
    {
        if ((*reply)->isFinished())
        {
            write_finished(*reply);
        }
        else
        {
            connect(*reply, &QModbusReply::finished, this, &Modbus_Plugin_Base::write_finished_slot);
        }
    }
    else
        qCCritical(ModbusLog).noquote() << tr("Write error: ") + this->errorString();
}

void Modbus_Plugin_Base::write_finished(QModbusReply* reply)
{
    if (!reply)
    {
        qCCritical(ModbusLog).noquote() << tr("Write finish error: ") + this->errorString();
        process_queue();
        return;
    }

    if (queue_->write_.size())
    {
        Modbus_Pack<Write_Cache_Item>& pack = queue_->write_.front();
        if (reply == pack.reply_)
        {
            queue_->write_.pop();
        }
        else
            qCCritical(ModbusLog).noquote() << tr("Write finished but is not queue front") << reply << pack.reply_;

        if (reply->error() != NoError)
        {
            qCWarning(ModbusLog).noquote() << tr("Write response error: %1 Device address: %2 (%3) Function: %4 Start unit: %5 Data:")
                          .arg(reply->errorString())
                          .arg(reply->serverAddress())
                          .arg(reply->error() == ProtocolError ?
                                   tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                   tr("code: 0x%1").arg(reply->error(), -1, 16))
                          .arg(pack.register_type_).arg(pack.start_address_) << cache_items_to_values(pack.items_);
        }
    }
    else
        qCCritical(ModbusLog).noquote() << tr("Write finished but queue is empty");

    reply->deleteLater();
    process_queue();
}

void Modbus_Plugin_Base::read_pack(int server_address, QModbusDataUnit::RegisterType register_type, int start_address, const std::vector<DeviceItem*>& items, QModbusReply** reply)
{
    QModbusDataUnit request(register_type, start_address, items.size());
    *reply = sendReadRequest(request, server_address);

    if (*reply)
    {
        if ((*reply)->isFinished())
        {
            read_finished(*reply);
        }
        else
        {
            connect(*reply, &QModbusReply::finished, this, &Modbus_Plugin_Base::read_finished_slot);
        }
    }
    else
        qCCritical(ModbusLog).noquote() << tr("Read error: ") + this->errorString();
}

void Modbus_Plugin_Base::read_finished(QModbusReply* reply)
{
    if (!reply)
    {
        qCCritical(ModbusLog).noquote() << tr("Read finish error: ") + this->errorString();
        process_queue();
        return;
    }

    if (queue_->read_.size())
    {
        Modbus_Pack<DeviceItem*>& pack = queue_->read_.front();
        if (reply == pack.reply_)
        {
            QVariant raw_data;
            const QModbusDataUnit unit = reply->result();
            for (uint i = 0; i < unit.valueCount() && i < pack.items_.size(); i++)
            {
                if (pack.register_type_ == QModbusDataUnit::Coils ||
                        pack.register_type_ == QModbusDataUnit::DiscreteInputs)
                {
                    raw_data = static_cast<bool>(unit.value(i));
                }
                else
                {
                    raw_data = static_cast<qint32>(unit.value(i));
                }

                QMetaObject::invokeMethod(pack.items_.at(i), "setRawValue", Qt::QueuedConnection,
                                          Q_ARG(const QVariant&, raw_data));
            }

            auto status_it = dev_status_cache_.find(std::make_pair(pack.server_address_, pack.register_type_));
            if (status_it != dev_status_cache_.end())
            {
                qCDebug(ModbusLog) << "Modbus device " << pack.server_address_ << "recovered" << status_it->second
                         << "RegisterType:" << pack.register_type_ << "Start:" << pack.start_address_ << "Value count:" << pack.items_.size();
                dev_status_cache_.erase(status_it);
            }

            queue_->read_.pop();
        }
        else
            qCCritical(ModbusLog).noquote() << tr("Read finished but is not queue front") << reply << pack.reply_;

        if (reply->error() != NoError)
        {
            print_cached(pack.server_address_, pack.register_type_, reply->error(), tr("Read response error: %5 Device address: %1 (%6) registerType: %2 Start: %3 Value count: %4")
                         .arg(pack.server_address_).arg(pack.register_type_).arg(pack.start_address_).arg(pack.items_.size())
                         .arg(reply->errorString())
                         .arg(reply->error() == QModbusDevice::ProtocolError ?
                                tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                  tr("code: 0x%1").arg(reply->error(), -1, 16)));
        }
    }
    else
        qCCritical(ModbusLog).noquote() << tr("Read finished but queue is empty");

    reply->deleteLater();
    process_queue();
}

/*static*/ int32_t Modbus_Plugin_Base::address(Device *dev, bool* ok)
{
    QVariant v = dev->param("address");
    return v.isValid() ? v.toInt(ok) : -2;
}

/*static*/ int32_t Modbus_Plugin_Base::unit(DeviceItem *item, bool* ok)
{
    QVariant v = item->param("unit");
    return v.isValid() ? v.toInt(ok) : -2;
}

} // namespace Modbus
} // namespace Dai
