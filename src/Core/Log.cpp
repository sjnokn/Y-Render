#include "Core/Log.h"

#include "Core/Platform.h"

#include <windows.h>

#include <fstream>
#include <mutex>

namespace YRender
{
namespace
{
std::wofstream g_logFile;
std::mutex g_logMutex;

const char* LevelName(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Error:
        return "Error";
    default:
        return "Unknown";
    }
}
} // namespace

void InitializeLog(const std::wstring& path)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logFile.open(path, std::ios::out | std::ios::trunc);
}

void Log(LogLevel level, const std::string& message)
{
    const std::string line = "[" + std::string(LevelName(level)) + "] " + message + "\n";
    OutputDebugStringA(line.c_str());

    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile.is_open())
    {
        g_logFile << Utf8ToWide(line);
        g_logFile.flush();
    }
}

void LogInfo(const std::string& message)
{
    Log(LogLevel::Info, message);
}

void LogWarning(const std::string& message)
{
    Log(LogLevel::Warning, message);
}

void LogError(const std::string& message)
{
    Log(LogLevel::Error, message);
}
} // namespace YRender
