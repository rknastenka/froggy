#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include "resource.h"
using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Input;

namespace winrt::WindToDo::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        SetupBackdrop();
        SetupAppWindow();
        SetupTrayIcon();
        LoadTasksAsync();

        // Apply DWM dark mode to match the system theme
        ApplyDwmDarkMode();

        // Re-apply when the user switches Windows light/dark theme
        if (auto root = this->Content().try_as<Microsoft::UI::Xaml::FrameworkElement>())
        {
            root.ActualThemeChanged([this](
                Microsoft::UI::Xaml::FrameworkElement const&,
                Windows::Foundation::IInspectable const&)
            {
                ApplyDwmDarkMode();
            });
        }

        // Start hidden; the tray icon brings the window up
        if (m_hwnd)
        {
            SetWindowPos(m_hwnd, nullptr, -32000, -32000, 380, 320,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    MainWindow::~MainWindow()
    {
        RemoveTrayIcon();
    }

    IObservableVector<WindToDo::TaskItem> MainWindow::Tasks()
    {
        if (!m_tasks)
        {
            m_tasks = single_threaded_observable_vector<WindToDo::TaskItem>();
        }
        return m_tasks;
    }

    void MainWindow::SetupBackdrop()
    {
        // Reserved for future backdrop effects (e.g. Mica, Acrylic)
    }

    void MainWindow::ApplyDwmDarkMode()
    {
        if (!m_hwnd) return;
        auto root = this->Content().try_as<Microsoft::UI::Xaml::FrameworkElement>();
        if (!root) return;
        BOOL useDark = (root.ActualTheme() == Microsoft::UI::Xaml::ElementTheme::Dark) ? TRUE : FALSE;
        DwmSetWindowAttribute(m_hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/,
            &useDark, sizeof(useDark));
    }

    // Configure the window as a compact, borderless, always-on-top widget
    void MainWindow::SetupAppWindow()
    {
        auto windowNative = this->try_as<::IWindowNative>();
        if (!windowNative) return;

        windowNative->get_WindowHandle(&m_hwnd);
        if (!m_hwnd) return;

        // Hide from taskbar
        LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
        exStyle |= WS_EX_TOOLWINDOW;
        exStyle &= ~WS_EX_APPWINDOW;
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle);

        auto windowId = Microsoft::UI::GetWindowIdFromWindow(m_hwnd);
        auto appWindow = Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);

        auto presenter = appWindow.Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();
        presenter.IsAlwaysOnTop(true);
        presenter.SetBorderAndTitleBar(false, false);
        presenter.IsResizable(false);
        presenter.IsMinimizable(false);
        presenter.IsMaximizable(false);

        appWindow.Title(L"Froggy");

        // Load the icon from the embedded Win32 resource (works unpackaged
        // without needing a loose .ico file next to the .exe).
        HICON hIcon = LoadIconW(GetModuleHandleW(nullptr),
                                MAKEINTRESOURCEW(IDI_TRAYICON));
        if (hIcon)
        {
            auto iconId = Microsoft::UI::GetIconIdFromIcon(hIcon);
            appWindow.SetIcon(iconId);
        }

        appWindow.Resize({ 380, 320 });

        // Strip window chrome for a clean popup look
        {
            LONG_PTR style = GetWindowLongPtrW(m_hwnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER |
                        WS_DLGFRAME | WS_SYSMENU | WS_MINIMIZEBOX |
                        WS_MAXIMIZEBOX);
            style |= WS_POPUP | WS_CLIPCHILDREN;
            SetWindowLongPtrW(m_hwnd, GWL_STYLE, style);
        }

        // Remove class drop shadow
        {
            ULONG_PTR classStyle = GetClassLongPtrW(m_hwnd, GCL_STYLE);
            classStyle &= ~CS_DROPSHADOW;
            SetClassLongPtrW(m_hwnd, GCL_STYLE, classStyle);
        }

        // DWM: rounded corners, no border colour, no shadow
        {
            DWORD cornerPref = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(m_hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/,
                &cornerPref, sizeof(cornerPref));

            COLORREF none = DWMWA_COLOR_NONE;
            DwmSetWindowAttribute(m_hwnd, 34 /*DWMWA_BORDER_COLOR*/,
                &none, sizeof(none));

            MARGINS margins = { 0, 0, 0, 0 };
            DwmExtendFrameIntoClientArea(m_hwnd, &margins);
        }

        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED |
            SWP_NOACTIVATE);

        // Intercept close to hide instead of quit
        appWindow.Closing(
            [this](Microsoft::UI::Windowing::AppWindow const&,
                   Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
            {
                if (!m_isQuitting)
                {
                    args.Cancel(true);
                    ShowWindow(m_hwnd, SW_HIDE);
                }
            });
    }

    // Register the system-tray (notification area) icon
    void MainWindow::SetupTrayIcon()
    {
        if (!m_hwnd) return;

        SetWindowSubclass(m_hwnd, SubclassProc, 0,
            reinterpret_cast<DWORD_PTR>(this));

        HICON hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_TRAYICON));

        NOTIFYICONDATAW nid{};
        nid.cbSize   = sizeof(nid);
        nid.hWnd     = m_hwnd;
        nid.uID      = TRAY_UID;
        nid.uFlags   = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon    = hIcon;
        wcscpy_s(nid.szTip, L"Froggy");
        Shell_NotifyIconW(NIM_ADD, &nid);
    }

    void MainWindow::RemoveTrayIcon()
    {
        if (!m_hwnd) return;

        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd   = m_hwnd;
        nid.uID    = TRAY_UID;
        Shell_NotifyIconW(NIM_DELETE, &nid);

        RemoveWindowSubclass(m_hwnd, SubclassProc, 0);
    }

    // Win32 subclass — handles tray clicks & window deactivation
    LRESULT CALLBACK MainWindow::SubclassProc(
        HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR /*subclassId*/, DWORD_PTR refData)
    {
        auto* self = reinterpret_cast<MainWindow*>(refData);

        if (msg == WM_ACTIVATE)
        {
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                self->m_lastDeactivateTime = GetTickCount64();
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        if (msg == WM_TRAYICON)
        {
            if (LOWORD(lParam) == WM_LBUTTONUP)
            {
                // Toggle: if just hidden by deactivation, treat as dismiss
                if (IsWindowVisible(hwnd) ||
                    (GetTickCount64() - self->m_lastDeactivateTime < 500))
                {
                    ShowWindow(hwnd, SW_HIDE);
                }
                else
                {
                    // Position the window above the tray icon
                    NOTIFYICONIDENTIFIER nii = { sizeof(nii) };
                    nii.hWnd = hwnd;
                    nii.uID = TRAY_UID;
                    RECT iconRect = { 0 };
                    Shell_NotifyIconGetRect(&nii, &iconRect);

                    int H = 320, W = 380;
                    int x = iconRect.left + (iconRect.right - iconRect.left) / 2 - W / 2;
                    int y = iconRect.top - H - 10;

                    // Clamp to work area
                    RECT workArea{};
                    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
                    if (x < workArea.left) x = workArea.left;
                    if (x + W > workArea.right) x = workArea.right - W;
                    if (y < workArea.top)  y = workArea.top;
                    if (y + H > workArea.bottom) y = workArea.bottom - H;

                    SetWindowPos(hwnd, HWND_TOPMOST, x, y, W, H, SWP_SHOWWINDOW);
                    SetForegroundWindow(hwnd);
                    self->PlayShowAnimation();
                }
                return 0;
            }

            if (LOWORD(lParam) == WM_RBUTTONUP)
            {
                POINT pt;
                GetCursorPos(&pt);

                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, IDM_QUIT, L"Quit Froggy");

                SetForegroundWindow(hwnd);
                UINT cmd = TrackPopupMenu(menu,
                    TPM_RETURNCMD | TPM_NONOTIFY,
                    pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);

                if (cmd == IDM_QUIT)
                {
                    self->m_isQuitting = true;
                    self->SaveTasks();
                    self->RemoveTrayIcon();
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                }
                return 0;
            }
        }

        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    IAsyncAction MainWindow::LoadTasksAsync()
    {
        auto strong = get_strong();
        winrt::apartment_context ui_thread;

        auto loaded = co_await TaskDataStore::LoadTasksAsync();

        co_await ui_thread;

        m_isLoading = true;
        for (auto const& t : loaded)
        {
            Tasks().Append(t);
        }
        m_isLoading = false;
    }

    void MainWindow::SaveTasks()
    {
        if (m_isLoading || !m_tasks) return;
        TaskDataStore::SaveTasksAsync(m_tasks);
    }

    void MainWindow::AddTaskFromInput()
    {
        auto text = TaskInputBox().Text();
        if (text.empty()) return;

        GUID guid;
        CoCreateGuid(&guid);
        wchar_t guidStr[40];
        StringFromGUID2(guid, guidStr, ARRAYSIZE(guidStr));
        auto task = make<WindToDo::implementation::TaskItem>(
            hstring(guidStr), hstring(text));

        Tasks().Append(task);
        TaskInputBox().Text(L"");
        SaveTasks();
    }

    void MainWindow::AddTask_Click(
        [[maybe_unused]] IInspectable const& sender,
        [[maybe_unused]] RoutedEventArgs const& e)
    {
        AddTaskFromInput();
    }

    void MainWindow::TaskInput_KeyDown(
        [[maybe_unused]] IInspectable const& sender,
        KeyRoutedEventArgs const& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Enter)
        {
            AddTaskFromInput();
            e.Handled(true);
        }
    }

    void MainWindow::DeleteTask_Click(
        IInspectable const& sender,
        [[maybe_unused]] RoutedEventArgs const& e)
    {
        auto button = sender.as<Microsoft::UI::Xaml::Controls::Button>();
        auto item = button.DataContext().as<WindToDo::TaskItem>();

        uint32_t index = 0;
        if (Tasks().IndexOf(item, index))
        {
            Tasks().RemoveAt(index);
            SaveTasks();
        }
    }

    void MainWindow::CheckBox_Changed(
        [[maybe_unused]] IInspectable const& sender,
        [[maybe_unused]] RoutedEventArgs const& e)
    {
        SaveTasks();
    }

    // Slide-up + fade-in when the popup appears
    void MainWindow::PlayShowAnimation()
    {
        auto rootElement = this->Content().as<Microsoft::UI::Xaml::UIElement>();
        auto visual = Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(rootElement);
        auto compositor = visual.Compositor();

        auto offsetAnim = compositor.CreateVector3KeyFrameAnimation();
        offsetAnim.InsertKeyFrame(0.0f, { 0.0f, 20.0f, 0.0f });
        offsetAnim.InsertKeyFrame(1.0f, { 0.0f, 0.0f, 0.0f });
        offsetAnim.Duration(std::chrono::milliseconds(250));

        auto opacityAnim = compositor.CreateScalarKeyFrameAnimation();
        opacityAnim.InsertKeyFrame(0.0f, 0.0f);
        opacityAnim.InsertKeyFrame(1.0f, 1.0f);
        opacityAnim.Duration(std::chrono::milliseconds(250));

        visual.StartAnimation(L"Offset", offsetAnim);
        visual.StartAnimation(L"Opacity", opacityAnim);
    }

    double MainWindow::CompletedOpacity(bool isCompleted)
    {
        return isCompleted ? 0.45 : 1.0;
    }
}
