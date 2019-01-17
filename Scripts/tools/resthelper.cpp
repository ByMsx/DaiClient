#include <QTimer>

#include <Dai/section.h>
#include "resthelper.h"

namespace Dai {

RestHelper::RestHelper(ItemGroup *group) :
    AutomationHelperItem(group)
{
    connect(group, &ItemGroup::controlChanged,
            this, &RestHelper::controlChanged);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout,
            this, &RestHelper::onTimer);
}

uint RestHelper::workTime() const { return m_timeRange.start(); }
uint RestHelper::restTime() const { return m_timeRange.end(); }

void RestHelper::setWorkTime(uint workTime)
{
    m_timeRange.set_start(workTime);
    checkParam(workTime, false);
}

void RestHelper::setRestTime(uint restTime)
{
    m_timeRange.set_end(restTime);
    checkParam(restTime, true);
}

bool RestHelper::resting()
{
    return m_resting;
}

bool RestHelper::alwaysWork() const { return m_alwaysWork; }
void RestHelper::setAlwaysWork(bool alwaysWork) { m_alwaysWork = alwaysWork; }

void RestHelper::checkParam(uint time, bool rest)
{
    if (m_resting == rest && m_timer->isActive())
    {
        if (!time)
            m_timer->stop();
        else
        {
            auto timePassed = QDateTime::currentDateTime().toTime_t() - m_start_time;
            if (timePassed > time)
                m_timer->start(0);
        }
    }
}

void RestHelper::controlChanged(DeviceItem *dev_item)
{
    if (dev_item->type() != type())
        return;

    bool isControlOn = group()->isControlOn(type());

    if (isControlOn)
    {
        if (!m_timer->isActive())
        {
            m_resting = false;
            m_start_time = QDateTime::currentDateTime().toTime_t();

            int work_time = m_timeRange.start() - m_timePassed;
            if (work_time < 0)
                work_time = 0;
            m_timer->start( work_time * 1000 );
        }
        else if (m_resting)
            setControlState(false);
    }
    else
    {
        if (!m_resting)
        {
            if (m_timer->isActive())
            {
                m_timePassed = QDateTime::currentDateTime().toTime_t() - m_start_time;
            }
        }
    }
}

void RestHelper::onTimer()
{
    bool flag = true, newState = m_resting;
    m_resting = !m_resting;

    if (m_resting)
    {
        if (m_timeRange.end() > 0)
        {
            m_timePassed = 0;
            m_start_time = QDateTime::currentDateTime().toTime_t();
            m_timer->start( m_timeRange.end() * 1000 );
        }
    }
    else
    {
        if (!m_alwaysWork)
            flag = false;
    }

    if (flag)
        setControlState(newState);
}

} // namespace Dai
