#include <algorithm>

#include <QTimer>
#include <QDateTime>
#include <QDateTime>

#include <Dai/section.h>

#include "severaltimeshelper.h"

namespace Dai {

SeveralTimesHelper::SeveralTimesHelper(ItemGroup *group) :
    AutomationHelperItem(group)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_timer, &QTimer::timeout,
            this, &SeveralTimesHelper::onTimer);

    m_timerOff = new QTimer(this);
    m_timerOff->setSingleShot(true);
    m_timerOff->setTimerType(Qt::VeryCoarseTimer);
    connect(m_timerOff, &QTimer::timeout,
            this, &SeveralTimesHelper::onTimerOff);
}

void SeveralTimesHelper::activate()
{
    if (!m_timer->isActive())
    {
        const QDateTime now = QDateTime::currentDateTime();

        QDateTime date(QDate::currentDate());
        date = date.addSecs(start());

        while( now.secsTo(date) <= 0 )
            date = date.addSecs(period());

        m_timer->start( now.secsTo(date) * 1000 );
    }
}

void SeveralTimesHelper::deactivate()
{
    m_timer->stop();
}

uint SeveralTimesHelper::count() const { return m_count; }
uint SeveralTimesHelper::start() const { return m_start; }
uint SeveralTimesHelper::duration() const { return m_duration; }

int SeveralTimesHelper::period() const { return (24 * 60 * 60) / count(); }

void SeveralTimesHelper::setCount(uint count) { m_count = count ? count : 1; }
void SeveralTimesHelper::setStart(uint start) { m_start = start; }
void SeveralTimesHelper::setDuration(uint duration) { m_duration = std::max(duration, static_cast<uint>(5)); }

void SeveralTimesHelper::onTimer()
{
    if (m_timer->interval() != period() * 1000)
        m_timer->start( period() * 1000 );

    writeToControl(true);
    m_timerOff->start( duration() * 1000 );
}

void SeveralTimesHelper::onTimerOff()
{
    writeToControl(false);
}

} // namespace Dai
