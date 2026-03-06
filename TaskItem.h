#pragma once
#include "pch.h"
#include "TaskItem.g.h"

namespace winrt::froggy::implementation
{
    struct TaskItem : TaskItemT<TaskItem> {

        private:
            hstring m_id{};
            hstring m_title{};
            bool    m_isCompleted{ false };
            hstring m_createdAt{};

            winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler>
                m_propertyChanged;

            void RaisePropertyChanged(hstring const& propertyName);

        public:
            TaskItem() = default;
            TaskItem(hstring const& id, hstring const& title);

            hstring Id();
            void Id(hstring const& value);

            hstring Title();
            void Title(hstring const& value);

            bool IsCompleted();
            void IsCompleted(bool value);

            hstring CreatedAt();
            void CreatedAt(hstring const& value);

            winrt::event_token PropertyChanged(
                Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
            void PropertyChanged(winrt::event_token const& token) noexcept;
    };
}

namespace winrt::froggy::factory_implementation
{
    struct TaskItem : TaskItemT<TaskItem, implementation::TaskItem> {};
}
