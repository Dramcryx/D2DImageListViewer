#ifndef EXAMPLE_MAINWINDOW_H
#define EXAMPLE_MAINWINDOW_H

#include <DocumentView.h>
#include <DocumentModel.h>

#include <memory>

class CMainWindow {
public:
    CMainWindow();
    ~CMainWindow();

    void AttachHandle(HWND window);
    operator HWND() { return window; }
    void Show();

    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    void OnSize(WPARAM, LPARAM);
    void OnDropfiles(WPARAM, LPARAM);

private:
    HWND window = nullptr;

    std::unique_ptr<CDocumentView> imagesView;
    CDocumentModel* model = nullptr;
};

#endif
