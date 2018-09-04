#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <vector>
#include <QObject>

namespace Dai
{

class PIDController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double wantPoint READ wantPoint WRITE setWantPoint)
    Q_PROPERTY(double proportional READ proportional WRITE setProportional)
    Q_PROPERTY(double integral READ integral WRITE setIntegral)
    Q_PROPERTY(double differential READ differential WRITE setDifferential)
    Q_PROPERTY(double period READ period WRITE setPeriod)
    Q_PROPERTY(double min READ min WRITE setMin)
public:
    PIDController(QObject* parent = nullptr);
    PIDController(double proportional, double integral, double differential,
                  double maxSaturationOrPeriod, QObject* parent = nullptr);

    void setWantPoint(double want);
    double wantPoint() const;

    double proportional() const;
    void setProportional(double proportional);

    double integral() const;
    void setIntegral(double integral);

    double differential() const;
    void setDifferential(double differential);

    double period() const;
    void setPeriod(double period);

    double min() const;
    void setMin(double minSaturation);
public slots:
    double calculate(double input);
private:
    double m_proportional;
    double m_integral;
    double m_integral_out = 0;
    double m_differential;
    double m_period = 0;
    double m_min = 0;
    double m_wantPoint = 0;
    double m_lastError = 0;
};

} // namespace Dai

#endif /* PID_CONTROLLER_H */
