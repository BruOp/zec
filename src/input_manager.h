#pragma once
#include "pch.h"
#include "utils/assert.h"
#include "timer.h"
#include "gainput/gainput.h"

namespace zec
{
    namespace input
    {
        enum struct Key : u32
        {
            ESCAPE = gainput::KeyEscape,
            UP = gainput::KeyUp,
            DOWN = gainput::KeyDown,
            LEFT = gainput::KeyLeft,
            RIGHT = gainput::KeyRight,
            W = gainput::KeyW,
            S = gainput::KeyS,
            A = gainput::KeyA,
            D = gainput::KeyD,
            E = gainput::KeyE,
            Q = gainput::KeyQ,
        };

        enum struct MouseInput : u32
        {
            LEFT = gainput::MouseButtonLeft,
            RIGHT = gainput::MouseButtonRight,
            MIDDLE = gainput::MouseButtonMiddle,
            SCROLL_UP = gainput::MouseButton3,
            SCROLL_DOWN = gainput::MouseButton4,

            AXIS_X = gainput::MouseAxisX,
            AXIS_Y = gainput::MouseAxisY,
        };

        struct InputState
        {
            gainput::InputManager internal_manager = {};
            gainput::DeviceId keyboard_id = 13;
            gainput::DeviceId mouse_id = UINT32_MAX;
            bool is_initialized = false;
        };

        struct InputMapping
        {
            gainput::InputMap internal_mapping;
        };

        void initialize(u32 width, u32 height);

        void destroy();

        void handle_msg(const MSG& msg);

        void update(const TimeData& time_data);

        InputMapping create_mapping();

        void map_bool(InputMapping& mapping, const u32 user_button, const Key key);
        void map_bool(InputMapping& mapping, const u32 user_button, const MouseInput key);

        void map_float(InputMapping& mapping, const u32 user_button, const MouseInput axis);

        bool button_is_down(InputMapping& mapping, const u32 user_button);

        bool button_was_down(InputMapping& mapping, const u32 user_button);

        float get_axis_delta(InputMapping& mapping, const u32 user_axis);
    }
}