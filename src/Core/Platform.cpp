#include "Core/Platform.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <vector>

namespace YRender
{
namespace
{
std::vector<std::wstring> g_assetRoots;
}

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

void ThrowIfFailed(HRESULT hr, const char* message)
{
    if (FAILED(hr))
    {
        std::ostringstream stream;
        stream << message << " HRESULT=0x" << std::hex << static_cast<uint32_t>(hr);
        throw std::runtime_error(stream.str());
    }
}

std::wstring GetExecutableDirectory()
{
    std::array<wchar_t, MAX_PATH> path{};
    GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    std::wstring fullPath = path.data();
    const size_t slash = fullPath.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : fullPath.substr(0, slash);
}

bool FileExists(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void AddAssetRoot(const std::wstring& root)
{
    g_assetRoots.push_back(root);
}

std::wstring FindAsset(const std::wstring& relativePath)
{
    const std::wstring exeDir = GetExecutableDirectory();
    const std::wstring roots[] = {
        L".",
        exeDir,
        exeDir + L"\\..",
        exeDir + L"\\..\\..",
    };

    for (const std::wstring& root : roots)
    {
        const std::wstring candidate = root + L"\\" + relativePath;
        if (FileExists(candidate))
        {
            return candidate;
        }
    }

    for (const std::wstring& root : g_assetRoots)
    {
        const std::wstring candidate = root + L"\\" + relativePath;
        if (FileExists(candidate))
        {
            return candidate;
        }
    }

    return relativePath;
}
} // namespace YRender
