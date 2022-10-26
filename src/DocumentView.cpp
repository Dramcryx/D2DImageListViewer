#include "DocumentView.h"

#include "DocumentViewPrivate.h"

#include <Direct2DMatrixSwitcher.h>
#include <IDocumentPage.h>

#include <d3d11_2.h>

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

std::ostream& operator<<(std::ostream& out, const D2D1_SIZE_F& size)
{
    return out << size.width << ' ' << size.height;
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
    if (CreateWindowEx(0, // EX STYLES
                DocumentViewClassName, // CLASS NAME
                L"Documents viewer", // WINDOW NAME
                WS_VISIBLE | (parent != nullptr ? WS_CHILD : 0), // DEF STYLES
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

void CDocumentView::Redraw()
{
    assert(this->window != nullptr);
    assert(InvalidateRect(this->window, NULL, false));
    assert(UpdateWindow(this->window));
}

void CDocumentView::SetModel(IDocumentModel* _model)
{
    this->model.reset(_model);
    if (this->surfaceContext.renderTarget != nullptr && this->model != nullptr) {
        this->model->CreateObjects(this->surfaceContext.renderTarget);
    }
    this->helper->SetModel(this->model.get());
}

IDocumentModel* CDocumentView::GetModel() const
{
    return this->model.get();
}

bool CDocumentView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::unordered_map<UINT, std::function<void(CDocumentView*, WPARAM, LPARAM)>> messageHandlers {
        {WM_PAINT, &CDocumentView::OnDraw},
        {WM_SIZE, &CDocumentView::OnSize},
        {WM_MOUSEWHEEL, &CDocumentView::OnScroll},
        {WM_LBUTTONUP, &CDocumentView::OnLButtonUp}
    };

    auto findRes = messageHandlers.find(msg);
    if (findRes != messageHandlers.end())
    {
        findRes->second(this, wParam, lParam);
        return true;
    }
    return false;
}

TImagesViewAlignment CDocumentView::GetAlignment() const
{
    return this->helper->GetAlignment();
}

void CDocumentView::SetAlignment(TImagesViewAlignment alignment)
{
    this->helper->SetAlignment(alignment);
    this->Redraw();
}

void CDocumentView::OnDraw(WPARAM, LPARAM)
{
    std::cout << "Redraw occured: " << GetTickCount64() << "\n";
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
        const auto& surfaceLayout = this->helper->GetLayout();
        {
            CDirect2DMatrixSwitcher switcher{
                this->surfaceContext.renderTarget,
                D2D1::Matrix3x2F::Scale(this->helper->GetZoom(), this->helper->GetZoom())
                    * D2D1::Matrix3x2F::Translation(surfaceLayout.viewportOffset.width, surfaceLayout.viewportOffset.height)};

            for (auto& pageLayout : surfaceLayout.pageRects) {
                renderTarget->DrawTextLayout(
                    {pageLayout.textRect.left, pageLayout.textRect.top},
                    pageLayout.textLayout.ptr,
                    surfaceContext.pageFrameBrush
                );
                renderTarget->DrawBitmap(
                    pageLayout.page->GetPageBitmap(),
                    pageLayout.pageRect,
                    1.f,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    nullptr
                );

                renderTarget->DrawRectangle(
                    pageLayout.pageRect,
                    this->surfaceContext.pageFrameBrush,
                    1.f / this->helper->GetZoom(),
                    nullptr
                );
            }
            if (activeIndex != -1) {
                renderTarget->DrawRectangle(
                    surfaceLayout.pageRects.at(activeIndex).pageRect,
                    surfaceContext.activePageFrameBrush,
                    1.f / this->helper->GetZoom(),
                    nullptr
                );
            }
        }

        const auto& scrollBarRects = this->helper->GetRelativeScrollBarRects();

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

    DXGI_PRESENT_PARAMETERS params{
        0, nullptr, nullptr, nullptr
    };
    OK(this->surfaceContext.swapChain->Present1(1, 0, &params));

    EndPaint(window, &ps);
}

void CDocumentView::OnSize(WPARAM, LPARAM lParam)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);
    this->resize(width, height);
}

void CDocumentView::OnScroll(WPARAM wParam, LPARAM lParam)
{
    int mouseDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (LOWORD(wParam) == MK_CONTROL)
    {
        this->helper->AddZoom(mouseDelta > 0 ? 0.1f : -0.1f);
    }
    else
    {
        POINT screenPoint{LOWORD(lParam), HIWORD(lParam)};
        ScreenToClient(window, &screenPoint);

        if (this->surfaceContext.renderTarget != nullptr
                && this->surfaceContext.renderTarget->GetSize().height - screenPoint.y < 10 )
        {
            this->helper->AddHScroll(mouseDelta > 0 ? 0.015 : -0.015);
        } else {
            this->helper->AddVScroll(mouseDelta > 0 ? 0.015 : -0.015);
        }
    }
    this->Redraw();
}


void CDocumentView::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    (void)wParam; // Ignore key for now
    int xClient = LOWORD(lParam) / this->helper->GetZoom();
    int yClient = HIWORD(lParam) / this->helper->GetZoom();

    if (model != nullptr && surfaceContext.renderTarget != nullptr) {
        auto sizeF = surfaceContext.renderTarget->GetSize();
        const auto& surfaceLayout = this->helper->GetLayout();

        xClient -= surfaceLayout.totalSurfaceSize.width * this->helper->GetHScroll();
        yClient -= surfaceLayout.totalSurfaceSize.height * this->helper->GetVScroll();

        auto isPtInRect = [xClient, yClient](const D2D1_RECT_F& rect) {
            return rect.left <= xClient && rect.right >= xClient
                    && rect.top <= yClient && rect.bottom >= yClient;
        };

        int oldIndex = std::exchange(this->activeIndex, -1);
        for (int i = 0; i < surfaceLayout.pageRects.size(); ++i) {
            if (isPtInRect(surfaceLayout.pageRects[i].pageRect)) {
                this->activeIndex = i;
                break;
            }
        }
        if (oldIndex != this->activeIndex) {
            this->Redraw();
        }
    }
}

void CDocumentView::createDependentResources(const D2D1_SIZE_U& size)
{
    this->surfaceContext.renderTarget.Reset();
    this->surfaceContext.pageFrameBrush.Reset();
    this->surfaceContext.activePageFrameBrush.Reset();
    this->surfaceContext.scrollBarBrush.Reset();
///////////////////
    // This flag is required in order to enable compatibility with Direct2D.
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_1
    };

    CComPtrOwner<ID3D11Device> d3dDevice;
    CComPtrOwner<ID3D11DeviceContext> d3dDeviceContext;
    OK(
        ::D3D11CreateDevice(
            nullptr,                    // specify nullptr to use the default adapter
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,                    // leave as nullptr if hardware is used
            creationFlags,              // optionally set debug and Direct2D compatibility flags
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,          // always set this to D3D11_SDK_VERSION
            &d3dDevice.ptr,
            nullptr,
            &d3dDeviceContext.ptr
        )
    );
    CComPtrOwner<IDXGIDevice1> dxgiDevice;
    // Obtain the underlying DXGI device of the Direct3D11 device.
    OK(d3dDevice->QueryInterface(&dxgiDevice.ptr));

    CComPtrOwner<ID2D1Device> d2dDevice;
    // Obtain the Direct2D device for 2-D rendering.
    OK(d2dFactory->CreateDevice(dxgiDevice.ptr, &d2dDevice.ptr));

    // Get Direct2D device's corresponding device context object.
    OK(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &this->surfaceContext.renderTarget.ptr));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
    swapChainDesc.Width = 0;                           // use automatic sizing
    swapChainDesc.Height = 0;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // this is the most common swapchain format
    swapChainDesc.Stereo = false; 
    swapChainDesc.SampleDesc.Count = 1;                // don't use multi-sampling
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;                     // use double buffering to enable flip
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // all apps must use this SwapEffect
    swapChainDesc.Flags = 0;

     // Identify the physical adapter (GPU or card) this device is runs on.
    CComPtrOwner<IDXGIAdapter> dxgiAdapter;
    OK(dxgiDevice->GetAdapter(&dxgiAdapter.ptr));

    // Get the factory object that created the DXGI device.
    CComPtrOwner<IDXGIFactory2> dxgiFactory;
    OK(dxgiAdapter->GetParent(IID_IDXGIFactory2, reinterpret_cast<void**>(&dxgiFactory.ptr)));

    // Get the final swap chain for this window from the DXGI factory.
    OK(dxgiFactory->CreateSwapChainForHwnd(
            d3dDevice.ptr,
            this->window,
            &swapChainDesc,
            nullptr,
            nullptr, // allow on all displays
            &this->surfaceContext.swapChain.ptr));

    // Ensure that DXGI doesn't queue more than one frame at a time.
    OK(dxgiDevice->SetMaximumFrameLatency(1));

    // Get the backbuffer for this window which is be the final 3D render target.
    CComPtrOwner<ID3D11Texture2D> backBuffer;
    OK(this->surfaceContext.swapChain->GetBuffer(0, IID_ID3D11Texture2D,reinterpret_cast<void**>(&backBuffer.ptr)));

    // Now we set up the Direct2D render target bitmap linked to the swapchain.
    // Whenever we render to this bitmap, it is directly rendered to the
    // swap chain associated with the window.
    D2D1_BITMAP_PROPERTIES1 bitmapProperties =
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
            96,
            96);

    // Direct2D needs the dxgi version of the backbuffer surface pointer.
    CComPtrOwner<IDXGISurface> dxgiBackBuffer;
    OK(this->surfaceContext.swapChain->GetBuffer(0, IID_IDXGISurface1, reinterpret_cast<void**>(&dxgiBackBuffer.ptr)));

    CComPtrOwner<ID2D1Bitmap1> targetBitmap = nullptr;
    // Get a D2D surface from the DXGI back buffer to use as the D2D render target.
    OK(this->surfaceContext.renderTarget->CreateBitmapFromDxgiSurface(
            dxgiBackBuffer.ptr,
            &bitmapProperties,
            &targetBitmap.ptr
        )
    );

    // Now we can set the Direct2D render target.
    this->surfaceContext.renderTarget->SetTarget(targetBitmap.ptr);

    ///////////////////
    OK(this->surfaceContext.renderTarget->CreateSolidColorBrush(
                    this->viewProperties.pageFrameColor,
                    &this->surfaceContext.pageFrameBrush.ptr));

    OK(this->surfaceContext.renderTarget->CreateSolidColorBrush(
                    this->viewProperties.activePageFrameColor,
                    &this->surfaceContext.activePageFrameBrush.ptr));

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
        this->surfaceContext.renderTarget->SetTarget(nullptr);
        OK(this->surfaceContext.swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
        // Now we set up the Direct2D render target bitmap linked to the swapchain.
        // Whenever we render to this bitmap, it is directly rendered to the
        // swap chain associated with the window.
        D2D1_BITMAP_PROPERTIES1 bitmapProperties =
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
                96,
                96);

        // Direct2D needs the dxgi version of the backbuffer surface pointer.
        CComPtrOwner<IDXGISurface> dxgiBackBuffer;
        OK(this->surfaceContext.swapChain->GetBuffer(0, IID_IDXGISurface1, reinterpret_cast<void**>(&dxgiBackBuffer.ptr)));

        CComPtrOwner<ID2D1Bitmap1> targetBitmap = nullptr;
        // Get a D2D surface from the DXGI back buffer to use as the D2D render target.
        OK(this->surfaceContext.renderTarget->CreateBitmapFromDxgiSurface(
                dxgiBackBuffer.ptr,
                &bitmapProperties,
                &targetBitmap.ptr
            )
        );

        // Now we can set the Direct2D render target.
        this->surfaceContext.renderTarget->SetTarget(targetBitmap.ptr);
    }
    this->helper->SetRenderTargetSize(D2D1_SIZE_F{float{width}, float{height}});
}
