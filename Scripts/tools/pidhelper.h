#ifndef PIDHELPER_H
#define PIDHELPER_H

#include "automationhelper.h"
#include "pidcontroller.h"

class QTimer;

namespace Dai
{

class PIDHelper : public AutomationHelperItem
{
    Q_OBJECT
    Q_PROPERTY(PIDController* pid READ pid)
    Q_PROPERTY(bool rest READ rest WRITE setRest)
public:
    PIDHelper(ItemGroup* group, uint sensor_type);

    PIDController* pid() const;

public slots:
    void startPWM();
    void calculatePID(uint current_time);
    void check();

    void setRest(bool rest);
    bool rest() const;

    double pwm_on() const;

private:
    void onPWM();

    uint m_sensor_type;
    PIDController* m_pid;
    QTimer* m_pwm_timer;

    double m_pwm_on = 0.;
    uchar m_step = 0;
    bool m_rest = false;
    uint m_wait_changes_start = 0;

//    quint64 m_lastCalculate = 0;
};

} // namespace Dai

#endif // PIDHELPER_H
