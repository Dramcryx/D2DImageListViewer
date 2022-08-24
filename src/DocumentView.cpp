#include "DocumentView.h"

#include "DocumentViewPrivate.h"

#include <Direct2DMatrixSwitcher.h>
#include <IDocumentPage.h>

#include <winuser.rh>

#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

std::ostream& operator<<(std::ostream& out, const D2D1_RECT_F& rect)
{
    return out << rect.left << ' ' << rect.top << ' ' << rect.right << ' ' << rect.bottom;
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

    this->d2dFactory.Reset();

    OK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory.ptr));
}

void CDocumentView::Show()
{
    ShowWindow(window, SW_NORMAL);
}

void CDocumentView::SetModel(IDocumentModel* _model)
{
    this->model.reset(_model);
}

IDocumentModel* CDocumentView::GetModel() const
{
    return this->model.get();
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
    BeginPaint(this->window, &ps);

    RECT rect = ps.rcPaint;
    auto size = D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top);

    auto& renderTarget = this->surfaceContext.renderTarget;
    if (renderTarget == nullptr) {
        this->createDependentResources(size);
    }

    D2D1_SIZE_F sizeF{float(size.width), float(size.height)};
    auto currentSizeF = renderTarget->GetSize();

    if (sizeF != currentSizeF) {
        resize(size.width, size.height);
    }

    renderTarget->BeginDraw();
    renderTarget->Clear(this->viewProperties.bkColor);

    if (this->model != nullptr)
    {
        const auto& surfaceLayout = this->helper->GetOrCreateLayout(
            DocumentViewPrivate::CDocumentPagesLayoutParams{
                    D2D1_SIZE_F{sizeF.width / viewProperties.zoom, sizeF.height / viewProperties.zoom},
                    5,
                    -5,
                    DocumentViewPrivate::CDocumentPagesLayoutParams::HorizontalFlow
            }, model.get());

        int totalSurfaceWidth = surfaceLayout.totalSurfaceSize.width * viewProperties.zoom;
        int visibleSurfaceWidth = size.width;
        float hVisibleToTotal = static_cast<float>(visibleSurfaceWidth) / static_cast<float>(totalSurfaceWidth);
        viewProperties.hScrollPos = std::clamp(viewProperties.hScrollPos, -1.0f + hVisibleToTotal, 0.0f);

        int totalSurfaceHeight = surfaceLayout.totalSurfaceSize.height * viewProperties.zoom;
        int visibleSurfaceHeight = size.height;
        float vVisibleToTotal = static_cast<float>(visibleSurfaceHeight) / static_cast<float>(totalSurfaceHeight);
        viewProperties.vScrollPos = std::clamp(viewProperties.vScrollPos, -1.0f + vVisibleToTotal, 0.0f);

        {
            CDirect2DMatrixSwitcher switcher{
                this->surfaceContext.renderTarget,
                D2D1::Matrix3x2F::Scale(viewProperties.zoom, viewProperties.zoom)
                    * D2D1::Matrix3x2F::Translation(totalSurfaceWidth * viewProperties.hScrollPos, totalSurfaceHeight * viewProperties.vScrollPos)};

            for (auto& pageLayout : surfaceLayout.pageRects) {
                renderTarget->DrawTextLayout(
                    {pageLayout.textRect.left, pageLayout.textRect.top},
                    pageLayout.textLayout.ptr,
                    surfaceContext.pageFrameBrush
                );
                renderTarget->DrawBitmap(
                    pageLayout.page->GetPageBitmap(),
                    pageLayout.pageRect,
                    1.0,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    NULL
                );

                renderTarget->DrawRectangle(pageLayout.pageRect, this->surfaceContext.pageFrameBrush, 1.0, nullptr);
            }
        }

        const auto& scrollBarRects = this->helper->GetOrCreateRelativeScrollBarRects(
            {totalSurfaceWidth, totalSurfaceHeight},
            sizeF,
            this->viewProperties.vScrollPos,
            this->viewProperties.hScrollPos
        );

        if (scrollBarRects.hScrollBar.has_value()) {
            renderTarget->FillRoundedRectangle(*scrollBarRects.hScrollBar, this->surfaceContext.scrollBarBrush);
        }
        if (scrollBarRects.vScrollBar.has_value()) {
            renderTarget->FillRoundedRectangle(*scrollBarRects.vScrollBar, this->surfaceContext.scrollBarBrush);
        }
    }
    
    if (renderTarget->EndDraw() == D2DERR_RECREATE_TARGET) {
        this->createDependentResources(size);
    }

    EndPaint(window, &ps);
}

void CDocumentView::OnSize(WPARAM, LPARAM lParam)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);
    this->resize(width, height);
}

void CDocumentView::OnSizing(WPARAM, LPARAM lParam)
{
    auto asRect = (RECT*)lParam;

    int newWidth = asRect->right - asRect->left - GetSystemMetrics(SM_CXVSCROLL);
    int newHeight = asRect->bottom - asRect->top - GetSystemMetrics(SM_CXHSCROLL);
    
    this->resize(newWidth, newHeight);
}

void CDocumentView::OnScroll(WPARAM wParam, LPARAM lParam)
{
    int mouseDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (LOWORD(wParam) == MK_CONTROL)
    {
        this->viewProperties.zoom += mouseDelta > 0 ? 0.1f : -0.1f;
        this->viewProperties.zoom = std::max(this->viewProperties.zoom, 0.1f);
    }
    else
    {
        POINT screenPoint{LOWORD(lParam), HIWORD(lParam)};
        ScreenToClient(window, &screenPoint);

        if (this->surfaceContext.renderTarget != nullptr
                && this->surfaceContext.renderTarget->GetSize().height - screenPoint.y < 10) {
            this->viewProperties.hScrollPos +=  mouseDelta > 0 ? 0.05 : -0.05;
        } else {
            this->viewProperties.vScrollPos += mouseDelta > 0 ? 0.05 : -0.05;
        }
    }
    assert(InvalidateRect(this->window, nullptr, false));
}

void CDocumentView::createDependentResources(const D2D1_SIZE_U& size)
{
    this->surfaceContext.renderTarget.Reset();
    this->surfaceContext.pageFrameBrush.Reset();
    this->surfaceContext.scrollBarBrush.Reset();

    OK(this->d2dFactory->CreateHwndRenderTarget(
                    D2D1::RenderTargetProperties(),
                    D2D1::HwndRenderTargetProperties(this->window, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
                    &this->surfaceContext.renderTarget.ptr));

    OK(this->surfaceContext.renderTarget->CreateSolidColorBrush(
                    this->viewProperties.pageFrameColor,
                    &this->surfaceContext.pageFrameBrush.ptr));

    OK(this->surfaceContext.renderTarget->CreateSolidColorBrush(
                    this->viewProperties.scrollBarColor,
                    &this->surfaceContext.scrollBarBrush.ptr));

    if (this->model != nullptr) {
        this->model->CreateObjects(this->surfaceContext.renderTarget);
    }
}

void CDocumentView::resize(int width, int height)
{
    if (this->surfaceContext.renderTarget != nullptr)
    {
        OK(this->surfaceContext.renderTarget->Resize(D2D1_SIZE_U{width, height}));
        assert(InvalidateRect(this->window, nullptr, false));
    }
}
