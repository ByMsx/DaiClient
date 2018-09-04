#include <QDebug>
#include <algorithm>

#include "pidcontroller.h"

namespace Dai
{

double valueInRange(double value, double threshold)
{
    if (value > threshold)
        return threshold;
    else if (value < -threshold)
        return -threshold;
    return value;
}

PIDController::PIDController(QObject *parent) :
    PIDController{ 0., 0., 0., 0., parent }
{
}

PIDController::PIDController(double proportional, double integral, double differential,
                             double maxSaturationOrPeriod, QObject *parent)
    : QObject(parent),
      m_proportional(proportional),
      m_integral(integral),
      m_differential(differential)
{
    setPeriod(maxSaturationOrPeriod);
}

void PIDController::setWantPoint(double want)
{
    if (m_wantPoint != want)
        m_wantPoint = want;
}
double PIDController::wantPoint() const { return m_wantPoint; }

double PIDController::proportional() const { return m_proportional; }
void PIDController::setProportional(double proportional) { m_proportional = proportional; }

double PIDController::integral() const { return m_integral; }
void PIDController::setIntegral(double integral) { m_integral = integral; }

double PIDController::differential() const { return m_differential; }
void PIDController::setDifferential(double differential) { m_differential = differential; }

double PIDController::period() const { return m_period; }
void PIDController::setPeriod(double period) { m_period = period > 1 ? period : 1; }

double PIDController::min() const { return m_min; }
void PIDController::setMin(double minSaturation) { m_min = minSaturation; }

double PIDController::calculate(double input)
{
    // Calculate Error
    double error = m_wantPoint - input;
    
    // Proportional term
    double proportional_out = m_proportional * error;
    
    // Integral term
    double integral_error = valueInRange(error, 1.5);

    m_integral_out += integral_error * m_period / 60.;
    m_integral_out = valueInRange(m_integral_out, 200.);

    double integral_out = m_integral * m_integral_out;
    
    // Derivative term
    double derivative = (error - m_lastError) / m_period;
    double differential_out = m_differential * 60. * derivative;
    
    // Calculate total output
    double output = proportional_out + integral_out + differential_out;
    
//    qDebug() << "ШИМ" << output
//             << "period" << period()
//             << '[' << proportional() << ',' << integral() << ',' << differential() << ']'
//             << '[' << proportional_out << ',' << integral_out << ',' << differential_out << ']';

    // Restrict to max/min
    if ( output > m_period )
        output = m_period;
    else if ( output < m_min )
        output = m_min;
    
    // Save error to previous error
    m_lastError = error;
    
    return output;
}

} // namespace Dai
