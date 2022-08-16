#ifndef DOCUMENT_VIEW_H
#define DOCUMENT_VIEW_H

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include <windows.h>
#include <d2d1.h>
#include <d2d1_1.h>

#include "ComPtrOwner.h"
#include "IDocumentModel.h"

class IDocumentPage;

class CDocumentView {
public:
    CDocumentView(HWND parent = nullptr);
    ~CDocumentView();

    void AttachHandle(HWND window);
    operator HWND() { return window; }
    void Show();

    void SetModel(IDocumentModel* model);
    IDocumentModel* GetModel() const;

    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void OnDraw(WPARAM, LPARAM);
    virtual void OnSize(WPARAM, LPARAM);
    virtual void OnSizing(WPARAM, LPARAM);
    virtual void OnScroll(WPARAM, LPARAM);

private:
    HWND window = NULL;
    CComPtrOwner<ID2D1Factory> d2dFactory = nullptr;

    // Direct2D objects and model
    struct CSurfaceProperties {
        CComPtrOwner<ID2D1HwndRenderTarget> renderTarget = nullptr;
        std::unique_ptr<IDocumentModel> model = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> backgroundColor = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> pageFrameColor = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> scrollColor = nullptr;
    } surfaceProps;

    // General properties
    struct CSurfaceState {
        double vScrollPos = 0.0;
        double hScrollPos = 0.0;
        float zoom = 1.0;
    } surfaceState;

    /// @brief Creates Direct2D objects
    void createDependentResources(const D2D1_SIZE_U& size);

    /// @brief Adjusts the size of the render target
    void resize(int width, int height);

    //////////////
    /// LAYOUT ///
    //////////////

    /// @brief Layout descriptor
    struct CDocumentPagesLayoutParams {
        D2D1_SIZE_F drawSurfaceSize;
        int pageMargin = 0;
        int pagesSpacing = 0;
        
        enum TStrategy {
            AlignLeft,
            AlignRight,
            AlignHCenter,
            HorizontalFlow,
            VerticalFlow
        } strategy;
    };

    /// @brief Resulting layout
    struct CDocumentPagesLayout {
        D2D1_SIZE_F totalSurfaceSize = {0.0, 0.0};
        std::vector<std::pair<const IDocumentPage*, D2D1_RECT_F>> pageRects;
    };

    std::optional<CDocumentPagesLayout> surfaceLayout;

    /// @brief Generates pages rects based on layout descriptor
    CDocumentPagesLayout createPagesLayout(const std::vector<IDocumentPage*>& pages, const CDocumentPagesLayoutParams& params);
};

#endif
