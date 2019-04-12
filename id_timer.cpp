#include "id_timer.h"

namespace Dai {

ID_Timer::ID_Timer(int id, QObject *parent) : QTimer(parent), timer_id_(id)
{
    QObject::connect(this, &QTimer::timeout, [this]()
    {
        emit timeout(timer_id_);
    });
}

} // namespace Dai
