#include "DocumentView.h"

#include <cassert>
#include <functional>
#include <unordered_map>

#include <d2d1.h>

template<typename T>
struct CComPtrOwner
{
    T* ptr;

    CComPtrOwner(T* _ptr) : ptr{_ptr}
    {}

    ~CComPtrOwner()
    {
        if(ptr != nullptr)
        {
            ptr->Release();
        }
    }

    operator T*()
    {
        return ptr;
    }

    T* operator->()
    {
        return ptr;
    }
};

CDocumentView::~CDocumentView() = default;

void CDocumentView::AttachHandle(HWND _window)
{
    this->window = _window;

    if (d2dFactory != nullptr)
    {
        d2dFactory->Release();
        d2dFactory = nullptr;
    }

    assert(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory) == S_OK);
}

void CDocumentView::SetModel(CDocumentModel* _model)
{
    this->model.reset(_model);
}

CDocumentModel* CDocumentView::GetModel() const
{
    return this->model.get();
}

bool CDocumentView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::unordered_map<UINT, std::function<void(CDocumentView*, WPARAM, LPARAM)>> messageHandlers{
        {WM_PAINT, &CDocumentView::OnDraw}
    };

    auto findRes = messageHandlers.find(msg);
    if (findRes != messageHandlers.end())
    {
        findRes->second(this, wParam, lParam);
        return true;
    }
    return false;
}

void CDocumentView::OnDraw(WPARAM, LPARAM)
{
    RECT rect;
    auto clientRect = GetClientRect(window, &rect);
    D2D1_SIZE_U size = D2D1::SizeU(
            rect.right - rect.left,
            rect.bottom - rect.top
            );
    
    CComPtrOwner<ID2D1HwndRenderTarget> renderTarget = nullptr;
    assert(d2dFactory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(window, size),
                &renderTarget.ptr) == S_OK);

    CComPtrOwner<ID2D1SolidColorBrush> brush = nullptr;
    assert(renderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Goldenrod),
                &brush.ptr) == S_OK);

    D2D1_RECT_F drawRect{rect.left, rect.top, rect.right, rect.bottom};

    renderTarget->BeginDraw();

    renderTarget->FillRectangle(&drawRect, brush);


    renderTarget->EndDraw();
    renderTarget->Release();
}