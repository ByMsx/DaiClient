#ifndef DAI_LIGHT_INDICATOR_H
#define DAI_LIGHT_INDICATOR_H

#include <QWidget>

namespace Dai {

class Light_Indicator : public QWidget
{
    Q_OBJECT
public:
    explicit Light_Indicator(QWidget *parent = nullptr);

signals:
public slots:
    void toggle();
    void set_state(bool state);
    void on();
    void off();
private:
    void paintEvent(QPaintEvent *) override;
    bool state_;
};

} // namespace Dai

#endif // DAI_LIGHT_INDICATOR_H
