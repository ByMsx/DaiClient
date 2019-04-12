#ifndef DAYTIMEHELPER_H
#define DAYTIMEHELPER_H

#include <QTimer>

#include "automationhelper.h"

namespace Dai {

class DayTimeHelper : public QObject {
    Q_OBJECT
public:
    DayTimeHelper(Project* prj, QObject* parent = nullptr);

    void stop();
signals:
    void onDayPartChanged(Section*, bool is_day);
public slots:
    void init();

private slots:
    void onTimer();

private:
    Project* prj;
    QTimer m_timer;
};

} // namespace Dai

#endif // DAYTIMEHELPER_H
