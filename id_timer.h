#ifndef DAI_ID_TIMER_H
#define DAI_ID_TIMER_H

#include<QTimer>

namespace Dai
{

class ID_Timer: public QTimer
{
    Q_OBJECT
public:
     explicit ID_Timer(int id, QObject *parent = nullptr);

private:
    int timer_id_;
signals:
    void timeout(int id);
};

} // namespace Dai

#endif // DAI_ID_TIMER_H
