#include "DocumentView.h"

#include "DocumentViewPrivate.h"

#include <Direct2DMatrixSwitcher.h>
#include <IDocumentPage.h>

#include <cassert>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include <d2d1.h>

#include <winuser.rh>

std::ostream& operator<<(std::ostream& out, const D2D1_RECT_F& rect)
{
    return out << rect.left << ' ' << rect.top << ' ' << rect.right << ' ' << rect.bottom << std::endl;
}

namespace {

const wchar_t* DocumentViewClassName = L"DIRECT2DDOCUMENTVIEW";

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
        ref.style = 0;
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

CDocumentView::CDocumentView(HWND parent)
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
                parent, // PARENT
                NULL, // MENU
                GetModuleHandle(NULL), // INSTANCE
                this) // ADDITIONAL PARAMS
                    == nullptr)
    {
        throw std::runtime_error("CDocumentView::CDocumentView; CreateWindowEx");
    }
    helper.reset(new DocumentViewPrivate::CDocumentLayoutHelper{});
}

CDocumentView::~CDocumentView() = default;

void CDocumentView::AttachHandle(HWND _window)
{
    this->window = _window;

    d2dFactory.Reset();

    OK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory.ptr));
}

void CDocumentView::Show()
{
    ShowWindow(window, SW_NORMAL);
}

void CDocumentView::SetModel(IDocumentModel* _model)
{
    this->surfaceProps.model.reset(_model);
}

IDocumentModel* CDocumentView::GetModel() const
{
    return this->surfaceProps.model.get();
}

bool CDocumentView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::unordered_map<UINT, std::function<void(CDocumentView*, WPARAM, LPARAM)>> messageHandlers {
        {WM_PAINT, &CDocumentView::OnDraw},
        //{WM_ERASEBKGND, &CDocumentView::OnDraw},
        {WM_SIZE, &CDocumentView::OnSize},
        //{WM_SIZING, &CDocumentView::OnSize},
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
    PAINTSTRUCT ps;
    BeginPaint(window, &ps);

    RECT rect = ps.rcPaint;
    auto size = D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top);

    auto& renderTarget = surfaceProps.renderTarget;
    if (renderTarget == nullptr) {
        createDependentResources(size);
    }

    D2D1_RECT_F drawRect{rect.left, rect.top, rect.right, rect.bottom};

    D2D1_SIZE_F sizeF{float(size.width), float(size.height)};
    auto currentSizeF = renderTarget->GetSize();

    if (sizeF != currentSizeF) {
        resize(size.width, size.height);
    }

    renderTarget->BeginDraw();
    renderTarget->Clear(surfaceProps.backgroundBrush->GetColor());

    if (surfaceProps.model != nullptr)
    {
        const auto& surfaceLayout = helper->GetOrCreateLayout(DocumentViewPrivate::CDocumentPagesLayoutParams{
                    [this, sizeF]() { return D2D1_SIZE_F{sizeF.width / surfaceState.zoom, sizeF.height / surfaceState.zoom};}(),
                    5,
                    -5,
                    DocumentViewPrivate::CDocumentPagesLayoutParams::HorizontalFlow
        }, surfaceProps.model.get());

        int totalSurfaceWidth = surfaceLayout.totalSurfaceSize.width * surfaceState.zoom;
        int visibleSurfaceWidth = size.width;
        float hVisibleToTotal = static_cast<float>(visibleSurfaceWidth) / static_cast<float>(totalSurfaceWidth);
        surfaceState.hScrollPos = std::clamp(surfaceState.hScrollPos, -1.0f + hVisibleToTotal, 0.0f);

        int totalSurfaceHeight = surfaceLayout.totalSurfaceSize.height * surfaceState.zoom;
        int visibleSurfaceHeight = size.height;
        float vVisibleToTotal = static_cast<float>(visibleSurfaceHeight) / static_cast<float>(totalSurfaceHeight);
        surfaceState.vScrollPos = std::clamp(surfaceState.vScrollPos, -1.0f + vVisibleToTotal, 0.0f);

        {
            CDirect2DMatrixSwitcher switcher{
                surfaceProps.renderTarget,
                D2D1::Matrix3x2F::Scale(surfaceState.zoom, surfaceState.zoom)
                    * D2D1::Matrix3x2F::Translation(totalSurfaceWidth * surfaceState.hScrollPos, totalSurfaceHeight * surfaceState.vScrollPos)};

            for (auto& pageLayout : surfaceLayout.pageRects) {
                renderTarget->DrawBitmap(
                    pageLayout.first->GetPageBitmap(),
                    pageLayout.second,
                    1.0,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    NULL
                );

                renderTarget->DrawRectangle(pageLayout.second, surfaceProps.pageFrameBrush, 1.0 / surfaceState.zoom, nullptr);
            }
        }

        auto scrollBarRects = helper->GetOrCreateScrollBarRects(sizeF, surfaceLayout.totalSurfaceSize);
        if (scrollBarRects.hScrollBar.has_value() && hVisibleToTotal < 1.0f) {
            auto& rect = scrollBarRects.hScrollBar->rect;
            rect.left -= visibleSurfaceWidth * surfaceState.hScrollPos;
            rect.right -= visibleSurfaceWidth * surfaceState.hScrollPos;
            rect.top += visibleSurfaceHeight;
            rect.bottom += visibleSurfaceHeight;

            renderTarget->FillRoundedRectangle(*scrollBarRects.hScrollBar, surfaceProps.scrollBarBrush);
        }
        if (scrollBarRects.vScrollBar.has_value() && vVisibleToTotal < 1.0f) {
            auto& rect = scrollBarRects.vScrollBar->rect;
            rect.left += visibleSurfaceWidth;
            rect.right += visibleSurfaceWidth;
            rect.top -= visibleSurfaceHeight * surfaceState.vScrollPos ;
            rect.bottom -= visibleSurfaceHeight * surfaceState.vScrollPos;
            renderTarget->FillRoundedRectangle(*scrollBarRects.vScrollBar, surfaceProps.scrollBarBrush);
        }
    }
    
    if (renderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
    {
        createDependentResources(size);
    }
    EndPaint(window, &ps);
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

void CDocumentView::OnScroll(WPARAM wParam, LPARAM lParam)
{
    int mouseDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (LOWORD(wParam) == MK_CONTROL) {
        surfaceState.zoom += mouseDelta > 0 ? 0.1f : -0.1f;
        surfaceState.zoom = std::max(surfaceState.zoom, 0.1f);
    } else {
        POINT screenPoint{LOWORD(lParam), HIWORD(lParam)};
        ScreenToClient(window, &screenPoint);

        if (surfaceProps.renderTarget != nullptr && surfaceProps.renderTarget->GetSize().height - screenPoint.y < 10) {
            surfaceState.hScrollPos +=  mouseDelta > 0 ? 0.05 : -0.05;
        } else {
            surfaceState.vScrollPos += mouseDelta > 0 ? 0.05 : -0.05;
        }
    }
    assert(InvalidateRect(window, nullptr, false));
}

void CDocumentView::createDependentResources(const D2D1_SIZE_U& size)
{
    surfaceProps.renderTarget.Reset();
    surfaceProps.backgroundBrush.Reset();
    surfaceProps.pageFrameBrush.Reset();
    surfaceProps.scrollBarBrush.Reset();

    OK(d2dFactory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(window, size),
                &surfaceProps.renderTarget.ptr));

    OK(surfaceProps.renderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::WhiteSmoke),
                &surfaceProps.backgroundBrush.ptr));

    OK(surfaceProps.renderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Black),
                &surfaceProps.pageFrameBrush.ptr));

    OK(surfaceProps.renderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Gray),
                &surfaceProps.scrollBarBrush.ptr));
    surfaceProps.scrollBarBrush->SetOpacity(0.5f);

    if (surfaceProps.model != nullptr)
    {
        surfaceProps.model->CreateObjects(surfaceProps.renderTarget);
    }
}

void CDocumentView::resize(int width, int height)
{
    if (surfaceProps.renderTarget != nullptr)
    {
        OK(surfaceProps.renderTarget->Resize(D2D1_SIZE_U{width, height}));
        assert(InvalidateRect(window, nullptr, false));
    }
}
