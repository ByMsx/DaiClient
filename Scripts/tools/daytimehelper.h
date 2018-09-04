#ifndef DAYTIMEHELPER_H
#define DAYTIMEHELPER_H

#include <QTimer>

#include "automationhelper.h"

namespace Dai {

class DayTimeHelper : public QObject {
    Q_OBJECT
public:
    DayTimeHelper(SectionManager* house_mng, QObject* parent = nullptr);

    void stop();
signals:
    void onDayPartChanged(Section*, bool);
public slots:
    void init();

private slots:
    void onTimer();

private:
    SectionManager* house_mng;
    QTimer m_timer;
};

} // namespace Dai

#endif // DAYTIMEHELPER_H
