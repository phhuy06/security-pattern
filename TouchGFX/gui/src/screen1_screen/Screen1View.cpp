#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/Color.hpp>
#include <touchgfx/Unicode.hpp>
#include <string.h>

using namespace touchgfx;

/* Tâm 9 điểm (lưới 3x3), khớp toạ độ widget trong Designer. */
static const int16_t CX[9] = { 50, 120, 190,  50, 120, 190,  50, 120, 190 };
static const int16_t CY[9] = { 90,  90,  90, 160, 160, 160, 230, 230, 230 };

static const uint8_t MIN_LEN = 4;     /* số điểm tối thiểu của pattern      */
static const int HIT_R2 = 22 * 22;    /* bán kính bắt điểm (bình phương)    */

/* Thời lượng (đơn vị tick, ~60fps). */
static const int SUCCESS_TICKS    = 180;  /* ~3s hiện thông báo mở khóa     */
static const int ERROR_TICKS      = 90;   /* ~1.5s hiện báo lỗi             */
static const int SHOW_FIRST_TICKS = 60;   /* ~1s giữ pattern vừa vẽ         */
static const int SAVE_DELAY_TICKS = 2;    /* để khung "Dang luu..." kịp vẽ  */

namespace
{
inline colortype cActive()  { return Color::getColorFromRGB(0, 170, 255); }
inline colortype cSuccess() { return Color::getColorFromRGB(0, 200, 80);  }
inline colortype cError()   { return Color::getColorFromRGB(230, 40, 40); }
inline colortype cIdle()    { return Color::getColorFromRGB(255, 255, 255); }
}

Screen1View::Screen1View()
    : state(ST_LOCKED), pending(P_NONE), autoTimer(0),
      seqLen(0), dragging(false), firstRegLen(0)
{
    for (int i = 0; i < 9; i++)
    {
        inSeq[i] = false;
    }
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    circles[0] = &circle1; circles[1] = &circle2; circles[2] = &circle3;
    circles[3] = &circle4; circles[4] = &circle5; circles[5] = &circle6;
    circles[6] = &circle7; circles[7] = &circle8; circles[8] = &circle9;

    circlePainters[0] = &circle1Painter; circlePainters[1] = &circle2Painter; circlePainters[2] = &circle3Painter;
    circlePainters[3] = &circle4Painter; circlePainters[4] = &circle5Painter; circlePainters[5] = &circle6Painter;
    circlePainters[6] = &circle7Painter; circlePainters[7] = &circle8Painter; circlePainters[8] = &circle9Painter;

    lines[0] = &line1; lines[1] = &line2; lines[2] = &line3; lines[3] = &line4;
    lines[4] = &line5; lines[5] = &line6; lines[6] = &line7; lines[7] = &line8;

    linePainters[0] = &line1Painter; linePainters[1] = &line2Painter; linePainters[2] = &line3Painter; linePainters[3] = &line4Painter;
    linePainters[4] = &line5Painter; linePainters[5] = &line6Painter; linePainters[6] = &line7Painter; linePainters[7] = &line8Painter;

    /* Mỗi line dùng cả màn làm canvas -> setStart/setEnd theo toạ độ tuyệt đối. */
    for (int i = 0; i < 8; i++)
    {
        lines[i]->setPosition(0, 0, 240, 320);
        lines[i]->setLineWidth(8);
        lines[i]->setVisible(false);
    }

    resetToLocked();
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

/* ------------------------------------------------------------------ touch */

void Screen1View::handleClickEvent(const ClickEvent& evt)
{
    if (autoTimer > 0)
    {
        return;   /* đang hiện thông báo -> chặn nhập */
    }

    if (evt.getType() == ClickEvent::PRESSED)
    {
        if (state == ST_UNLOCKED)
        {
            return;
        }
        if (state == ST_LOCKED && !presenter->patternSet())
        {
            return;   /* chưa đăng ký pattern -> không cho mở khóa */
        }
        beginSequence(evt.getX(), evt.getY());
    }
    else if (evt.getType() == ClickEvent::RELEASED)
    {
        if (dragging)
        {
            finishSequence();
        }
    }
}

void Screen1View::handleDragEvent(const DragEvent& evt)
{
    if (dragging)
    {
        processPoint(evt.getNewX(), evt.getNewY());
    }
}

void Screen1View::handleTickEvent()
{
    if (autoTimer > 0)
    {
        autoTimer--;
        if (autoTimer == 0)
        {
            runPending();
        }
    }
}

/* ------------------------------------------------------------- vẽ pattern */

void Screen1View::beginSequence(int16_t x, int16_t y)
{
    clearDrawing();
    dragging = true;
    processPoint(x, y);
}

void Screen1View::processPoint(int16_t x, int16_t y)
{
    int idx = hitDot(x, y);
    if (idx < 0 || inSeq[idx])
    {
        return;
    }

    if (seqLen > 0)
    {
        int mid = middleDot(seq[seqLen - 1], (uint8_t)idx);
        if (mid >= 0 && !inSeq[mid])
        {
            addDot((uint8_t)mid);   /* tự chèn điểm trung gian */
        }
    }
    addDot((uint8_t)idx);
}

void Screen1View::addDot(uint8_t idx)
{
    inSeq[idx] = true;
    seq[seqLen] = idx;
    seqLen++;

    setDotColor(idx, cActive());

    if (seqLen >= 2)
    {
        uint8_t li = (uint8_t)(seqLen - 2);
        uint8_t a = seq[seqLen - 2];
        uint8_t b = seq[seqLen - 1];
        lines[li]->setStart(CX[a], CY[a]);
        lines[li]->setEnd(CX[b], CY[b]);
        linePainters[li]->setColor(cActive());
        lines[li]->setVisible(true);
        lines[li]->invalidate();
    }
}

int Screen1View::hitDot(int16_t x, int16_t y) const
{
    for (int i = 0; i < 9; i++)
    {
        int dx = x - CX[i];
        int dy = y - CY[i];
        if (dx * dx + dy * dy <= HIT_R2)
        {
            return i;
        }
    }
    return -1;
}

int Screen1View::middleDot(uint8_t a, uint8_t b) const
{
    int ra = a / 3, ca = a % 3;
    int rb = b / 3, cb = b % 3;
    int sr = ra + rb, sc = ca + cb;

    /* Điểm giữa tồn tại khi hai điểm cách nhau 2 ô trên cùng đường thẳng. */
    if ((sr % 2) == 0 && (sc % 2) == 0)
    {
        int mid = (sr / 2) * 3 + (sc / 2);
        if (mid != a && mid != b)
        {
            return mid;
        }
    }
    return -1;
}

void Screen1View::finishSequence()
{
    dragging = false;
    if (seqLen == 0)
    {
        return;   /* không chạm điểm nào */
    }

    switch (state)
    {
    case ST_LOCKED:
        if (seqLen >= MIN_LEN && presenter->verifyPattern(seq, seqLen))
        {
            colorWholePattern(cSuccess());
            setStatus("Mo khoa OK");
            state = ST_UNLOCKED;
            pending = P_LOCK;
            autoTimer = SUCCESS_TICKS;
        }
        else
        {
            colorWholePattern(cError());
            setStatus("Sai, thu lai");
            pending = P_LOCK;
            autoTimer = ERROR_TICKS;
        }
        break;

    case ST_REGISTER_FIRST:
        if (seqLen < MIN_LEN)
        {
            colorWholePattern(cError());
            setStatus("Toi thieu 4 diem");
            pending = P_REG_AGAIN;
            autoTimer = ERROR_TICKS;
        }
        else
        {
            memcpy(firstReg, seq, seqLen);
            firstRegLen = seqLen;
            colorWholePattern(cActive());
            setStatus("Ve lai de xac nhan");
            state = ST_REGISTER_CONFIRM;
            pending = P_CONFIRM_CLEAR;
            autoTimer = SHOW_FIRST_TICKS;
        }
        break;

    case ST_REGISTER_CONFIRM:
        if (seqLen == firstRegLen && memcmp(seq, firstReg, seqLen) == 0)
        {
            colorWholePattern(cActive());
            setStatus("Dang luu...");
            pending = P_SAVE;
            autoTimer = SAVE_DELAY_TICKS;
        }
        else
        {
            colorWholePattern(cError());
            setStatus("Khong khop");
            pending = P_REG_AGAIN;
            autoTimer = ERROR_TICKS;
        }
        break;

    default:
        break;
    }
}

/* --------------------------------------------------- trạng thái / hiển thị */

void Screen1View::runPending()
{
    Pending p = pending;
    pending = P_NONE;

    switch (p)
    {
    case P_LOCK:
        resetToLocked();
        break;
    case P_REG_AGAIN:
        clearDrawing();
        state = ST_REGISTER_FIRST;
        setStatus("Ve pattern moi");
        break;
    case P_CONFIRM_CLEAR:
        clearDrawing();           /* giữ nguyên ST_REGISTER_CONFIRM */
        setStatus("Ve lai de xac nhan");
        break;
    case P_SAVE:
        doSave();
        break;
    default:
        break;
    }
}

void Screen1View::doSave()
{
    if (presenter->savePattern(firstReg, firstRegLen))
    {
        colorWholePattern(cSuccess());
        setStatus("Da luu");
        state = ST_LOCKED;
        pending = P_LOCK;
        autoTimer = SUCCESS_TICKS;
    }
    else
    {
        colorWholePattern(cError());
        setStatus("Loi luu Flash");
        state = ST_REGISTER_FIRST;
        pending = P_REG_AGAIN;
        autoTimer = ERROR_TICKS;
    }
}

void Screen1View::startRegistration()
{
    if (state == ST_REGISTER_FIRST || state == ST_REGISTER_CONFIRM)
    {
        return;   /* đang đăng ký rồi */
    }
    clearDrawing();
    dragging = false;
    state = ST_REGISTER_FIRST;
    pending = P_NONE;
    autoTimer = 0;
    setStatus("Ve pattern moi");
}

void Screen1View::resetToLocked()
{
    clearDrawing();
    dragging = false;
    state = ST_LOCKED;
    pending = P_NONE;
    autoTimer = 0;
    if (presenter->patternSet())
    {
        setStatus("Nhap pattern");
    }
    else
    {
        setStatus("Giu BOOT 3s");
    }
}

void Screen1View::clearDrawing()
{
    for (int i = 0; i < 8; i++)
    {
        lines[i]->setVisible(false);
        lines[i]->invalidate();
    }
    for (int i = 0; i < 9; i++)
    {
        setDotColor((uint8_t)i, cIdle());
        inSeq[i] = false;
    }
    seqLen = 0;
}

void Screen1View::setStatus(const char* msg)
{
    txtStatus.invalidate();        /* xóa vùng cũ */
    Unicode::strncpy(txtStatusBuffer, msg, TXTSTATUS_SIZE);
    txtStatus.resizeToCurrentText();
    txtStatus.setX((int16_t)(120 - txtStatus.getWidth() / 2));
    txtStatus.invalidate();        /* vẽ vùng mới */
}

void Screen1View::setDotColor(uint8_t idx, colortype color)
{
    circlePainters[idx]->setColor(color);
    circles[idx]->invalidate();
}

void Screen1View::colorWholePattern(colortype color)
{
    for (uint8_t k = 0; k < seqLen; k++)
    {
        setDotColor(seq[k], color);
    }
    for (uint8_t li = 0; li + 1 < seqLen; li++)
    {
        linePainters[li]->setColor(color);
        lines[li]->invalidate();
    }
}
