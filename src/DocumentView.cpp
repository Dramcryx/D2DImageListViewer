#include "DocumentView.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include <d2d1.h>

#include <winuser.rh>

namespace {

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
        ref.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
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

struct CDocumentPagesLayoutParams {
    int pageMargin = 0;
    int pagesSpacing = 0;
    
    enum TStrategy {
        AlignLeft,
        AlignRight,
        AlignHCenter
    } strategy;
};

struct CDocumentPagesLayout {
    D2D1_SIZE_F totalSurfaceSize;
    std::vector<std::pair<const IDocumentPage*, D2D1_RECT_F>> pageRects;
};

CDocumentPagesLayout createPagesLayout(const std::vector<IDocumentPage*>& pages, const CDocumentPagesLayoutParams& params)
{
    CDocumentPagesLayout retval;

    switch (params.strategy)
    {
    case CDocumentPagesLayoutParams::AlignLeft:
    {
        float topOffset = 0.0;
        float maxWidth = 0.0;
        for (const auto& page : pages) {
            auto pageSize = page->GetPageSize();
            retval.pageRects.emplace_back(page, D2D1_RECT_F{0.0, topOffset, (float)pageSize.cx, topOffset + (float)pageSize.cy});
            maxWidth = std::max(maxWidth, (float)pageSize.cx);
            topOffset += pageSize.cy;
            topOffset += params.pagesSpacing;
        }
        retval.totalSurfaceSize = {maxWidth, topOffset - params.pagesSpacing};
        break;
    }
    case CDocumentPagesLayoutParams::AlignRight:
        break;
    
    default:
        break;
    }
    return retval;
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
    this->surfaceProps.model.reset(_model);
}

CDocumentModel* CDocumentView::GetModel() const
{
    return this->surfaceProps.model.get();
}

bool CDocumentView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::unordered_map<UINT, std::function<void(CDocumentView*, WPARAM, LPARAM)>> messageHandlers{
        {WM_PAINT, &CDocumentView::OnDraw},
        //{WM_ERASEBKGND, &CDocumentView::OnDraw},
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

    auto& renderTarget = surfaceProps.renderTarget;
    if (renderTarget == nullptr) {
        createDependentResources(size);
    }

    static CComPtrOwner<ID2D1SolidColorBrush> brush = nullptr;
    if (brush.ptr == nullptr) {
        assert(renderTarget->CreateSolidColorBrush(
                    D2D1::ColorF(D2D1::ColorF::Goldenrod),
                    &brush.ptr) == S_OK);
    }

    static CComPtrOwner<ID2D1SolidColorBrush> scrollBrush = nullptr;
    if (scrollBrush.ptr == nullptr) {
        assert(renderTarget->CreateSolidColorBrush(
                    D2D1::ColorF(D2D1::ColorF::Gray),
                    &scrollBrush.ptr) == S_OK);
    }

    D2D1_RECT_F drawRect{rect.left, rect.top, rect.right, rect.bottom};

    D2D1_SIZE_F sizeF{float(size.width), float(size.height)};
    auto currentSizeF = renderTarget->GetSize();

    if (std::tie(sizeF.width, sizeF.height) != std::tie(currentSizeF.width, currentSizeF.height)) {
        resize(size.width, size.height);
    }
    
    renderTarget->BeginDraw();

    if (surfaceProps.model != nullptr)
    {
        std::vector<IDocumentPage*> pages;
        for(int i = 0; i < surfaceProps.model->GetPageCount(); ++i)
        {
            pages.push_back(reinterpret_cast<IDocumentPage*>(surfaceProps.model->GetData(i, TDocumentModelRoles::PageRole)));
        }
        auto pagesLayout = createPagesLayout(pages, CDocumentPagesLayoutParams{
            0,
            8,
            CDocumentPagesLayoutParams::AlignLeft
        });
    
        renderTarget->FillRectangle(D2D1_RECT_F{0, 0, std::max((float)size.width, pagesLayout.totalSurfaceSize.width), pagesLayout.totalSurfaceSize.height}, brush);
    
        static CComPtrOwner<ID2D1Layer> pagesLayer{nullptr};
        if (pagesLayer.ptr == nullptr) {
            assert(renderTarget->CreateLayer(&pagesLayer.ptr) == S_OK);
        }

        renderTarget->PushLayer(
            D2D1::LayerParameters(
                D2D1::InfiniteRect(),
                NULL,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                D2D1::IdentityMatrix(),
                1.0,
                NULL,
                D2D1_LAYER_OPTIONS_NONE
            ),
            pagesLayer
        );
        for (auto& pageLayout : pagesLayout.pageRects) {
            renderTarget->DrawBitmap(pageLayout.first->GetPageBitmap(),
                pageLayout.second,
                1.0,
                D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                NULL);

        }
        renderTarget->PopLayer();

        int totalSurfaceWidth = pagesLayout.totalSurfaceSize.width;
        int visibleSurfaceWidth = size.width;
        int totalSurfaceHeight = pagesLayout.totalSurfaceSize.height;
        int visibleSurfaceHeight = size.height;

        auto zoom  = D2D1::Matrix3x2F::Identity();//D2D1::Matrix3x2F::Scale(surfaceState.zoom, surfaceState.zoom);
        auto scroll = D2D1::Matrix3x2F::Translation(totalSurfaceWidth * surfaceState.hScrollPos, totalSurfaceHeight * surfaceState.vScrollPos);
        renderTarget->SetTransform(scroll * zoom);
        // scrollRect
        {
            double hVisibleToTotal = static_cast<double>(visibleSurfaceWidth) / static_cast<double>(totalSurfaceWidth);

            int hScrollBarWidth = visibleSurfaceWidth * hVisibleToTotal;

            surfaceState.hScrollPos = std::clamp(surfaceState.hScrollPos, -1.0 + hVisibleToTotal, 0.0);

            const int hScrollHeight = 5;

            int hScrollBarTopPos = -(totalSurfaceHeight * surfaceState.vScrollPos) + visibleSurfaceHeight - hScrollHeight;
            int hScrollBarLeftPos = -(totalSurfaceWidth * surfaceState.hScrollPos + visibleSurfaceWidth * surfaceState.hScrollPos);

            D2D1_RECT_F scrollRect{hScrollBarLeftPos + 2, hScrollBarTopPos - 2, hScrollBarLeftPos + hScrollBarWidth - 2, hScrollBarTopPos + hScrollHeight - 2};
            D2D1_ROUNDED_RECT scrollDrawRect{scrollRect, 4.0, 4.0};
            renderTarget->FillRoundedRectangle(scrollDrawRect, scrollBrush);
        }
        // scrollRect
        {
            double vVisibleToTotal = static_cast<double>(visibleSurfaceHeight) / static_cast<double>(totalSurfaceHeight);

            int vScrollBarHeight = visibleSurfaceHeight * vVisibleToTotal;

            surfaceState.vScrollPos = std::clamp(surfaceState.vScrollPos, -1.0 + vVisibleToTotal, 0.0);

            const int vScrollWidth = 5;

            int vScrollBarLeftPos = -(totalSurfaceWidth  * surfaceState.hScrollPos) + visibleSurfaceWidth - vScrollWidth;
            int vScrollBarTopPos = -(totalSurfaceHeight * surfaceState.vScrollPos + visibleSurfaceHeight * surfaceState.vScrollPos);

            D2D1_RECT_F scrollRect{vScrollBarLeftPos - 2, vScrollBarTopPos + 2, vScrollBarLeftPos + vScrollWidth - 2, vScrollBarTopPos + vScrollBarHeight - 2};
            D2D1_ROUNDED_RECT scrollDrawRect{scrollRect, 4.0, 4.0};
            renderTarget->FillRoundedRectangle(scrollDrawRect, scrollBrush);
        }
    }
    else
    {
        renderTarget->FillRectangle(&drawRect, brush);
    }
    
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

void CDocumentView::OnScroll(WPARAM wParam, LPARAM lParam)
{
    int mouseDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (LOWORD(wParam) == MK_CONTROL) {
        surfaceState.zoom += (double)(mouseDelta) / 100;
    } else {
        POINT screenPoint{LOWORD(lParam), HIWORD(lParam)};
        ScreenToClient(window, &screenPoint);

        if (surfaceProps.renderTarget != nullptr && surfaceProps.renderTarget->GetSize().height - screenPoint.y < 10) {
            surfaceState.hScrollPos +=  mouseDelta > 0 ? 0.05 : -0.05;
        } else {
            surfaceState.vScrollPos += mouseDelta > 0 ? 0.05 : -0.05;
        }
    }
}

void CDocumentView::createDependentResources(const D2D1_SIZE_U& size)
{
    std::cout << __PRETTY_FUNCTION__ << "\n";
    surfaceProps.renderTarget.Reset();

    assert(d2dFactory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(window, size),
                &surfaceProps.renderTarget.ptr) == S_OK);

    if (surfaceProps.model != nullptr)
    {
        surfaceProps.model->CreateObjects(surfaceProps.renderTarget);
    }
}

void CDocumentView::resize(int width, int height)
{
    if (surfaceProps.renderTarget != nullptr)
    {
        assert(surfaceProps.renderTarget->Resize(D2D1_SIZE_U{width, height}) == S_OK);
    }
}
