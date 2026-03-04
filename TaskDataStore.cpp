#include "pch.h"
#include "TaskDataStore.h"

using namespace winrt;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::WindToDo
{
    // ---------------------------------------------------------------------------
    //  File helpers – use %LOCALAPPDATA%\WindToDo\ (works unpackaged)
    // ---------------------------------------------------------------------------

    std::filesystem::path TaskDataStore::GetDataFolder()
    {
        wchar_t* appData = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData)))
        {
            std::filesystem::path folder{ appData };
            CoTaskMemFree(appData);
            folder /= L"WindToDo";
            std::filesystem::create_directories(folder);
            return folder;
        }
        // Fallback: next to the executable
        return std::filesystem::current_path();
    }

    std::filesystem::path TaskDataStore::GetDataFilePath()
    {
        return GetDataFolder() / kFileName;
    }

    std::wstring TaskDataStore::ReadFileContents(const std::filesystem::path& path)
    {
        std::wifstream ifs(path, std::ios::in);
        if (!ifs.is_open()) return {};
        std::wstringstream wss;
        wss << ifs.rdbuf();
        return wss.str();
    }

    void TaskDataStore::WriteFileContents(const std::filesystem::path& path, const std::wstring& text)
    {
        std::wofstream ofs(path, std::ios::out | std::ios::trunc);
        if (ofs.is_open())
            ofs << text;
    }

    // ---------------------------------------------------------------------------

    JsonObject TaskDataStore::TaskToJson(WindToDo::TaskItem const& task)
    {
        JsonObject obj;
        obj.SetNamedValue(L"id",          JsonValue::CreateStringValue(task.Id()));
        obj.SetNamedValue(L"title",       JsonValue::CreateStringValue(task.Title()));
        obj.SetNamedValue(L"isCompleted", JsonValue::CreateBooleanValue(task.IsCompleted()));
        obj.SetNamedValue(L"createdAt",   JsonValue::CreateStringValue(task.CreatedAt()));
        return obj;
    }

    WindToDo::TaskItem TaskDataStore::JsonToTask(JsonObject const& obj)
    {
        auto item = winrt::make<implementation::TaskItem>(
            obj.GetNamedString(L"id", L""),
            obj.GetNamedString(L"title", L"(untitled)"));

        item.IsCompleted(obj.GetNamedBoolean(L"isCompleted", false));
        item.CreatedAt(obj.GetNamedString(L"createdAt", L""));
        return item;
    }

    IAsyncAction TaskDataStore::SaveTasksAsync(
        IObservableVector<WindToDo::TaskItem> const& tasks)
    {
        // Read existing file first to preserve other keys (e.g. windowRect)
        JsonObject root;
        auto filePath = GetDataFilePath();
        auto text = ReadFileContents(filePath);
        if (!text.empty())
            JsonObject::TryParse(hstring{ text }, root);

        JsonArray arr;
        for (auto const& t : tasks)
            arr.Append(TaskToJson(t));
        root.SetNamedValue(L"tasks", arr);

        WriteFileContents(filePath, std::wstring{ root.Stringify() });
        co_return;
    }

    IAsyncOperation<IObservableVector<WindToDo::TaskItem>>
        TaskDataStore::LoadTasksAsync()
    {
        auto tasks = winrt::single_threaded_observable_vector<WindToDo::TaskItem>();

        auto filePath = GetDataFilePath();
        auto text = ReadFileContents(filePath);
        if (!text.empty())
        {
            JsonObject root;
            if (JsonObject::TryParse(hstring{ text }, root))
            {
                auto arr = root.GetNamedArray(L"tasks", JsonArray{});
                for (uint32_t i = 0; i < arr.Size(); ++i)
                    tasks.Append(JsonToTask(arr.GetObjectAt(i)));
            }
        }

        co_return tasks;
    }

    IAsyncAction TaskDataStore::SaveWindowRectAsync(
        int32_t x, int32_t y, int32_t width, int32_t height)
    {
        JsonObject root;
        auto filePath = GetDataFilePath();
        auto text = ReadFileContents(filePath);
        if (!text.empty())
            JsonObject::TryParse(hstring{ text }, root);

        JsonObject rect;
        rect.SetNamedValue(L"x", JsonValue::CreateNumberValue(x));
        rect.SetNamedValue(L"y", JsonValue::CreateNumberValue(y));
        rect.SetNamedValue(L"w", JsonValue::CreateNumberValue(width));
        rect.SetNamedValue(L"h", JsonValue::CreateNumberValue(height));
        root.SetNamedValue(L"windowRect", rect);

        WriteFileContents(filePath, std::wstring{ root.Stringify() });
        co_return;
    }

    IAsyncOperation<JsonObject>
        TaskDataStore::LoadWindowRectAsync()
    {
        auto filePath = GetDataFilePath();
        auto text = ReadFileContents(filePath);
        if (!text.empty())
        {
            JsonObject root;
            if (JsonObject::TryParse(hstring{ text }, root) &&
                root.HasKey(L"windowRect"))
            {
                co_return root.GetNamedObject(L"windowRect");
            }
        }

        co_return nullptr;
    }
}
