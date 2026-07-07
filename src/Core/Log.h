#pragma once

#include <string>

namespace YRender
{
enum class LogLevel
{
    Info,
    Warning,
    Error,
};

void InitializeLog(const std::wstring& path);
void Log(LogLevel level, const std::string& message);
void LogInfo(const std::string& message);
void LogWarning(const std::string& message);
void LogError(const std::string& message);
} // namespace YRender
