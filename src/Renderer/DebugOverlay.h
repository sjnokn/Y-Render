#pragma once

#include <string>

namespace YRender
{
struct DebugState
{
    int demoIndex = 0;
    bool wireframe = false;
};

std::wstring BuildDebugTitle(const DebugState& state);
} // namespace YRender
