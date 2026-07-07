#include "Renderer/DebugOverlay.h"

#include <algorithm>
#include <sstream>

namespace YRender
{
const wchar_t* PostModeName(int mode)
{
    static const wchar_t* names[] = {L"None", L"Grayscale", L"Invert", L"Vignette"};
    return names[std::clamp(mode, 0, 3)];
}

std::wstring BuildDebugTitle(const DebugState& state)
{
    std::wstringstream title;
    title << L"Y-Render | FPS " << static_cast<int>(state.fps)
          << L" | Demo " << state.demoIndex
          << L" | Post " << PostModeName(state.postMode)
          << L" | " << (state.wireframe ? L"Wireframe" : L"Solid");
    return title.str();
}
} // namespace YRender
