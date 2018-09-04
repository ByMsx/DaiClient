#include <set>
#include <map>
#include <optional>

#include <QTimer>

#include <Dai/sectionmanager.h>
#include "daytimehelper.h"

namespace Dai {

DayTimeHelper::DayTimeHelper(SectionManager *house_mng, QObject *parent) :
    QObject(parent), house_mng(house_mng)
{
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &DayTimeHelper::onTimer);
}

void DayTimeHelper::stop()
{
    if (m_timer.isActive())
        m_timer.stop();
}

void DayTimeHelper::init()
{
    stop();

    qint64 current_secs, zero_secs, day_secs, night_secs, min_secs;

    QDateTime current = QDateTime::currentDateTime();
    current_secs = current.toSecsSinceEpoch() + 1;

    current.setTime({0, 0});
    zero_secs = current.toSecsSinceEpoch();

    std::optional<qint64> next_secs;

    for (Section* sct: house_mng->sections())
    {
        day_secs = zero_secs + sct->dayTime()->start();
        night_secs = zero_secs + sct->dayTime()->end();

        if (current_secs >= night_secs)
            night_secs += 24 * 60 * 60;

        if (current_secs >= day_secs)
            day_secs += 24 * 60 * 60;

        min_secs = std::min(day_secs, night_secs);
        if (!next_secs || next_secs.value() > min_secs)
            next_secs = min_secs;
    }

    if (next_secs)
    {
        m_timer.setInterval((next_secs.value() - current_secs) * 1000);
        if (m_timer.interval() < 3000)
            qCritical() << "DayTimeHelper interval is too small" << m_timer.interval() << current_secs << next_secs.value();
//        qDebug() << "Day part will change in" << QDateTime::fromSecsSinceEpoch(next_secs.value()).toString() << "ms:" << m_timer.interval();
        m_timer.start();
    }
    else
        qWarning() << "DayTimeHelper empty";
}

void DayTimeHelper::onTimer()
{
    QDateTime current = QDateTime::currentDateTime();
    qint64 current_secs = current.toSecsSinceEpoch();

    current.setTime({0, 0});
    qint64 zero_secs = current.toSecsSinceEpoch();

    qint64 value_secs;
    auto checkDayPartChanged = [&](qint64 value) {
        value_secs = zero_secs + value;
        return value_secs >= (current_secs - 1) && value_secs <= (current_secs + 1);
    };

    for (Section* sct: house_mng->sections())
    {
        if (checkDayPartChanged(sct->dayTime()->start()))
            emit onDayPartChanged(sct, true);

        if (checkDayPartChanged(sct->dayTime()->end()))
            emit onDayPartChanged(sct, false);
    }

//    qDebug() << "Day part changed in" << QDateTime::currentDateTime().toString();
    init();
}

} // namespace Dai
