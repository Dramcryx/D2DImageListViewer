#include <Defines.h>

#include <MainWindow.h>

#include <iostream>
#include <cassert>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    OK(CoInitializeEx(NULL, COINIT_MULTITHREADED));

    CMainWindow mainWindow;
    mainWindow.Show();

    MSG msg;
    while (GetMessage(&msg, mainWindow, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();

    return 0;
}