#pragma once

#include <string>

namespace YRender
{
struct DebugState
{
    float fps = 0.0f;
    int demoIndex = 0;
    int postMode = 0;
    bool wireframe = false;
};

const wchar_t* PostModeName(int mode);
std::wstring BuildDebugTitle(const DebugState& state);
} // namespace YRender
