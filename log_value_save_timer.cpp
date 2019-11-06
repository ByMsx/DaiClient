#include <functional>

#include <QDateTime>

#include <Dai/commands.h>
#include <Dai/project.h>
#include <Dai/device.h>
#include <Dai/db/device_item_value.h>
#include <plus/dai/database.h>

#include "worker.h"
#include "log_value_save_timer.h"
#include "id_timer.h"

namespace Dai {

Log_Value_Save_Timer::Log_Value_Save_Timer(Project* project, Worker *worker) :
    prj_(project), worker_(worker)
{
    connect(&item_values_timer_, &QTimer::timeout, this, &Log_Value_Save_Timer::save_item_values);
    item_values_timer_.setSingleShot(true);

    connect(&value_pack_timer_, &QTimer::timeout, this, &Log_Value_Save_Timer::send_value_pack);
    value_pack_timer_.setSingleShot(true);

    connect(&event_pack_timer_, &QTimer::timeout, this, &Log_Value_Save_Timer::send_event_pack);
    event_pack_timer_.setSingleShot(true);

    for (const Save_Timer &save_timer : project->save_timers_)
    {
        if (save_timer.interval() > 0)
        {
            ID_Timer *timer = new ID_Timer(save_timer.id());
            timer->setTimerType(Qt::VeryCoarseTimer);
            connect(timer, &ID_Timer::timeout, this, &Log_Value_Save_Timer::process_items);
            timer->start(save_timer.interval());
            timers_list_.emplace_back(timer);
        }
    }

    if (!timers_list_.empty())
    {
        for (Device* dev: prj_->devices())
        {
            for (DeviceItem* dev_item: dev->items())
            {
                if (prj_->item_type_mng_.save_timer_id(dev_item->type_id()) > 0)
                {
                    cached_values_.emplace(dev_item->id(), dev_item->raw_value());
                }
            }
        }
    }
}

Log_Value_Save_Timer::~Log_Value_Save_Timer()
{
    stop();
    save_item_values();
    save_to_db(value_pack_);
    save_to_db(event_pack_);
}

QVector<Device_Item_Value> Log_Value_Save_Timer::get_unsaved_values() const
{
    QVector<Device_Item_Value> value_vect;
    for (auto it: waited_item_values_)
    {
        value_vect.push_back(Device_Item_Value{it.first, it.second.first, it.second.second});
    }
    return value_vect;
}

void Log_Value_Save_Timer::add_log_value_item(const Log_Value_Item &item)
{
    waited_item_values_[item.item_id()] = std::make_pair(item.raw_value(), item.value());
    if (!item_values_timer_.isActive() || (item.need_to_save() && item_values_timer_.remainingTime() > 500))
    {
        item_values_timer_.start(item.need_to_save() ? 500 : 5000);
    }

    value_pack_.push_back(item);
    if (!value_pack_timer_.isActive() || (item.need_to_save() && value_pack_timer_.remainingTime() > 10))
    {
        value_pack_timer_.start(item.need_to_save() ? 10 : 250);
    }
}

void Log_Value_Save_Timer::add_log_event_item(const Log_Event_Item &item)
{
    if (item.type_id() == QtDebugMsg && item.category().startsWith("net"))
    {
        return;
    }

    event_pack_.push_back(item);
    if (!event_pack_timer_.isActive() || event_pack_.size() < 100)
    {
        event_pack_timer_.start(1000);
    }
}

void Log_Value_Save_Timer::save_item_values()
{
    QVariantList log_values_pack;
    for (auto it: waited_item_values_)
    {
        log_values_pack.push_back(it.first);
        log_values_pack.push_back(it.second.first);
        log_values_pack.push_back(it.second.second);
    }

    Helpz::Database::Table table = Helpz::Database::db_table<Device_Item_Value>();
    QString sql = "INSERT INTO " + table.name() + '(' + table.field_names().join(',') + ") VALUES" +
            Helpz::Database::Base::get_q_array(table.field_names().size(), waited_item_values_.size()) +
            "ON DUPLICATE KEY UPDATE raw = VALUES(raw), display = VALUES(display)";

    Helpz::Database::Base& db = Helpz::Database::Base::get_thread_local_instance();
    if (!db.exec(sql, log_values_pack).isActive())
    {
        // TODO: Do something
    }
    waited_item_values_.clear();
}

void Log_Value_Save_Timer::send_value_pack()
{
    std::shared_ptr<QVector<Log_Value_Item>> pack = std::make_shared<QVector<Log_Value_Item>>(std::move(value_pack_));
    value_pack_.clear();

    send(Ver_2_2::Cmd::LOG_PACK_VALUES, std::move(pack));
}

void Log_Value_Save_Timer::send_event_pack()
{
    std::shared_ptr<QVector<Log_Event_Item>> pack = std::make_shared<QVector<Log_Event_Item>>(std::move(event_pack_));
    event_pack_.clear();

    send(Ver_2_2::Cmd::LOG_PACK_EVENTS, std::move(pack));
}

void Log_Value_Save_Timer::stop()
{
    item_values_timer_.stop();
    value_pack_timer_.stop();
    event_pack_timer_.stop();

    //timer_.stop();
    for (QTimer *timer : timers_list_)
    {
        timer->stop();
        delete timer;
    }
    timers_list_.clear();
}

void Log_Value_Save_Timer::process_items(int timer_id)
{
    //qDebug() << "!!! process_items" << timer_id;
    //    if (timer_.interval() != (period_ * 1000))
//        timer_.setInterval(period_ * 1000);

    Log_Value_Item pack_item{QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()};

    Item_Type_Manager* typeMng = &prj_->item_type_mng_;

    decltype(cached_values_)::iterator val_it;

    std::shared_ptr<QVector<Log_Value_Item>> pack = std::make_shared<QVector<Log_Value_Item>>();

    for (Device* dev: prj_->devices())
    {
        for (DeviceItem* dev_item: dev->items())
        {
            if (typeMng->save_timer_id(dev_item->type_id()) != timer_id)
                continue;

            val_it = cached_values_.find(dev_item->id());

            if (val_it == cached_values_.end())
                cached_values_.emplace(dev_item->id(), dev_item->raw_value());
            else if (val_it->second != dev_item->raw_value())
                val_it->second = dev_item->raw_value();
            else
                continue;

            pack_item.set_item_id(dev_item->id());
            pack_item.set_raw_value(dev_item->raw_value());
            pack_item.set_value(dev_item->value());
            pack->push_back(pack_item);

            pack_item.set_timestamp_msecs(pack_item.timestamp_msecs() + 1);
        }
    }

    if (!pack->empty())
    {
        send(Ver_2_2::Cmd::LOG_PACK_VALUES, std::move(pack));
    }
}

void Log_Value_Save_Timer::save(Helpz::Database::Base* db, QVector<Log_Value_Item> pack)
{
    for (Log_Value_Item& pack_item: pack)
    {
        if (Database::Helper::save_log_changes(db, pack_item))
        {
            emit change(pack_item, false);
        }
        else
        {
            // TODO: Error event
            qWarning() << "Failed change log with device item" << pack_item.item_id() << pack_item.value();
        }
    }
}

template<typename T>
void Log_Value_Save_Timer::send(uint16_t cmd, std::shared_ptr<QVector<T> > pack)
{
    if (!pack || pack->empty())
    {
        return;
    }

    auto proto = worker_->net_protocol();
    if (proto)
    {
        proto->send(cmd)
                .timeout([this, pack]()
        {
            save_to_db(*pack);
        }, std::chrono::seconds(5), std::chrono::milliseconds(1500)) << *pack;
    }
    else
    {
        save_to_db(*pack);
    }
}

template<typename T>
bool Log_Value_Save_Timer::save_to_db(const QVector<T> &pack)
{
    if (pack.empty())
    {
        return true;
    }

    QVariantList values;
    for (const T& item: pack)
    {
        values += item.to_variantlist();
    }

    Helpz::Database::Table table = Helpz::Database::db_table<T>();
    QString sql = "INSERT IGNORE INTO " + table.name() + '(' + table.field_names().join(',') + ") VALUES" +
            Helpz::Database::Base::get_q_array(table.field_names().size(), pack.size());

    Helpz::Database::Base& db = Helpz::Database::Base::get_thread_local_instance();
    return db.exec(sql, values).isActive();
}

} // namespace Dai
