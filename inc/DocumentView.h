#ifndef DOCUMENT_VIEW_H
#define DOCUMENT_VIEW_H

#include <Defines.h>
#include <ComPtrOwner.h>
#include <IDocumentModel.h>
#include <DocumentViewParams.h>

#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <windows.h>

#include <memory>

struct IDocumentPage;

namespace DocumentViewPrivate {
class CDocumentLayoutHelper;
}

class CDocumentView {
public:
    CDocumentView(HWND parent = nullptr);
    ~CDocumentView();

    void AttachHandle(HWND window);
    operator HWND() { return window; }
    void Show();
    void Redraw();

    void SetModel(IDocumentModel* model);
    IDocumentModel* GetModel() const;

    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    TImagesViewAlignment GetAlignment() const;
    void SetAlignment(TImagesViewAlignment alignment);

protected:
    // Windows messages
    void OnDraw(WPARAM, LPARAM);
    void OnSize(WPARAM, LPARAM);
    void OnScroll(WPARAM, LPARAM);
    void OnLButtonUp(WPARAM, LPARAM);

private:
    HWND window = NULL;

    std::unique_ptr<IDocumentModel> model = nullptr;
    int activeIndex = -1;

    CComPtrOwner<ID2D1Factory1> d2dFactory = nullptr;
    std::unique_ptr<DocumentViewPrivate::CDocumentLayoutHelper> helper;

    // Direct2D objects
    struct CSurfaceContext {
        CComPtrOwner<ID2D1DeviceContext> deviceContext = nullptr;
        CComPtrOwner<IDXGISwapChain1> swapChain = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> pageFrameBrush = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> activePageFrameBrush = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> scrollBarBrush = nullptr;
    } surfaceContext;

    // General properties
    struct CViewProperties {
        D2D_COLOR_F bkColor{D2D1::ColorF{D2D1::ColorF::WhiteSmoke, 1.0f}};
        D2D_COLOR_F pageFrameColor{D2D1::ColorF{D2D1::ColorF::Black, 1.0f}};
        D2D_COLOR_F activePageFrameColor{D2D1::ColorF{D2D1::ColorF::Blue, 1.0f}};
        D2D_COLOR_F scrollBarColor{D2D1::ColorF{D2D1::ColorF::Black, .5f}};
    } viewProperties;

    /// @brief Creates Direct2D objects
    void createDependentResources();
    void createSwapChainBitmap();

    /// @brief Adjusts the size of the render target
    void resize(int width, int height);
};

#endif
