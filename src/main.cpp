#define WINE_UNICODE_NATIVE

#include <iostream>
#include <cassert>

#include <windows.h>
#include <winuser.rh>

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include "DocumentView.h"
#include "DocumentModel.h"

static constexpr wchar_t* DocumentViewClassName = L"DIRECT2DDOCUMENTVIEW";

LRESULT WndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    assert(CoInitializeEx(NULL, COINIT_MULTITHREADED) == S_OK);

    WNDCLASSEX viewClass;
    viewClass.cbSize = sizeof(WNDCLASSEX);
    viewClass.style = CS_VREDRAW | CS_HREDRAW;
    viewClass.lpfnWndProc = WndProc;
    viewClass.cbClsExtra = 0;
    viewClass.cbWndExtra = 0;
    viewClass.hInstance = hInstance;
    viewClass.hIcon = nullptr;
    viewClass.hCursor = (HCURSOR) LoadImage(NULL, MAKEINTRESOURCE(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED);
    viewClass.hbrBackground = (HBRUSH) GetStockObject( LTGRAY_BRUSH );
    viewClass.lpszMenuName = nullptr;
    viewClass.lpszClassName = DocumentViewClassName;
    viewClass.hIconSm = nullptr;

    if (RegisterClassEx(&viewClass) == 0)
    {
        std::cerr << GetLastError() << ";RegisterClassEx\n";
        return 1;
    }

    CDocumentView view;
    HWND viewHandle = nullptr;
    if ((viewHandle = CreateWindowEx(WS_EX_ACCEPTFILES, // EX STYLES
                DocumentViewClassName, // CLASS NAME
                L"Documents viewer", // WINDOW NAME
                WS_OVERLAPPEDWINDOW, // DEF STYLES
                0, // X
                0, // Y
                600, // W
                800, // H
                NULL, // PARENT
                NULL, // MENU
                hInstance, // INSTANCE
                &view)) // ADDITIONAL PARAMS
                    == nullptr)
    {
        std::cerr << GetLastError() << ";CreateWindowEx\n";
        return 2;
    }

    ShowWindow(viewHandle, SW_NORMAL);

    MSG msg;
    while (GetMessage(&msg, viewHandle, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();

    return 0;
}