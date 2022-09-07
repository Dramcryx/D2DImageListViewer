#include <MainWindow.h>

#include <shellapi.h>
#include <winuser.rh>

#include <cassert>
#include <optional>
#include <unordered_map>
#include <functional>

namespace {

constexpr int MainWindowMenu = 2580;

const wchar_t* MainWindowClassName = L"DIRECT2DEXAMPLEMAINWINDOW";

LRESULT MainWndowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CMainWindow* mainWindow = nullptr;
    if (msg == WM_CREATE)
    {
        auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        mainWindow = reinterpret_cast<CMainWindow*>(createStruct->lpCreateParams);
        if (mainWindow == nullptr) {
            abort();
        }
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(mainWindow));
        mainWindow->AttachHandle(window);
    }
    else
    {
        mainWindow = reinterpret_cast<CMainWindow*>(GetWindowLongPtr(window, GWLP_USERDATA));
        if (mainWindow != nullptr && mainWindow->HandleMessage(msg, wParam, lParam))
        {
            return 0;
        }
    }
    
    return DefWindowProc(window, msg, wParam, lParam);
}

void RegisterMainWindowClass()
{
    static std::optional<WNDCLASSEX> MainWindow;
    if (!MainWindow.has_value())
    {
        MainWindow = WNDCLASSEX{};
        auto& ref = *MainWindow;
        ref.cbSize = sizeof(WNDCLASSEX);
        ref.style = 0;
        ref.lpfnWndProc = MainWndowProc;
        ref.cbClsExtra = 0;
        ref.cbWndExtra = 0;
        ref.hInstance = GetModuleHandle(NULL);
        ref.hIcon = nullptr;
        ref.hCursor = (HCURSOR) LoadImage(NULL, MAKEINTRESOURCE(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED);
        ref.hbrBackground = (HBRUSH) GetStockObject( LTGRAY_BRUSH );
        ref.lpszMenuName = MAKEINTRESOURCE(MainWindowMenu);
        ref.lpszClassName = MainWindowClassName;
        ref.hIconSm = nullptr;

        if (RegisterClassEx(&ref) == 0)
        {
            throw std::runtime_error("RegisterMainWindowClass; RegisterClassEx");
        }
    }
}

}

CMainWindow::CMainWindow()
{
    RegisterMainWindowClass();
    if (CreateWindowEx(WS_EX_ACCEPTFILES, // EX STYLES
                MainWindowClassName, // CLASS NAME
                L"Images viewer example", // WINDOW NAME
                WS_OVERLAPPEDWINDOW, // DEF STYLES
                0, // X
                0, // Y
                600, // W
                800, // H
                NULL, // PARENT
                NULL, // MENU
                GetModuleHandle(NULL), // INSTANCE
                this) // ADDITIONAL PARAMS
                    == nullptr)
    {
        throw std::runtime_error("CMainWindow::CMainWindow; CreateWindowEx");
    }
}

CMainWindow::~CMainWindow() = default;

void CMainWindow::AttachHandle(HWND _window)
{
    this->window = _window;

    imagesView.reset(new CDocumentView{window});
    imagesView->SetModel(model = new CDocumentModel{});
}

void CMainWindow::Show()
{
    ShowWindow(window, SW_NORMAL);
}

bool CMainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::unordered_map<UINT, std::function<void(CMainWindow*, WPARAM, LPARAM)>> messageHandlers {
        {WM_SIZE, &CMainWindow::OnSize},
        {WM_DROPFILES, &CMainWindow::OnDropfiles}
    };

    auto findRes = messageHandlers.find(msg);
    if (findRes != messageHandlers.end())
    {
        findRes->second(this, wParam, lParam);
        return true;
    }
    return false;
}

void CMainWindow::OnSize(WPARAM, LPARAM lParam)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);
    height -= 50;
    assert(SetWindowPos(*imagesView, nullptr, 0, 50, width, height, SWP_NOZORDER));
}

void CMainWindow::OnDropfiles(WPARAM wParam, LPARAM)
{
    auto dropHandle = (HDROP)wParam;
    int filesCount = DragQueryFile(dropHandle, 0xFFFFFFFF, nullptr, 0);
    assert(filesCount != 0);

    wchar_t filename[1024] = {0};
    for (int i = 0; i < filesCount; ++i) {
        assert(DragQueryFile(dropHandle, i, filename, 1023));
        model->AddImageFromFile(filename);
    }
    DragFinish(dropHandle);
    assert(InvalidateRect(*imagesView, nullptr, false));
}