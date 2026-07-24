#pragma once

#include <windows.h>

namespace YRender
{
class InputState
{
public:
    void SetKey(UINT key, bool down);
    bool IsDown(UINT key) const;
    bool ConsumePressed(UINT key);
    void Clear();

private:
    // 512 covers both Win32 VK codes (0..255) and the higher GLFW "named" key
    // tokens (e.g. GLFW_KEY_ESCAPE=256, F1=290, LEFT_SHIFT=340) that survive
    // any unmapped path before GlfwKeyToVk collapses them to VK values.
    static constexpr int kKeyCount = 512;
    bool m_down[kKeyCount]{};
    bool m_pressed[kKeyCount]{};
};
} // namespace YRender
