#include <QTimer>
#include <QDebug>

#include <Dai/section.h>

#include "pidhelper.h"

namespace Dai
{

PIDHelper::PIDHelper(ItemGroup *group, uint sensor_type) :
    AutomationHelperItem(group),
    m_sensor_type(sensor_type)
{
    m_pid = new PIDController(this);

    m_pwm_timer = new QTimer(this);
    m_pwm_timer->setSingleShot(true);
    connect(m_pwm_timer, &QTimer::timeout,
            this, &PIDHelper::onPWM);
}

PIDController *PIDHelper::pid() const { return m_pid; }

void PIDHelper::startPWM()
{
    if (!m_pwm_timer->isActive())
    {
        m_wait_changes_start = 0;
        m_pwm_timer->start();
    }
}

void PIDHelper::calculatePID(uint current_time)
{
    Q_UNUSED(current_time)

    auto sensor = group()->value(m_sensor_type);
    if (sensor->status() != ItemGroup::valUnknown)
    {
        double temperature = sensor->value();

//        if (m_lastCalculate == 0)
//            m_lastCalculate = current_time - m_pid->period();

//        uint new_period = current_time - m_lastCalculate;
//        if (new_period == 0)
//            new_period = 1;

        //        pid.setPeriod(new_period);
        m_pwm_on = m_pid->calculate(temperature);

//        m_lastCalculate = current_time;

//        qDebug() << "Секция" << section()->id()
//                 << "Надо" << m_pid->wantPoint()
//                 << "Сейчас" << temperature
//                 << "Период" << m_pwm_timer->interval()
//                 << "ШИМ" << m_pwm_on;
    }
    else
        m_pwm_on = 0.;
}

void PIDHelper::onPWM()
{
    if (m_rest || m_pid->wantPoint() == 0.)
        return;

    uint current_time = QDateTime::currentDateTime().toTime_t();

    if (m_step == 0)
        calculatePID( current_time );
    m_step = (m_step + 1) % 2;

    bool state = group()->isControlOn(type());

    // Init Timer
    {
        double interval = state ? m_pid->period() - m_pwm_on : m_pwm_on;

        if (interval == m_pid->min() || interval == m_pid->period())
        {
            m_step = 0;
            interval = m_pid->period();
        }

        m_pwm_timer->setInterval(interval * 1000);
    }

    bool change_flag = false, new_state;
    if ( (m_pwm_on > m_pid->min() && m_pwm_on < m_pid->period()) ||
         ((m_pwm_on == m_pid->min()) == state) )
    {
        change_flag = true;
        new_state = !state;
    }

//    qDebug() << "onPWM new_state" << (new_state? *new_state ? "TRUE" : "FALSE" : "UNINIT")
//             << "interval" << m_pwm_timer->interval() << "now state" << state;

    if (change_flag)
    {
        m_wait_changes_start = current_time;
        writeToControl(new_state);
    }
    else
        startPWM();
}

void PIDHelper::check()
{
//    qDebug() << m_pwm_timer->isActive() << m_pwm_timer->interval() << m_pwm_timer->remainingTime() << m_wait_changes_start << QDateTime::currentDateTime().toTime_t();
    if ( !m_pwm_timer->isActive() &&
         (m_wait_changes_start == 0 ||
          (QDateTime::currentDateTime().toTime_t() - m_wait_changes_start) > 30) )
        onPWM();
}

void PIDHelper::setRest(bool rest) { if (m_rest != rest) m_rest = rest; }
bool PIDHelper::rest() const { return m_rest; }

double PIDHelper::pwm_on() const { return m_pwm_on; }

} // namespace Dai
