#ifndef RESTHELPER_H
#define RESTHELPER_H

#include <Dai/timerange.h>

#include "automationhelper.h"

class QTimer;

namespace Dai {

class Section;
class DeviceItem;

class RestHelper : public AutomationHelperItem {
    Q_OBJECT
    Q_PROPERTY(uint workTime READ workTime WRITE setWorkTime)
    Q_PROPERTY(uint restTime READ restTime WRITE setRestTime)

    Q_PROPERTY(bool resting READ resting)
    Q_PROPERTY(bool alwaysWork READ alwaysWork WRITE setAlwaysWork)
public:
    RestHelper(ItemGroup* group);

    uint workTime() const;
    uint restTime() const;

    bool resting();

    bool alwaysWork() const;

public slots:
    void setWorkTime(uint workTime);
    void setRestTime(uint restTime);

    void setAlwaysWork(bool alwaysWork);

private:
    void checkParam(uint time, bool rest);
    void controlChanged(DeviceItem* dev_item);
    void onTimer();

    QTimer* m_timer;

    uint m_timePassed = 0;

    bool m_resting = false;
    quint64 m_start_time = 0;
    bool m_alwaysWork = true;

    enum State {
        Uninitialized
    };

    State state{};

    TimeRange m_timeRange;
};

} // namespace Dai

#endif // RESTHELPER_H
