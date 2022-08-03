#ifndef DOCUMENT_VIEW_H
#define DOCUMENT_VIEW_H

#include <memory>

#include <windows.h>
#include <d2d1.h>
#include <d2d1_1.h>

#include "ComPtrOwner.h"
#include "DocumentModel.h"

class CDocumentView {
public:
    CDocumentView() = default;
    ~CDocumentView();

    void AttachHandle(HWND window);

    void SetModel(CDocumentModel* model);
    CDocumentModel* GetModel() const;

    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void OnDraw(WPARAM, LPARAM);
    virtual void OnSize(WPARAM, LPARAM);
    virtual void OnSizing(WPARAM, LPARAM);
    virtual void OnScroll(WPARAM, LPARAM);

private:
    HWND window = NULL;
    CComPtrOwner<ID2D1Factory> d2dFactory = nullptr;
    CComPtrOwner<ID2D1HwndRenderTarget> renderTarget = nullptr;
    std::unique_ptr<CDocumentModel> model = nullptr;

    struct CSurfaceState {
        int vScrollPos = 0;
        int hScrollPos = 0;
        double zoom = 1.0;
    } surfaceState;

    void createDependentResources(const D2D1_SIZE_U& size);

    void resize(int width, int height);
};


#endif
