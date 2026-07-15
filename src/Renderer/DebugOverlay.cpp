#include "Renderer/DebugOverlay.h"

#include <sstream>

namespace YRender
{
std::wstring BuildDebugTitle(const DebugState& state)
{
    const wchar_t* preset = state.demoIndex == 0 ? L"基础光照" : (state.demoIndex == 1 ? L"卡通分层 + 边缘光" : (state.demoIndex == 2 ? L"溶解 + 噪声" : L"深度雾 + 深度描边"));
    std::wstringstream title;
    title << L"Y-Render | " << preset
          << L" | " << (state.wireframe ? L"线框" : L"实体");
    return title.str();
}
} // namespace YRender
