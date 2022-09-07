#include <Defines.h>

#include "DocumentModel.h"

#include <DocumentView.h>

#include <iostream>
#include <cassert>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    OK(CoInitializeEx(NULL, COINIT_MULTITHREADED));

    CDocumentView view;
    view.Show();
    view.SetModel(new CDocumentModel());

    MSG msg;
    while (GetMessage(&msg, view, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();

    return 0;
}