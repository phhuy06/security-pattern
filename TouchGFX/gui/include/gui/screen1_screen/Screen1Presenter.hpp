#ifndef SCREEN1PRESENTER_HPP
#define SCREEN1PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Screen1View;

class Screen1Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    Screen1Presenter(Screen1View& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    /* Chuyển yêu cầu đăng ký từ Model xuống View. */
    virtual void notifyRegisterRequested();

    /* Cầu nối tới Model cho View. */
    bool patternSet() const { return model->isPatternSet(); }
    bool verifyPattern(const uint8_t* dots, uint8_t len) const { return model->verifyPattern(dots, len); }
    bool savePattern(const uint8_t* dots, uint8_t len) { return model->savePattern(dots, len); }
    // Cầu nối gửi và nhận loại thông báo từ Model
    void setNotifyType(Model::NotifyType type) { model->setNotifyType(type); }
        Model::NotifyType getNotifyType() const    { return model->getNotifyType(); }
    virtual ~Screen1Presenter() {}

    bool isLockedOut() const {
    	return model->isLockedOut();
    }
    uint32_t getRemainingLockoutSeconds() const {
    	return model->getRemainingLockoutSeconds();
    }
    void incrementFailCount() {
    	model->incrementFailCount();
    }
    void resetFailCount() {
    	model->resetFailCount();
    }
    uint8_t getFailCount() const {
    	return model->getFailCount();
    }
private:
    Screen1Presenter();

    Screen1View& view;
};

#endif // SCREEN1PRESENTER_HPP
