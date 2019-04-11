    }
#include <functional>

#include <QDateTime>

#include <Dai/project.h>
#include <Dai/device.h>
#include <plus/dai/database.h>

#include "log_value_save_timer.h"

namespace Dai {

Log_Value_Save_Timer::Log_Value_Save_Timer()
{
    //connect(&timer_, &QTimer::timeout, this, &Log_Value_Save_Timer::process_items);
    //timer_.setTimerType(Qt::VeryCoarseTimer);
}

Log_Value_Save_Timer::Log_Value_Save_Timer(Project* project, Helpz::Database::Thread* db_thread) : prj_(project), db_thread_(db_thread)
{
    for (const Save_Timer &save_timer : project->save_timers_)
    {
        if (save_timer.interval() > 0)
        {
            qDebug() << "!!! Start timer" <<  save_timer.interval();
            QTimer *timer = new QTimer;
            timer->setTimerType(Qt::VeryCoarseTimer);
            connect(timer, &QTimer::timeout, this, &Log_Value_Save_Timer::process_items);
            timer->start(save_timer.interval());
            timers_list_.emplace_back(timer);
        }
    }
}

void Log_Value_Save_Timer::start(int period, Project* project, Helpz::Database::Thread* db_thread)
{
    /*prj_ = project;
    db_thread_ = db_thread;

    const QDateTime cur_time = QDateTime::currentDateTime();
    const uint32_t need2add = period - (cur_time.toTime_t() % period);
    uint32_t interval = cur_time.secsTo(cur_time.addSecs(need2add));

    timer_.start(interval * 1000);*/
}

void Log_Value_Save_Timer::stop()
{
    //timer_.stop();
    for (QTimer *timer : timers_list_)
    {
        timer->stop();
        delete timer;
        qDebug() << "!!! delete timer";
    }
    timers_list_.clear();
}

void Log_Value_Save_Timer::process_items()
{
    qDebug() << "!!! process_items" << qobject_cast<QTimer*>(sender())->interval();
    /*if (timer_.interval() != (period_ * 1000))
        timer_.setInterval(period_ * 1000);

    Log_Value_Item pack_item;
    {
        QDateTime cur_date = QDateTime::currentDateTime().toUTC();
        cur_date.setTime(QTime(cur_date.time().hour(), cur_date.time().minute(), 0));
        pack_item.set_time_msecs(cur_date.toMSecsSinceEpoch());
    }

    Item_Type_Manager* typeMng = &prj_->item_type_mng_;

    decltype(cached_values_)::iterator val_it;

    QVector<Log_Value_Item> pack;

    for (Device* dev: prj_->devices())
    {
        for (DeviceItem* dev_item: dev->items())
        {
            if (typeMng->save_algorithm(dev_item->type_id()) != Item_Type::saSaveByTimer)
                continue;

            val_it = cached_values_.find(dev_item->id());

            if (val_it == cached_values_.end())
                cached_values_.emplace(dev_item->id(), dev_item->raw_value());
            else if (val_it->second != dev_item->raw_value())
                val_it->second = dev_item->raw_value();
            else
                continue;

            pack_item.set_id(0);
            pack_item.set_item_id(dev_item->id());
            pack_item.set_raw_value(dev_item->raw_value());
            pack_item.set_value(dev_item->value());
            pack.push_back(pack_item);
        }
    }

    if (pack.size())
        db_thread_->add_query(std::bind(&Log_Value_Save_Timer::save, this, std::placeholders::_1, std::move(pack)));
        */
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

} // namespace Dai
