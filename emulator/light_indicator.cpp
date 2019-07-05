#include <QPainter>

#include "light_indicator.h"

namespace Dai {

Light_Indicator::Light_Indicator(QWidget *parent) :
    QWidget(parent), state_(false)
{
}

void Light_Indicator::toggle()
{
    state_ = !state_;
    update();
}

void Light_Indicator::set_state(bool state)
{
    if (state_ != state)
    {
        state_ = state;
        update();
    }
}

void Light_Indicator::on()
{
    set_state(true);
}

void Light_Indicator::off()
{
    set_state(false);
}

void Light_Indicator::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRadialGradient gr(rect().center(), rect().width()/2);
//    gr.setColorAt(0.0, QColor(118, 214, 130));

    int hue = 123, lightness = 50;
    if (state_)
    {
        hue = 50;
        lightness = 254;
    }

    gr.setColorAt(0.0, QColor::fromHsl(hue, 254, 235 ));
    gr.setColorAt(1.0, QColor::fromHsl(hue, 254, lightness));

    p.setBrush(gr);
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));
}

} // namespace Dai
