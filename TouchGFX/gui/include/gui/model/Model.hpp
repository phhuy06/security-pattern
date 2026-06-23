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

    /* Trạng thái pattern đã lưu trong Flash. */
    bool isPatternSet() const
    {
        return patternSet;
    }

    /* So khớp chuỗi điểm với pattern đã lưu. */
    bool verifyPattern(const uint8_t* dots, uint8_t len) const;

    /* Ghi pattern mới vào Flash (vĩnh viễn) và cập nhật bản RAM. */
    bool savePattern(const uint8_t* dots, uint8_t len);

protected:
    ModelListener* modelListener;

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
