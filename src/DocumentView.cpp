#include "DocumentView.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include <d2d1.h>

#include <winuser.rh>

namespace{
std::ostream& operator<<(std::ostream& out, const D2D1_RECT_F& rect)
{
    return out << rect.left << ' ' << rect.top << ' ' << rect.right << ' ' << rect.bottom << std::endl;
}

constexpr wchar_t* DocumentViewClassName = L"DIRECT2DDOCUMENTVIEW";

LRESULT DocumentViewProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CDocumentView* documentView = nullptr;
    if (msg == WM_CREATE)
    {
        auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        documentView = reinterpret_cast<CDocumentView*>(createStruct->lpCreateParams);
        assert(documentView != nullptr);
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(documentView));
        documentView->AttachHandle(window);
        documentView->SetModel(new CDocumentModel());
    }
    else
    {
        documentView = reinterpret_cast<CDocumentView*>(GetWindowLongPtr(window, GWLP_USERDATA));
        if (documentView != nullptr && documentView->HandleMessage(msg, wParam, lParam))
        {
            return 0;
        }
    }
    
    return DefWindowProc(window, msg, wParam, lParam);
}

void RegisterDocumentViewClass()
{
    static std::optional<WNDCLASSEX> viewClass;
    if (!viewClass.has_value())
    {
        viewClass = WNDCLASSEX{};
        auto& ref = *viewClass;
        ref.cbSize = sizeof(WNDCLASSEX);
        ref.style = CS_VREDRAW | CS_HREDRAW;
        ref.lpfnWndProc = DocumentViewProc;
        ref.cbClsExtra = 0;
        ref.cbWndExtra = 0;
        ref.hInstance = GetModuleHandle(NULL);
        ref.hIcon = nullptr;
        ref.hCursor = (HCURSOR) LoadImage(NULL, MAKEINTRESOURCE(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED);
        ref.hbrBackground = (HBRUSH) GetStockObject( LTGRAY_BRUSH );
        ref.lpszMenuName = nullptr;
        ref.lpszClassName = DocumentViewClassName;
        ref.hIconSm = nullptr;

        if (RegisterClassEx(&ref) == 0)
        {
            throw std::runtime_error("CDocumentView::CDocumentView; RegisterClassEx");
        }
    }
}

}

CDocumentView::CDocumentView()
{
    RegisterDocumentViewClass();
    if (CreateWindowEx(WS_EX_ACCEPTFILES, // EX STYLES
                DocumentViewClassName, // CLASS NAME
                L"Documents viewer", // WINDOW NAME
                WS_OVERLAPPEDWINDOW, // DEF STYLES
                0, // X
                0, // Y
                600, // W
                800, // H
                NULL, // PARENT
                NULL, // MENU
                GetModuleHandle(NULL), // INSTANCE
                this) // ADDITIONAL PARAMS
                    == nullptr)
    {
        throw std::runtime_error("CDocumentView::CDocumentView; CreateWindowEx");
    }

}

CDocumentView::~CDocumentView() = default;

void CDocumentView::AttachHandle(HWND _window)
{
    this->window = _window;

    d2dFactory.Reset();

    assert(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory.ptr) == S_OK);
}

void CDocumentView::Show()
{
    ShowWindow(window, SW_NORMAL);
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
        {WM_PAINT, &CDocumentView::OnDraw},
        {WM_ERASEBKGND, &CDocumentView::OnDraw},
        {WM_SIZE, &CDocumentView::OnSize},
        //{WM_SIZING, &CDocumentView::OnSizing}
        {WM_MOUSEWHEEL, &CDocumentView::OnScroll},
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
    auto size = D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top);

    if (renderTarget == nullptr) {
        createDependentResources(size);
    }

    CComPtrOwner<ID2D1SolidColorBrush> brush = nullptr;
    assert(renderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Goldenrod),
                &brush.ptr) == S_OK);

    CComPtrOwner<ID2D1SolidColorBrush> scrollBrush = nullptr;
    assert(renderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Gray),
                &scrollBrush.ptr) == S_OK);

    D2D1_RECT_F drawRect{rect.left, rect.top, rect.right, rect.bottom};

    resize(size.width, size.height);
    
    renderTarget->BeginDraw();

    renderTarget->FillRectangle(&drawRect, brush);

    if (model != nullptr)
    {
        float pageOffset = 0.0f;
        for(int i = 0; i < model->GetPageCount(); ++i)
        {
            auto page = reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole));
            if (page != nullptr)
            {
                const auto pageSize = page->GetPageSize();
                
                const D2D1_RECT_F d2dPageRect{0,0, pageSize.cx, pageSize.cy};

                double pageScale = std::min((double)size.width / pageSize.cx, 1.0);
                const D2D1_RECT_F d2dScaledRect{
                    0.0,
                    pageOffset,
                    pageSize.cx * pageScale,
                    pageOffset + pageSize.cy * pageScale};
                
                renderTarget->DrawBitmap(page->GetPageBitmap(),
                    d2dScaledRect,
                    1.0,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    NULL);
                pageOffset += d2dScaledRect.bottom - d2dScaledRect.top;
            }
        }
        surfaceState.vScrollPos = std::clamp(surfaceState.vScrollPos.load(), (int)size.height - (int)pageOffset, 0);
        surfaceState.zoom = std::max(surfaceState.zoom, 0.0);

        // scrollRect
        {
            int totalSurfaceHeight = pageOffset;
            int scrollHeight = size.height / (double)(totalSurfaceHeight) * size.height;
            const int scrollWidth = 5;
            int scrollTop = -surfaceState.vScrollPos - (double)(surfaceState.vScrollPos) / (totalSurfaceHeight) * size.height;

            D2D1_RECT_F scrollRect{drawRect.right - scrollWidth - 2, scrollTop, drawRect.right - 2, scrollTop + scrollHeight};
            D2D1_ROUNDED_RECT scrollDrawRect{scrollRect, 4.0, 4.0};
            renderTarget->FillRoundedRectangle(scrollDrawRect, scrollBrush);
        }
    }
    auto zoom  = D2D1::Matrix3x2F::Identity();//D2D1::Matrix3x2F::Scale(surfaceState.zoom, surfaceState.zoom);
    auto scroll = D2D1::Matrix3x2F::Translation(surfaceState.hScrollPos, surfaceState.vScrollPos);
    renderTarget->SetTransform(scroll * zoom);
    renderTarget->EndDraw();
}

void CDocumentView::OnSize(WPARAM, LPARAM lParam)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);
    resize(width, height);
}

void CDocumentView::OnSizing(WPARAM, LPARAM lParam)
{
    auto asRect = (RECT*)lParam;

    int newWidth = asRect->right - asRect->left - GetSystemMetrics(SM_CXVSCROLL);
    int newHeight = asRect->bottom - asRect->top - GetSystemMetrics(SM_CXHSCROLL);
    
    resize(newWidth, newHeight);
}

void CDocumentView::OnScroll(WPARAM wParam, LPARAM)
{
    int mouseDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (LOWORD(wParam) == MK_CONTROL) {
        surfaceState.zoom += (double)(mouseDelta) / 100;
    } else {
        surfaceState.vScrollPos += mouseDelta;
    }
}

void CDocumentView::createDependentResources(const D2D1_SIZE_U& size)
{
    std::cout << __PRETTY_FUNCTION__ << "\n";
    renderTarget.Reset();

    assert(d2dFactory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(window, size),
                &renderTarget.ptr) == S_OK);

    if (model != nullptr)
    {
        model->CreateObjects(renderTarget);
    }
}

void CDocumentView::resize(int width, int height)
{
    if (renderTarget != nullptr)
    {
        assert(renderTarget->Resize(D2D1_SIZE_U{width, height}) == S_OK);
    }
}
