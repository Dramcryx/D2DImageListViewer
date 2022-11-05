#include <MainWindow.h>

#include <DocumentFromDisk.h>

#include <shellapi.h>
#include <winuser.rh>
#include <windowsx.h>

#include <cassert>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <functional>

namespace {

constexpr int MainWindowMenu = 2580;

const wchar_t* MainWindowClassName = L"DIRECT2DEXAMPLEMAINWINDOW";

LRESULT WINAPI MainWndowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
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
    model = new CBasicDocumentModel{};
    const wchar_t *files[] = {
        L"../bin/pic1.png",
        L"../bin/pic2.jpeg",
        L"../bin/pic3.jpg",
        L"../bin/pic4.jpg"
    };
    for (auto file: files) {
        model->AddDocument(new CDocumentFromDisk{file});
    }
    imagesView->SetModel(model);
    

    layoutGroup = CreateWindowEx(WS_EX_WINDOWEDGE,
                                 L"BUTTON",
                                 L"Layout mode:",
                                 WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                                 0, 0,
                                 300, 50,
                                 window,
                                 NULL,
                                 NULL, NULL);
    layoutLeftRadio = CreateWindowEx(WS_EX_WINDOWEDGE,
                                     L"BUTTON",
                                     L"Left",
                                     WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                                     0, 20,
                                     75, 20,
                                     this->window,
                                     NULL,
                                     NULL, NULL);
    Button_SetCheck(layoutLeftRadio, BST_CHECKED);
    layoutRightRadio = CreateWindowEx(WS_EX_WINDOWEDGE,
                                      L"BUTTON",
                                      L"Right",
                                      WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                                      75, 20,
                                      75, 20,
                                      this->window,
                                      NULL,
                                      NULL, NULL);
    layoutHCenterRadio = CreateWindowEx(WS_EX_WINDOWEDGE,
                                        L"BUTTON",
                                        L"Center",
                                        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                                        150, 20,
                                        75, 20,
                                        this->window,
                                        NULL,
                                        NULL, NULL);
    layoutFlowRadio = CreateWindowEx(WS_EX_WINDOWEDGE,
                                     L"BUTTON",
                                     L"Flow",
                                     WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                                     225, 20,
                                     75, 20,
                                     this->window,
                                     NULL,
                                     NULL, NULL);
}

void CMainWindow::Show()
{
    ShowWindow(window, SW_NORMAL);
}

bool CMainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::unordered_map<UINT, std::function<void(CMainWindow*, WPARAM, LPARAM)>> messageHandlers {
        {WM_SIZE, &CMainWindow::OnSize},
        {WM_SIZING, &CMainWindow::OnSizing},
        {WM_DROPFILES, &CMainWindow::OnDropfiles},
        {WM_COMMAND, &CMainWindow::OnCommand},
        {WM_KEYDOWN, &CMainWindow::OnKeydown}
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
    imagesView->Redraw();
}

void CMainWindow::OnSizing(WPARAM, LPARAM lParam)
{
    auto windowRect = (RECT*)lParam;
    int width = windowRect->right - windowRect->left;
    int height = windowRect->bottom - windowRect->top;
    height -= 50;
    assert(SetWindowPos(*imagesView, nullptr, 0, 50, width, height, SWP_NOZORDER));
    imagesView->Redraw();
}

void CMainWindow::OnDropfiles(WPARAM wParam, LPARAM)
{
    auto dropHandle = (HDROP)wParam;
    int filesCount = DragQueryFile(dropHandle, 0xFFFFFFFF, nullptr, 0);
    assert(filesCount != 0);

    std::vector<wchar_t> wcharsBuffer;
    wcharsBuffer.resize(4096, 0);
    for (int i = 0; i < filesCount; ++i) {
        int pathLength = DragQueryFile(dropHandle, i, nullptr, 0);
        if (pathLength > wcharsBuffer.size()) {
            wcharsBuffer.resize(pathLength + 1);
        }
        assert(DragQueryFile(dropHandle, i, wcharsBuffer.data(), wcharsBuffer.size()));
        model->AddDocument(new CDocumentFromDisk{wcharsBuffer.data()});
    }
    DragFinish(dropHandle);
    imagesView->Redraw();
}

void CMainWindow::OnCommand(WPARAM wParam, LPARAM lParam)
{
    if (HIWORD(wParam) == BN_CLICKED) {
        OnBnClicked((HWND)lParam);
    }
}

void CMainWindow::OnBnClicked(HWND button)
{
    if (button == layoutLeftRadio) {
        imagesView->SetAlignment(TImagesViewAlignment::AlignLeft);
    }
    if (button == layoutRightRadio) {
        imagesView->SetAlignment(TImagesViewAlignment::AlignRight);
    }
    if (button == layoutHCenterRadio) {
        imagesView->SetAlignment(TImagesViewAlignment::AlignHCenter);
    }
    if (button == layoutFlowRadio) {
        imagesView->SetAlignment(TImagesViewAlignment::HorizontalFlow);
    }
}

void CMainWindow::OnKeydown(WPARAM wParam, LPARAM)
{
    if (wParam == VK_DELETE) {
        auto selection = this->imagesView->GetSelectedPages();
        std::set<const IDocument*> docs;
        for (auto page : selection) {
            auto pagePtr = reinterpret_cast<IPage*>(model->GetData(page, TDocumentModelRoles::PageRole));
            docs.insert(pagePtr->GetDocument());
        }
        for (auto doc : docs) {
            model->DeleteDocument(doc);
        }
    }
}