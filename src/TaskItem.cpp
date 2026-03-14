#include "pch.h"
#include "TaskItem.h"
#include "TaskItem.g.cpp"

using namespace winrt;
using namespace Microsoft::UI::Xaml::Data;

namespace winrt::kyrios::implementation
{
    TaskItem::TaskItem(hstring const& id, hstring const& title)
        : m_id(id), m_title(title)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buf[64];
        swprintf_s(buf, L"%04d-%02d-%02dT%02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);
        m_createdAt = buf;
    }

    // ID, Title, IsCompleted, CreatedAt properties with change notification
    hstring TaskItem::Id()                    { return m_id; }
    void    TaskItem::Id(hstring const& v)
    {
        if (m_id != v) { m_id = v; RaisePropertyChanged(L"Id"); }
    }

    hstring TaskItem::Title()                 { return m_title; }
    void    TaskItem::Title(hstring const& v)
    {
        if (m_title != v) { m_title = v; RaisePropertyChanged(L"Title"); }
    }

    bool    TaskItem::IsCompleted()            { return m_isCompleted; }
    void    TaskItem::IsCompleted(bool v)
    {
        if (m_isCompleted != v) { m_isCompleted = v; RaisePropertyChanged(L"IsCompleted"); }
    }

    hstring TaskItem::CreatedAt()              { return m_createdAt; }
    void    TaskItem::CreatedAt(hstring const& v)
    {
        if (m_createdAt != v) { m_createdAt = v; RaisePropertyChanged(L"CreatedAt"); }
    }

    event_token TaskItem::PropertyChanged(PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void TaskItem::PropertyChanged(event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void TaskItem::RaisePropertyChanged(hstring const& name)
    {
        m_propertyChanged(*this, PropertyChangedEventArgs(name));
    }
}
