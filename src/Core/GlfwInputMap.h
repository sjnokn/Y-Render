#pragma once

// Maps GLFW key tokens to Windows Virtual-Key codes so that the existing
// InputState / Camera / OnKeyPressed logic (which all query Win32 VK values)
// can be reused verbatim behind a GLFW event chain.
//
// Printable tokens pass through unchanged: GLFW_KEY_A..Z (65..90) already equal
// 'A'..'Z' (== VK_A..Z), GLFW_KEY_0..9 (48..57) == VK_0..9, GLFW_KEY_SPACE (32)
// == VK_SPACE. Only the GLFW "named" keys (>= 256) need translation; without it
// they would index past the 256-entry portion of the input tables.
//
// Include order: <glad/glad.h> (Phase 3c) must precede any <GLFW/glfw3.h> /
// <windows.h> in GL-using TUs. Include this header after glad in such TUs.
// GLFW_INCLUDE_NONE is forced so glfw3.h never auto-includes <GL/gl.h>, which
// would collide with glad's `#define glXxx glad_glXxx` macros.

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace YRender
{
inline int GlfwKeyToVk(int key)
{
    switch (key)
    {
    case GLFW_KEY_ESCAPE: return 0x1B; // VK_ESCAPE
    case GLFW_KEY_ENTER: return 0x0D;  // VK_RETURN
    case GLFW_KEY_TAB: return 0x09;    // VK_TAB
    case GLFW_KEY_BACKSPACE: return 0x08; // VK_BACK
    case GLFW_KEY_INSERT: return 0x2D; // VK_INSERT
    case GLFW_KEY_DELETE: return 0x2E; // VK_DELETE
    case GLFW_KEY_RIGHT: return 0x27;  // VK_RIGHT
    case GLFW_KEY_LEFT: return 0x25;   // VK_LEFT
    case GLFW_KEY_DOWN: return 0x28;   // VK_DOWN
    case GLFW_KEY_UP: return 0x26;     // VK_UP
    case GLFW_KEY_PAGE_UP: return 0x21;   // VK_PRIOR
    case GLFW_KEY_PAGE_DOWN: return 0x22; // VK_NEXT
    case GLFW_KEY_HOME: return 0x24;   // VK_HOME
    case GLFW_KEY_END: return 0x23;    // VK_END
    case GLFW_KEY_CAPS_LOCK: return 0x14;    // VK_CAPITAL
    case GLFW_KEY_SCROLL_LOCK: return 0x91;  // VK_SCROLL
    case GLFW_KEY_NUM_LOCK: return 0x90;     // VK_NUMLOCK
    case GLFW_KEY_PRINT_SCREEN: return 0x2A; // VK_SNAPSHOT
    case GLFW_KEY_PAUSE: return 0x13;        // VK_PAUSE
    case GLFW_KEY_LEFT_SHIFT: return 0x10;   // VK_SHIFT
    case GLFW_KEY_LEFT_CONTROL: return 0x11; // VK_CONTROL
    case GLFW_KEY_LEFT_ALT: return 0x12;     // VK_MENU
    case GLFW_KEY_LEFT_SUPER: return 0x5B;   // VK_LWIN
    case GLFW_KEY_RIGHT_SHIFT: return 0x10;  // VK_SHIFT
    case GLFW_KEY_RIGHT_CONTROL: return 0x11;// VK_CONTROL
    case GLFW_KEY_RIGHT_ALT: return 0x12;    // VK_MENU
    case GLFW_KEY_RIGHT_SUPER: return 0x5C;  // VK_RWIN
    case GLFW_KEY_MENU: return 0x12;         // VK_MENU
    default: break;
    }

    // GLFW_KEY_F1 (290) .. GLFW_KEY_F25 (314) -> VK_F1 (0x70) .. VK_F25 (0x88)
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
    {
        return 0x70 + (key - GLFW_KEY_F1);
    }

    // Everything else (printables + unknowns) is returned as-is.
    return key;
}
} // namespace YRender
