#include "Core/Input.h"

namespace YRender
{
void InputState::SetKey(UINT key, bool down)
{
    if (key >= 256)
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
    return key < 256 && m_down[key];
}

bool InputState::ConsumePressed(UINT key)
{
    if (key >= 256 || !m_pressed[key])
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
