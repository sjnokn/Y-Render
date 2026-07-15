#pragma once

#include <string>
#include <vector>

namespace YRender
{
struct ProjectConfig
{
    unsigned int windowWidth = 1280;
    unsigned int windowHeight = 720;
    int defaultDemo = 0;
    bool showDebugUi = false;
    std::wstring characterModel = L"assets\\models\\character.obj";
    std::vector<std::wstring> assetRoots;
};

ProjectConfig LoadProjectConfig(const std::wstring& path);
} // namespace YRender
