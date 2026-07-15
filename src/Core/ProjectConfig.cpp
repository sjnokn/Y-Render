#include "Core/ProjectConfig.h"

#include "Core/Platform.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace YRender
{
namespace
{
std::string Trim(std::string value)
{
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !isSpace(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
    return value;
}

bool ParseBool(const std::string& value, bool fallback)
{
    if (value == "true" || value == "1" || value == "yes")
    {
        return true;
    }
    if (value == "false" || value == "0" || value == "no")
    {
        return false;
    }
    return fallback;
}
} // namespace

ProjectConfig LoadProjectConfig(const std::wstring& path)
{
    ProjectConfig config;
    config.assetRoots.push_back(L"assets");

    std::ifstream file(path);
    if (!file)
    {
        return config;
    }

    std::string line;
    while (std::getline(file, line))
    {
        const size_t comment = line.find('#');
        if (comment != std::string::npos)
        {
            line = line.substr(0, comment);
        }

        const size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }

        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        if (key == "window_width")
        {
            config.windowWidth = static_cast<unsigned int>(std::max(1, std::stoi(value)));
        }
        else if (key == "window_height")
        {
            config.windowHeight = static_cast<unsigned int>(std::max(1, std::stoi(value)));
        }
        else if (key == "default_demo")
        {
            config.defaultDemo = std::clamp(std::stoi(value), 0, 2);
        }
        else if (key == "show_debug_ui")
        {
            config.showDebugUi = ParseBool(value, config.showDebugUi);
        }
        else if (key == "character_model")
        {
            config.characterModel = Utf8ToWide(value);
        }
        else if (key == "asset_root")
        {
            config.assetRoots.push_back(Utf8ToWide(value));
        }
    }

    return config;
}
} // namespace YRender
