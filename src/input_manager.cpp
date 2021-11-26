#include "input_manager.h"

#include <windows.h>
#include <windowsx.h>

// Taken from DirectXTK Keyboard.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
enum VKKeys : unsigned char
{
    NONE = 0,

    BACK = 0X8,
    TAB = 0X9,

    ENTER = 0XD,

    PAUSE = 0X13,
    CAPSLOCK = 0X14,
    KANA = 0X15,

    KANJI = 0X19,

    ESCAPE = 0X1B,
    IMECONVERT = 0X1C,
    IMENOCONVERT = 0X1D,

    SPACE = 0X20,
    PAGEUP = 0X21,
    PAGEDOWN = 0X22,
    END = 0X23,
    HOME = 0X24,
    LEFT = 0X25,
    UP = 0X26,
    RIGHT = 0X27,
    DOWN = 0X28,
    SELECT = 0X29,
    PRINT = 0X2A,
    EXECUTE = 0X2B,
    PRINTSCREEN = 0X2C,
    INSERT = 0X2D,
    DELETEKEY = 0X2E,
    HELP = 0X2F,
    D0 = 0X30,
    D1 = 0X31,
    D2 = 0X32,
    D3 = 0X33,
    D4 = 0X34,
    D5 = 0X35,
    D6 = 0X36,
    D7 = 0X37,
    D8 = 0X38,
    D9 = 0X39,

    A = 0X41,
    B = 0X42,
    C = 0X43,
    D = 0X44,
    E = 0X45,
    F = 0X46,
    G = 0X47,
    H = 0X48,
    I = 0X49,
    J = 0X4A,
    K = 0X4B,
    L = 0X4C,
    M = 0X4D,
    N = 0X4E,
    O = 0X4F,
    P = 0X50,
    Q = 0X51,
    R = 0X52,
    S = 0X53,
    T = 0X54,
    U = 0X55,
    V = 0X56,
    W = 0X57,
    X = 0X58,
    Y = 0X59,
    Z = 0X5A,
    LEFTWINDOWS = 0X5B,
    RIGHTWINDOWS = 0X5C,
    APPS = 0X5D,

    SLEEP = 0X5F,
    NUMPAD0 = 0X60,
    NUMPAD1 = 0X61,
    NUMPAD2 = 0X62,
    NUMPAD3 = 0X63,
    NUMPAD4 = 0X64,
    NUMPAD5 = 0X65,
    NUMPAD6 = 0X66,
    NUMPAD7 = 0X67,
    NUMPAD8 = 0X68,
    NUMPAD9 = 0X69,
    MULTIPLY = 0X6A,
    ADD = 0X6B,
    SEPARATOR = 0X6C,
    SUBTRACT = 0X6D,

    DECIMAL = 0X6E,
    DIVIDE = 0X6F,
    F1 = 0X70,
    F2 = 0X71,
    F3 = 0X72,
    F4 = 0X73,
    F5 = 0X74,
    F6 = 0X75,
    F7 = 0X76,
    F8 = 0X77,
    F9 = 0X78,
    F10 = 0X79,
    F11 = 0X7A,
    F12 = 0X7B,
    F13 = 0X7C,
    F14 = 0X7D,
    F15 = 0X7E,
    F16 = 0X7F,
    F17 = 0X80,
    F18 = 0X81,
    F19 = 0X82,
    F20 = 0X83,
    F21 = 0X84,
    F22 = 0X85,
    F23 = 0X86,
    F24 = 0X87,

    NUMLOCK = 0X90,
    SCROLL = 0X91,

    LEFTSHIFT = 0XA0,
    RIGHTSHIFT = 0XA1,
    LEFTCONTROL = 0XA2,
    RIGHTCONTROL = 0XA3,
    LEFTALT = 0XA4,
    RIGHTALT = 0XA5,
    BROWSERBACK = 0XA6,
    BROWSERFORWARD = 0XA7,
    BROWSERREFRESH = 0XA8,
    BROWSERSTOP = 0XA9,
    BROWSERSEARCH = 0XAA,
    BROWSERFAVORITES = 0XAB,
    BROWSERHOME = 0XAC,
    VOLUMEMUTE = 0XAD,
    VOLUMEDOWN = 0XAE,
    VOLUMEUP = 0XAF,
    MEDIANEXTTRACK = 0XB0,
    MEDIAPREVIOUSTRACK = 0XB1,
    MEDIASTOP = 0XB2,
    MEDIAPLAYPAUSE = 0XB3,
    LAUNCHMAIL = 0XB4,
    SELECTMEDIA = 0XB5,
    LAUNCHAPPLICATION1 = 0XB6,
    LAUNCHAPPLICATION2 = 0XB7,

    OEMSEMICOLON = 0XBA,
    OEMPLUS = 0XBB,
    OEMCOMMA = 0XBC,
    OEMMINUS = 0XBD,
    OEMPERIOD = 0XBE,
    OEMQUESTION = 0XBF,
    OEMTILDE = 0XC0,

    OEMOPENBRACKETS = 0XDB,
    OEMPIPE = 0XDC,
    OEMCLOSEBRACKETS = 0XDD,
    OEMQUOTES = 0XDE,
    OEM8 = 0XDF,

    OEMBACKSLASH = 0XE2,

    PROCESSKEY = 0XE5,

    OEMCOPY = 0XF2,
    OEMAUTO = 0XF3,
    OEMENLW = 0XF4,

    ATTN = 0XF6,
    CRSEL = 0XF7,
    EXSEL = 0XF8,
    ERASEEOF = 0XF9,
    PLAY = 0XFA,
    ZOOM = 0XFB,

    PA1 = 0XFD,
    OEMCLEAR = 0XFE,
};


namespace zec::input
{
    InputKey to_zec_key(UINT msg, WPARAM wParam)
    {
        switch (msg)
        {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return InputKey::MOUSE_LEFT;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return InputKey::MOUSE_RIGHT;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return InputKey::MOUSE_MIDDLE;
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
            case WM_SYSKEYDOWN:
            {
                switch (wParam)
                {
                case VKKeys::ESCAPE:
                    return InputKey::ESCAPE;
                case VKKeys::LEFTSHIFT:
                    return InputKey::LEFT_SHIFT;
                case VKKeys::UP:
                    return InputKey::UP;
                case VKKeys::DOWN:
                    return InputKey::DOWN;
                case VKKeys::LEFT:
                    return InputKey::LEFT;
                case VKKeys::RIGHT:
                    return InputKey::RIGHT;
                case VKKeys::W:
                    return InputKey::W;
                case VKKeys::S:
                    return InputKey::S;
                case VKKeys::A:
                    return InputKey::A;
                case VKKeys::D:
                    return InputKey::D;
                case VKKeys::E:
                    return InputKey::E;
                case VKKeys::Q:
                    return InputKey::Q;
                }
            }
        }
        return InputKey::INVALID;
    }

    void set_key_bit(InputState& input_state, const InputKey key, const bool state)
    {
        if (key != InputKey::INVALID)
        {
            input_state.data.set(static_cast<size_t>(key), state);
        }
    }

    InputManager::InputManager() :
        InputManager(0, 0)
    { }

    InputManager::InputManager(const u32 width, const u32 height)
        : width{ width }
        , height{ height }
        , previous_state{ }
        , pending_state{  }
    {
    }

    void InputManager::set_dimensions(const u32 width_, const u32 height_)
    {
        width = width_;
        height = height_;
    }

    void InputManager::update(const TimeData& /*time_data*/)
    {
        previous_state = pending_state;
    }

    InputState InputManager::get_state() const
    {
        // We need to transform our pending state before we can use it in other classes
        InputState out_state = pending_state;

        const auto mouse_axis_x = static_cast<size_t>(InputAxis::MOUSE_X);
        const float prevX = previous_state.axis_values[mouse_axis_x];
        const float currX = pending_state.axis_values[mouse_axis_x];
        if (out_state.axis_modes[mouse_axis_x] == AxisMode::RELATIVE)
        {
            out_state.axis_values[mouse_axis_x] = (currX - prevX) / float(width);
        }
        else
        {
            out_state.axis_values[mouse_axis_x] = (currX - prevX);
        }

        const auto mouse_axis_y = static_cast<size_t>(InputAxis::MOUSE_Y);
        const float prevY = previous_state.axis_values[mouse_axis_y];
        const float currY = pending_state.axis_values[mouse_axis_y];
        if (out_state.axis_modes[mouse_axis_y] == AxisMode::RELATIVE)
        {
            out_state.axis_values[mouse_axis_y] = (currY - prevY) / float(height);
        }
        else
        {
            out_state.axis_values[mouse_axis_y] = (currY - prevY);
        }

        const auto mouse_wheel_axis = static_cast<size_t>(InputAxis::MOUSE_SCROLL);
        const float prevZ = previous_state.axis_values[mouse_wheel_axis];
        const float currZ = pending_state.axis_values[mouse_wheel_axis];
        out_state.axis_values[mouse_wheel_axis] = currZ - prevZ;

        return out_state;
    }

    void InputManager::handle_msg(const MSG& msg)
    {
        switch (msg.message)
        {
            case WM_ACTIVATEAPP:
                reset();
                break;

            case WM_MOUSEWHEEL:
                pending_state.axis_values[static_cast<size_t>(InputAxis::MOUSE_SCROLL)] += GET_WHEEL_DELTA_WPARAM(msg.wParam);
                break;

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_KEYUP:
            {
                // Set key state to 0
                const auto key = to_zec_key(msg.message, msg.wParam);
                set_key_bit(pending_state, key, false);
                break;
            }
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_KEYDOWN:
            {
                // Set key state to 1
                const auto key = to_zec_key(msg.message, msg.wParam);
                set_key_bit(pending_state, key, true);
                break;
            }
        default:
            break;
        }

        // Handle mouse move events
        {
            switch (msg.message)
            {
            case WM_MOUSEHOVER:
            case WM_MOUSEMOVE:
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                // All mouse messages provide a new pointer position
                pending_state.axis_values[static_cast<size_t>(InputAxis::MOUSE_X)] = static_cast<float>(GET_X_LPARAM(msg.lParam));
                pending_state.axis_values[static_cast<size_t>(InputAxis::MOUSE_Y)] = static_cast<float>(GET_Y_LPARAM(msg.lParam));
                return;
            default:
                return;
            }
        }
    }

    void InputManager::reset()
    {
        previous_state = pending_state;
    }
}
