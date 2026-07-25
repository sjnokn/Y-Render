#include "Renderer/DebugOverlay.h"

#include <sstream>

namespace YRender
{
std::wstring BuildDebugTitle(const DebugState& state)
{
    const wchar_t* preset = state.demoIndex == 0 ? L"Basic Lighting" : (state.demoIndex == 1 ? L"Toon Ramp + Rim" : (state.demoIndex == 2 ? L"Dissolve + Noise" : L"Depth Fog + Outline"));
    std::wstringstream title;
    title << L"Y-Render | " << preset
          << L" | " << (state.wireframe ? L"Wireframe" : L"Solid");
    return title.str();
}
} // namespace YRender
