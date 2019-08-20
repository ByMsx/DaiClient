#include <deque>
#include <vector>
#include <queue>
#include <iterator>
#include <type_traits>

#include <QElapsedTimer>

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

QElapsedTimer tt;

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
                        (!is_write || (dev_item->register_type() == QModbusDataUnit::Coils ||
                                       dev_item->register_type() == QModbusDataUnit::HoldingRegisters)))
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

class Modbus_Pack_Read_Manager
{
public:
    Modbus_Pack_Read_Manager(const Modbus_Pack_Read_Manager&) = delete;
    Modbus_Pack_Read_Manager& operator =(const Modbus_Pack_Read_Manager&) = delete;
    Modbus_Pack_Read_Manager(std::vector<Modbus_Pack<DeviceItem*>>&& packs) : packs_(std::move(packs))
    {
        for (Modbus_Pack<DeviceItem*> &item_pack : packs_)
        {
            for (DeviceItem* device_item : item_pack.items_)
            {
                new_values_.emplace(device_item, QVariant());
            }
        }
    }

    Modbus_Pack_Read_Manager(Modbus_Pack_Read_Manager&& o) :
        packs_(std::move(o.packs_)), new_values_(std::move(o.new_values_)), position_(std::move(o.position_))
    {
        o.packs_.clear();
        o.new_values_.clear();
    }

    ~Modbus_Pack_Read_Manager()
    {
        if (packs_.size())
        {
            QMetaObject::invokeMethod(packs_.front().items_.front()->device(), "set_device_items_values",
                                      QArgument<std::map<DeviceItem*, QVariant>>("std::map<DeviceItem*, QVariant>", new_values_));
            while (packs_.size())
            {
                if (packs_.front().reply_)
                    QObject::disconnect(packs_.front().reply_, 0, 0, 0);
                packs_.pop_back();
            }
        }
    }

    std::vector<Modbus_Pack<DeviceItem*>> packs_;
    std::map<DeviceItem*, QVariant> new_values_;
    int position_ = -1;
};


// -----------------------------------------------------------------

struct Modbus_Queue
{
    std::deque<Modbus_Pack<Write_Cache_Item>> write_;
    std::deque<Modbus_Pack_Read_Manager> read_;

    bool is_active() const
    {
        if (!read_.empty())
        {
            int position  = read_.front().position_;
            if (position > -1 && read_.front().packs_.at(position).reply_)
            {
                return true;
            }
        }

        return write_.size() && write_.front().reply_;
    }

    void clear()
    {
        while (write_.size())
        {
            if (write_.front().reply_)
                QObject::disconnect(write_.front().reply_, 0, 0, 0);
            write_.pop_front();
        }

        while (read_.size())
        {
            read_.pop_front();
        }
    }

    void clear_by_address(int address)
    {
        for (std::size_t i = 0; i < write_.size(); ++i)
        {
            if (write_.front().server_address_ == address)
            {
                if (write_.front().reply_)
                    QObject::disconnect(write_.front().reply_, 0, 0, 0);
                write_.erase(write_.begin() + i);
                --i;
            }
        }

        std::deque<Modbus_Pack_Read_Manager> read;
        for (auto& it : read_)
        {
            if (it.packs_.front().server_address_ != address)
            {
                read.push_back(std::move(it));
            }
        }
        read_ = std::move(read);
    }
};

// -----------------------------------------------------------------

Modbus_Plugin_Base::Modbus_Plugin_Base() :
    QModbusRtuSerialMaster(),
    b_break(false),
    is_port_name_in_config_(false)
{
    queue_ = new Modbus_Queue;
}

Modbus_Plugin_Base::~Modbus_Plugin_Base()
{
    stop();

    queue_->clear();
    delete queue_;
    queue_ = nullptr;
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

    is_port_name_in_config_ = !config_.name.isEmpty();
    if (!is_port_name_in_config_)
    {
        qCDebug(ModbusLog) << "No port name in config file";
        config_.name = Config::getUSBSerial();
    }

    qCDebug(ModbusLog).noquote() << "Used as serial port:" << config_.name << "available:" << config_.available_ports().join(", ");

    connect(this, &QModbusClient::errorOccurred, [this](Error e)
    {
        queue_->clear();
        //qCCritical(ModbusLog).noquote() << "Occurred:" << e << errorString();
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
        queue_->write_.push_back(std::move(pack));
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

void Modbus_Plugin_Base::clear_queue()
{
    queue_->clear();
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

    if (!is_port_name_in_config_)
    {
        config_.name = Config::getUSBSerial();
        qCDebug(ModbusLog) << "No port name in config file. Use:" << config_.name;
    }

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
    if (dev_items.isEmpty())
    {
        return;
    }

    for (auto& it : queue_->read_)
    {
        if (it.packs_.front().server_address_ == Modbus_Plugin_Base::address(dev_items.front()->device()))
        {
            return;
        }
    }

//    qint64 elapsed = tt.restart();
//    qWarning().nospace() << ">>>> read " << (elapsed < 100 ? (elapsed < 10 ? "  " : " ") : "") <<  elapsed << " \tsize " << dev_items.size() << ' ' << dev_items.front()->device()->toString();

    Modbus_Pack_Builder<DeviceItem*> pack_builder(dev_items);
    Modbus_Pack_Read_Manager mng(std::move(pack_builder.container_));
    queue_->read_.push_back(std::move(mng));
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
                queue_->write_.pop_front();
                process_queue();
            }
        }
        else if (queue_->read_.size())
        {
            Modbus_Pack_Read_Manager& modbus_pack_read_manager = queue_->read_.front();
            ++modbus_pack_read_manager.position_;
            if (modbus_pack_read_manager.position_ >= modbus_pack_read_manager.packs_.size())
            {
                queue_->read_.pop_front();
                process_queue();
            }
            else
            {
                Modbus_Pack<DeviceItem*>& pack = modbus_pack_read_manager.packs_.at(modbus_pack_read_manager.position_);
//                qint64 elapsed = tt.restart();
//                qWarning().nospace() << "->>>> read " << (elapsed < 100 ? (elapsed < 10 ? "  " : " ") : "") <<  elapsed << " \tsize " << pack.items_.size() << ' ' << pack.items_.front()->device()->toString();
                read_pack(pack.server_address_, pack.register_type_, pack.start_address_, pack.items_, &pack.reply_);                

                if (!pack.reply_)
                {
                    process_queue();
                }
            }
        }
    }
    else if (b_break && queue_)
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
    if (!items.size() || state() != ConnectedState)
        return;

    QVector<quint16> values = cache_items_to_values(items);
    qCDebug(ModbusLog).noquote().nospace()
            << items.front().user_id_ << "|WRITE " << values << ' '
            << (register_type == QModbusDataUnit::Coils ? "Coils" : "HoldingRegisters")
            << " START " << start_address << " TO ADR " << server_address;

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
    if (!reply || b_break)
    {
        qCCritical(ModbusLog).noquote() << tr("Write finish error: ") + this->errorString();
        process_queue();
        return;
    }

    if (queue_->write_.size())
    {
        Modbus_Pack<Write_Cache_Item>& pack = queue_->write_.front();
        if (reply != pack.reply_)
        {
            qCCritical(ModbusLog).noquote() << tr("Write finished but is not queue front") << reply << pack.reply_;
        }

        pack.reply_ = nullptr;

        queue_->write_.pop_front();

        if (reply->error() != NoError)
        {
            qCWarning(ModbusLog).noquote() << tr("Write response error: %1 Device address: %2 (%3) Function: %4 Start unit: %5 Data:")
                          .arg(reply->errorString())
                          .arg(reply->serverAddress())
                          .arg(reply->error() == ProtocolError ?
                                   tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                   tr("code: 0x%1").arg(reply->error(), -1, 16))
                          .arg(pack.register_type_).arg(pack.start_address_) << cache_items_to_values(pack.items_);
            queue_->clear_by_address(reply->serverAddress());
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
    if (!reply || b_break)
    {
        qCCritical(ModbusLog).noquote() << tr("Read finish error: ") + this->errorString();
        process_queue();
        return;
    }        

    if (queue_->read_.size())
    {        
        Modbus_Pack_Read_Manager& modbus_pack_read_manager = queue_->read_.front();
        Modbus_Pack<DeviceItem*>& pack = modbus_pack_read_manager.packs_.at(modbus_pack_read_manager.position_);
        if (reply != pack.reply_)
        {
            qCCritical(ModbusLog).noquote() << tr("Read finished but is not queue front") << reply << pack.reply_;
        }

        pack.reply_ = nullptr;

        QVariant raw_data;
        const QModbusDataUnit unit = reply->result();
        for (uint i = 0; i < pack.items_.size(); ++i)
        {
            if (reply->error() == NoError && i < unit.valueCount())
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
            }
            else
                raw_data.clear();

            modbus_pack_read_manager.new_values_.at(pack.items_.at(i)) = raw_data;
//                QMetaObject::invokeMethod(pack.items_.at(i), "setRawValue", Qt::QueuedConnection, Q_ARG(const QVariant&, raw_data));
        }

        if (reply->error() != NoError)
        {
            print_cached(pack.server_address_, pack.register_type_, reply->error(), tr("Read response error: %5 Device address: %1 (%6) registerType: %2 Start: %3 Value count: %4")
                         .arg(pack.server_address_).arg(pack.register_type_).arg(pack.start_address_).arg(pack.items_.size())
                         .arg(reply->errorString())
                         .arg(reply->error() == QModbusDevice::ProtocolError ?
                                tr("Mobus exception: 0x%1").arg(reply->rawResult().exceptionCode(), -1, 16) :
                                  tr("code: 0x%1").arg(reply->error(), -1, 16)));
            queue_->clear_by_address(pack.server_address_);
        }
        else
        {            
            auto status_it = dev_status_cache_.find(std::make_pair(pack.server_address_, pack.register_type_));
            if (status_it != dev_status_cache_.end())
            {
                qCDebug(ModbusLog) << "Modbus device" << pack.server_address_ << "recovered" << status_it->second
                         << "RegisterType:" << pack.register_type_ << "Start:" << pack.start_address_ << "Value count:" << pack.items_.size();
                dev_status_cache_.erase(status_it);
            }
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
