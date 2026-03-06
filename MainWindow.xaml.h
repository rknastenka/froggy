#pragma once

#include "MainWindow.g.h"
#include "TaskItem.h"
#include "TaskDataStore.h"

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT TRAY_UID    = 1;
constexpr UINT IDM_QUIT    = 1002;
constexpr int  kWindowWidth  = 380;
constexpr int  kWindowHeight = 320;

namespace winrt::froggy::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        Windows::Foundation::Collections::IObservableVector<froggy::TaskItem> Tasks();

        // XAML event handlers
        void AddTask_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& e);

        void TaskInput_KeyDown(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void DeleteTask_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& e);

        void CheckBox_Changed(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& e);

        static double CompletedOpacity(bool isCompleted);

    private:
        Windows::Foundation::Collections::IObservableVector<froggy::TaskItem>
            m_tasks{ nullptr };
        HWND m_hwnd{ nullptr };
        bool m_isQuitting{ false };
        bool m_isLoading{ false };
        ULONGLONG m_lastDeactivateTime{ 0 };

        void AddTaskFromInput();
        winrt::fire_and_forget SaveTasks();
        Windows::Foundation::IAsyncAction LoadTasksAsync();

        void SetupBackdrop();
        void ApplyDwmDarkMode();
        void SetupAppWindow();
        void SetupTrayIcon();
        void RemoveTrayIcon();
        void PlayShowAnimation();
        void UpdateEmptyState();

        static LRESULT CALLBACK SubclassProc(
            HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
            UINT_PTR subclassId, DWORD_PTR refData);
    };
}

namespace winrt::froggy::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
