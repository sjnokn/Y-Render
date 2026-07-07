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

private:
    bool m_down[256]{};
    bool m_pressed[256]{};
};
} // namespace YRender
