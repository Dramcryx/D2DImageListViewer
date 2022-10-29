#ifndef EXAMPLE_MAINWINDOW_H
#define EXAMPLE_MAINWINDOW_H

#include <DocumentView.h>
#include <BasicDocumentModel.h>

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
    void OnSizing(WPARAM, LPARAM);
    void OnDropfiles(WPARAM, LPARAM);
    void OnCommand(WPARAM, LPARAM);
    void OnBnClicked(HWND button);
    void OnKeydown(WPARAM, LPARAM);

private:
    HWND window = nullptr;

    HWND layoutGroup = nullptr;
    HWND layoutLeftRadio = nullptr;
    HWND layoutRightRadio = nullptr;
    HWND layoutHCenterRadio = nullptr;
    HWND layoutFlowRadio = nullptr;

    std::unique_ptr<CDocumentView> imagesView;
    CBasicDocumentModel* model = nullptr;
};

#endif
