#ifndef DOCUMENT_VIEW_H
#define DOCUMENT_VIEW_H

#include <atomic>
#include <memory>

#include <windows.h>
#include <d2d1.h>
#include <d2d1_1.h>

#include "ComPtrOwner.h"
#include "IDocumentModel.h"

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

    struct CSurfaceProperties {
        CComPtrOwner<ID2D1HwndRenderTarget> renderTarget = nullptr;
        std::unique_ptr<IDocumentModel> model = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> backgroundColor = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> pageFrameColor = nullptr;
        CComPtrOwner<ID2D1SolidColorBrush> scrollColor = nullptr;
    } surfaceProps;

    struct CSurfaceState {
        double vScrollPos = 0.0;
        double hScrollPos = 0.0;
        double zoom = 1.0;
    } surfaceState;

    void createDependentResources(const D2D1_SIZE_U& size);

    void resize(int width, int height);
};


#endif
