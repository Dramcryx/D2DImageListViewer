#ifndef D2DILV_DIRECT2D_MATRIX_SWITCHER_H
#define D2DILV_DIRECT2D_MATRIX_SWITCHER_H

#include <d2d1.h>

class CDirect2DMatrixSwitcher {
public:
    explicit CDirect2DMatrixSwitcher(ID2D1RenderTarget* _target, const D2D1_MATRIX_3X2_F& newTransform) :
        target{_target}
    {
        target->GetTransform(&oldTransform);
        target->SetTransform(newTransform);
    }

    ~CDirect2DMatrixSwitcher()
    {
        target->SetTransform(oldTransform);
    }

private:
    ID2D1RenderTarget* target;
    D2D1_MATRIX_3X2_F oldTransform;
};

#endif
