#ifndef MODEL_HPP
#define MODEL_HPP

#include <stdint.h>

class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();
    enum NotifyType {
            NOTIFY_NONE,
            NOTIFY_UNLOCK_OK,       // Đăng nhập / Mở khóa thành công
            NOTIFY_REG_OK           // Đăng ký Pattern mới thành công
        };
    void setNotifyType(NotifyType type) { currentNotify = type; }
        NotifyType getNotifyType() const    { return currentNotify; }
    /* Trạng thái pattern đã lưu trong Flash. */
    bool isPatternSet() const
    {
        return patternSet;
    }

    /* So khớp chuỗi điểm với pattern đã lưu. */
    bool verifyPattern(const uint8_t* dots, uint8_t len) const;

    /* Ghi pattern mới vào Flash (vĩnh viễn) và cập nhật bản RAM. */
    bool savePattern(const uint8_t* dots, uint8_t len);
    void incrementFailCount();
    void resetFailCount();
    uint8_t getFailCount() const { return failCount; }

    bool isLockedOut() const;
    uint32_t getRemainingLockoutSeconds() const;

protected:
    ModelListener* modelListener;
    NotifyType currentNotify;
    uint8_t failCount;
    uint32_t lockoutEndTime;

private:
    /* Pattern đã lưu (bản RAM). */
    uint8_t storedDots[9];
    uint8_t storedLen;
    bool    patternSet;

    /* Phát hiện giữ nút BOOT (PA0) 3 giây. */
    bool     buttonWasDown;
    uint32_t buttonDownTick;
    bool     registerFired;
};

#endif // MODEL_HPP
