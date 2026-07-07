#pragma once

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <stdexcept>
#include <string>

namespace YRender
{
std::string WideToUtf8(const std::wstring& text);
std::wstring Utf8ToWide(const std::string& text);
void ThrowIfFailed(HRESULT hr, const char* message);
std::wstring GetExecutableDirectory();
bool FileExists(const std::wstring& path);
std::wstring FindAsset(const std::wstring& relativePath);
} // namespace YRender
