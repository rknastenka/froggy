#include "pch.h"
#include "TaskDataStore.h"

using namespace winrt;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::froggy
{
    std::mutex TaskDataStore::s_fileMutex;

    // ---------------------------------------------------------------------------
    //  File helpers – use %LOCALAPPDATA%\froggy\ (works unpackaged)
    // ---------------------------------------------------------------------------

    std::filesystem::path TaskDataStore::GetDataFolder()
    {
        wchar_t* appData = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData)))
        {
            std::filesystem::path folder{ appData };
            CoTaskMemFree(appData);
            folder /= L"froggy";
            std::filesystem::create_directories(folder);
            return folder;
        }
        throw std::runtime_error("GetDataFolder: failed to resolve %LOCALAPPDATA%");
    }

    std::filesystem::path TaskDataStore::GetDataFilePath()
    {
        return GetDataFolder() / kFileName;
    }

    std::wstring TaskDataStore::ReadFileContents(const std::filesystem::path& path)
    {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs.is_open()) return {};
        std::string utf8((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
        if (utf8.empty()) return {};
        int wLen = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        if (wLen <= 0) return {};
        std::wstring result(wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), wLen);
        return result;
    }

    void TaskDataStore::WriteFileContents(const std::filesystem::path& path, const std::wstring& text)
    {
        // Write to a temporary file first, then atomically rename to the
        // target.  This prevents truncating existing data when the write
        // fails partway (e.g. disk full, permission denied).
        auto tmpPath = path;
        tmpPath += L".tmp";

        {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (utf8Len <= 0)
                throw std::runtime_error("WriteFileContents: encoding conversion failed");
            std::string utf8(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), utf8Len, nullptr, nullptr);

            std::ofstream ofs(tmpPath, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!ofs.is_open())
                throw std::runtime_error("WriteFileContents: could not open temp file");
            ofs.write(utf8.data(), utf8.size());
            ofs.flush();
            if (!ofs.good())
                throw std::runtime_error("WriteFileContents: write failed");
        }

        // Atomic replace — MoveFileExW with MOVEFILE_REPLACE_EXISTING
        if (!MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            // Clean up the temp file on failure
            DeleteFileW(tmpPath.c_str());
            throw std::runtime_error("WriteFileContents: rename failed");
        }
    }

    // ---------------------------------------------------------------------------

    JsonObject TaskDataStore::TaskToJson(froggy::TaskItem const& task)
    {
        JsonObject obj;
        obj.SetNamedValue(L"id",          JsonValue::CreateStringValue(task.Id()));
        obj.SetNamedValue(L"title",       JsonValue::CreateStringValue(task.Title()));
        obj.SetNamedValue(L"isCompleted", JsonValue::CreateBooleanValue(task.IsCompleted()));
        obj.SetNamedValue(L"createdAt",   JsonValue::CreateStringValue(task.CreatedAt()));
        return obj;
    }

    froggy::TaskItem TaskDataStore::JsonToTask(JsonObject const& obj)
    {
        auto id = obj.GetNamedString(L"id", L"");
        auto title = obj.GetNamedString(L"title", L"(untitled)");

        // Validate id is non-empty and title is within bounds
        if (id.empty()) {
            GUID guid;
            CoCreateGuid(&guid);
            wchar_t guidStr[40];
            StringFromGUID2(guid, guidStr, ARRAYSIZE(guidStr));
            id = guidStr;
        }
        constexpr uint32_t kMaxTitleLength = 500;
        if (title.size() > kMaxTitleLength)
            title = hstring(std::wstring_view(title).substr(0, kMaxTitleLength));

        auto item = winrt::make<implementation::TaskItem>(id, title);

        item.IsCompleted(obj.GetNamedBoolean(L"isCompleted", false));
        item.CreatedAt(obj.GetNamedString(L"createdAt", L""));
        return item;
    }

    IAsyncAction TaskDataStore::SaveTasksAsync(
        IObservableVector<froggy::TaskItem> const& tasks)
    {
        std::lock_guard lock(s_fileMutex);

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

    IAsyncOperation<IObservableVector<froggy::TaskItem>>
        TaskDataStore::LoadTasksAsync()
    {
        std::lock_guard lock(s_fileMutex);

        auto tasks = winrt::single_threaded_observable_vector<froggy::TaskItem>();

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
}
