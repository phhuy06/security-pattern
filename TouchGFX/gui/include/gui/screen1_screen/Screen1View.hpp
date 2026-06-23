#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void handleClickEvent(const touchgfx::ClickEvent& evt);
    virtual void handleDragEvent(const touchgfx::DragEvent& evt);
    virtual void handleTickEvent();

    /* Gọi bởi Presenter khi người dùng giữ nút BOOT 3s. */
    void startRegistration();

protected:
    enum AppState
    {
        ST_LOCKED,             /* khóa, chờ nhập pattern mở khóa            */
        ST_REGISTER_FIRST,     /* đăng ký: vẽ pattern mới                   */
        ST_REGISTER_CONFIRM,   /* đăng ký: vẽ lại để xác nhận               */
        ST_UNLOCKED            /* mở khóa thành công, đang hiện thông báo   */
    };

    /* Hành động trễ chạy khi autoTimer về 0. */
    enum Pending
    {
        P_NONE,
        P_LOCK,                /* quay về trạng thái khóa                   */
        P_REG_AGAIN,           /* bắt đầu lại đăng ký từ bước vẽ            */
        P_CONFIRM_CLEAR,       /* xóa nét, giữ bước xác nhận                 */
        P_SAVE                 /* ghi pattern vào Flash (blocking)          */
    };

    /* Con trỏ tới widget của lớp base (gán trong setupScreen). */
    touchgfx::Circle*       circles[9];
    touchgfx::PainterRGB565* circlePainters[9];
    touchgfx::Line*         lines[8];
    touchgfx::PainterRGB565* linePainters[8];

    AppState state;
    Pending  pending;
    int      autoTimer;        /* số tick còn lại trước khi chạy pending    */

    uint8_t seq[9];            /* chuỗi điểm đang vẽ                        */
    uint8_t seqLen;
    bool    inSeq[9];          /* điểm đã nằm trong chuỗi chưa              */
    bool    dragging;

    uint8_t firstReg[9];       /* pattern lần vẽ đầu khi đăng ký            */
    uint8_t firstRegLen;

    /* Logic vẽ / hit-test. */
    void beginSequence(int16_t x, int16_t y);
    void processPoint(int16_t x, int16_t y);
    void addDot(uint8_t idx);
    int  hitDot(int16_t x, int16_t y) const;
    int  middleDot(uint8_t a, uint8_t b) const;
    void finishSequence();

    /* Chuyển trạng thái / hiển thị. */
    void resetToLocked();
    void clearDrawing();
    void runPending();
    void doSave();
    void setStatus(const char* msg);
    void setDotColor(uint8_t idx, touchgfx::colortype color);
    void colorWholePattern(touchgfx::colortype color);
};

#endif // SCREEN1VIEW_HPP
