#ifndef D2DILV_DOCUMENT_VIEW_H
#define D2DILV_DOCUMENT_VIEW_H

#include <Defines.h>
#include <ComPtr.h>
#include <IDocumentModel.h>
#include <DocumentViewParams.h>

#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <windows.h>

#include <memory>

/// @brief Forward declarations
namespace DocumentViewPrivate {
class CDocumentLayoutHelper;
}

/// @brief Viewer of the document model. Subscribes to model's notifications.
class CDocumentView : private IDocumentsModelCallback {
public:
    /// @brief Constructor
    /// @param parent Parent window to attach
    CDocumentView(HWND parent = nullptr);

    /// @brief Destructor
    ~CDocumentView();

    /// @brief Called internally by window procedure to attach window handle to this object
    /// @param window Window handle from CreateWindowEx
    void AttachHandle(HWND window);

    /// @brief Operator to quickly represent this object for Win32 calls
    operator HWND() { return window; }

    /// @brief Makes the view visible
    void Show();

    /// @brief Schedule view redraw
    void Redraw();

    /// @brief Set new model. The view takes ownership of it and subscribes to its' updates.
    /// Unsubscribes from old model if exists.
    /// @param model Pointer to model
    void SetModel(IDocumentsModel* model);

    /// @brief Get current document model
    /// @return Model-owned pointer to the model
    IDocumentsModel* GetModel() const;

    /// @brief Get active index if exists
    /// @return Active index or -1 if none is active
    int GetActiveIndex() const { return activeIndex; }

    /// @brief Messgage handler called by window procedure
    /// @param msg Message ID
    /// @param wParam Word parameter
    /// @param lParam Long parameter
    /// @return True if message was handled, false otherwise
    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    /// @brief Get current pages alignment
    /// @return Pages alignment
    TImagesViewAlignment GetAlignment() const;

    /// @brief Set new pages alignment
    /// @param alignment New pages alignment
    void SetAlignment(TImagesViewAlignment alignment);

protected:
    // Windows messages
    void OnDraw(WPARAM, LPARAM);
    void OnSize(WPARAM, LPARAM);
    void OnScroll(WPARAM, LPARAM);
    void OnLButtonUp(WPARAM, LPARAM);

    void OnDocumentAdded(IDocument* doc) override;
    void OnDocumentDeleted(IDocument* doc) override;

private:
    HWND window = NULL;

    std::unique_ptr<IDocumentsModel> model = nullptr;
    int activeIndex = -1;

    CComPtr<ID2D1Factory1> d2dFactory = nullptr;
    std::unique_ptr<DocumentViewPrivate::CDocumentLayoutHelper> helper;

    // Direct2D objects
    struct CSurfaceContext {
        CComPtr<ID2D1DeviceContext> deviceContext = nullptr;
        CComPtr<IDXGISwapChain1> swapChain = nullptr;
        CComPtr<ID2D1SolidColorBrush> pageFrameBrush = nullptr;
        CComPtr<ID2D1SolidColorBrush> activePageFrameBrush = nullptr;
        CComPtr<ID2D1SolidColorBrush> scrollBarBrush = nullptr;
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
    /// @brief Creates new bitmap for swapchain
    void createSwapChainBitmap();

    /// @brief Adjusts the size of the render target
    void resize(int width, int height);
};

#endif
