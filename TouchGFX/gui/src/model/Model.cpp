#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <string.h>

extern "C"
{
#include "main.h"               /* USER_BUTTON_Pin, USER_BUTTON_GPIO_Port, HAL */
#include "pattern_storage.h"
}

/* Giữ nút bao lâu thì vào chế độ đăng ký. */
#define HOLD_MS  3000u

Model::Model()
    : modelListener(0),
      storedLen(0),
      patternSet(false),
      buttonWasDown(false),
      buttonDownTick(0),
      registerFired(false)
{
    memset(storedDots, 0, sizeof(storedDots));
    /* Nạp pattern từ Flash khi khởi động (chỉ đọc bộ nhớ, an toàn). */
    patternSet = PatternStorage_Load(storedDots, &storedLen);
}

void Model::tick()
{
    /* USER_BUTTON (PA0) trên Discovery là active-high: nhấn -> mức cao. */
    bool down = (HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin) == GPIO_PIN_SET);
    uint32_t now = HAL_GetTick();

    if (down)
    {
        if (!buttonWasDown)
        {
            buttonWasDown = true;
            buttonDownTick = now;
            registerFired = false;
        }
        else if (!registerFired && (now - buttonDownTick) >= HOLD_MS)
        {
            registerFired = true;            /* chỉ bắn một lần mỗi lần giữ */
            if (modelListener != 0)
            {
                modelListener->notifyRegisterRequested();
            }
        }
    }
    else
    {
        buttonWasDown = false;
    }
}

bool Model::verifyPattern(const uint8_t* dots, uint8_t len) const
{
    if (!patternSet || len != storedLen)
    {
        return false;
    }
    for (uint8_t i = 0; i < len; i++)
    {
        if (dots[i] != storedDots[i])
        {
            return false;
        }
    }
    return true;
}

bool Model::savePattern(const uint8_t* dots, uint8_t len)
{
    if (!PatternStorage_Save(dots, len))
    {
        return false;
    }
    memset(storedDots, 0, sizeof(storedDots));
    memcpy(storedDots, dots, len);
    storedLen = len;
    patternSet = true;
    return true;
}
