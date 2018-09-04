#ifndef SEVERALTIMESHELPER_H
#define SEVERALTIMESHELPER_H

#include "Scripts/tools/automationhelper.h"

class QTimer;

typedef unsigned int uint;

namespace Dai {

class SeveralTimesHelper : public AutomationHelperItem {
    Q_OBJECT
    Q_PROPERTY(uint count READ count WRITE setCount)
    Q_PROPERTY(uint start READ start WRITE setStart)
    Q_PROPERTY(uint duration READ duration WRITE setDuration)
public:
    SeveralTimesHelper(ItemGroup* group);

    uint count() const;
    uint start() const;
    uint duration() const;

    int period() const;
public slots:
    void activate();
    void deactivate();

    void setCount(uint count);
    void setStart(uint start);
    void setDuration(uint duration);
private:
    void onTimer();
    void onTimerOff();

    QTimer* m_timer;
    QTimer* m_timerOff;
    uint m_count = 1;
    uint m_start = 0;
    uint m_duration = 5;
};

} // namespace Dai

#endif // SEVERALTIMESHELPER_H
