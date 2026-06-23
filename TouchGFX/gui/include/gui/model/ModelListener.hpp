#ifndef MODELLISTENER_HPP
#define MODELLISTENER_HPP

#include <gui/model/Model.hpp>

class ModelListener
{
public:
    ModelListener() : model(0) {}
    
    virtual ~ModelListener() {}

    void bind(Model* m)
    {
        model = m;
    }

    /* Model báo: người dùng đã giữ nút BOOT đủ lâu -> yêu cầu vào chế độ đăng ký. */
    virtual void notifyRegisterRequested() {}
protected:
    Model* model;
};

#endif // MODELLISTENER_HPP
