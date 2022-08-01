#ifndef DOCUMENT_VIEW_H
#define DOCUMENT_VIEW_H

#include <memory>

#include <windows.h>
#include <d2d1.h>
#include <d2d1_1.h>

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

private:
    HWND window = NULL;
    ID2D1Factory* d2dFactory = nullptr;
    std::unique_ptr<CDocumentModel> model = nullptr;
};


#endif