#include <gui/screen2_screen/Screen2View.hpp>
#include <touchgfx/Color.hpp>
#include <touchgfx/Unicode.hpp>

using namespace touchgfx;

Screen2View::Screen2View() : tickCounter(0)
{
}

void Screen2View::setupScreen()
{
    Screen2ViewBase::setupScreen();
    tickCounter = 0;

    Model::NotifyType type = presenter->getNotifyType();

    switch (type)
    {
    case Model::NOTIFY_UNLOCK_OK:
        Unicode::strncpy(txtNotificationBuffer, "MO KHOA THANH CONG!", 40);
        txtNotification.setColor(Color::getColorFromRGB(0, 200, 80));
        break;

    case Model::NOTIFY_REG_OK:
        Unicode::strncpy(txtNotificationBuffer, "DANG KY THANH CONG!", 40);
        txtNotification.setColor(Color::getColorFromRGB(0, 170, 255));
        break;

    default:
        Unicode::strncpy(txtNotificationBuffer, "SUCCESS!", 40);
        break;
    }

    txtNotification.resizeToCurrentText();
    txtNotification.setX((int16_t)(120 - txtNotification.getWidth() / 2));
    txtNotification.invalidate();
}

void Screen2View::tearDownScreen()
{
    Screen2ViewBase::tearDownScreen();
}

void Screen2View::handleTickEvent()
{
    tickCounter++;
    if (tickCounter >= 180)
    {
        tickCounter = 0;
        application().gotoScreen1ScreenNoTransition();
    }
}
