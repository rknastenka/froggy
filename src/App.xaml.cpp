#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::krisp::implementation
{
    App::App()
    {
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            auto errorMessage = e.Message();
            OutputDebugStringW((L"Unhandled exception: " + std::wstring(errorMessage) + L"\n").c_str());
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
        });
#endif
    }

    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        window = make<MainWindow>();
        window.Activate();
    }
}

// ---------------------------------------------------------------------------
//  Custom entry point
//  Defined because DISABLE_XAML_GENERATED_MAIN is set in the project.
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Normal launch path — full XAML application with system tray
    try
    {
        ::Microsoft::UI::Xaml::Application::Start(
            [](auto&&)
            {
                winrt::make<winrt::krisp::implementation::App>();
            });
    }
    catch (winrt::hresult_error const& ex)
    {
        wchar_t buf[512];
        swprintf_s(buf, L"XAML startup failed: 0x%08X  %s",
                   static_cast<uint32_t>(ex.code()), ex.message().c_str());
        OutputDebugStringW(buf);
        MessageBoxW(nullptr, buf, L"krisp – startup error", MB_ICONERROR | MB_OK);
    }

    winrt::uninit_apartment();
    return 0;
}
