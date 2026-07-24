#include "Core/Input.h"

namespace YRender
{
void InputState::SetKey(UINT key, bool down)
{
    if (key >= kKeyCount)
    {
        return;
    }

    if (down && !m_down[key])
    {
        m_pressed[key] = true;
    }
    m_down[key] = down;
}

bool InputState::IsDown(UINT key) const
{
    return key < kKeyCount && m_down[key];
}

bool InputState::ConsumePressed(UINT key)
{
    if (key >= kKeyCount || !m_pressed[key])
    {
        return false;
    }

    m_pressed[key] = false;
    return true;
}

void InputState::Clear()
{
    for (bool& down : m_down)
    {
        down = false;
    }
    for (bool& pressed : m_pressed)
    {
        pressed = false;
    }
}
} // namespace YRender
