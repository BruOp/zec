#pragma once
#include "pch.h"
#include "utils/assert.h"
#include "gainput/gainput.h"

namespace zec
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
    class InputMapping;

    class InputManager
    {
    public:
        InputManager() :
            internal_manager{},
            keyboard_id{ internal_manager.CreateDevice<gainput::InputDeviceKeyboard>() },
            mouse_id{ internal_manager.CreateDevice<gainput::InputDeviceMouse>() }
        {};

        void init(u32 width, u32 height);

        void update();
        inline void handle_window_message(const MSG& msg)
        {
            internal_manager.HandleMessage(msg);
        }

        InputMapping create_mapping();

        gainput::InputManager internal_manager;
    private:
        gainput::DeviceId keyboard_id = UINT32_MAX;
        gainput::DeviceId mouse_id = UINT32_MAX;

    };

    class InputMapping
    {
    public:
        friend class InputManager;

        inline void map_bool(const u32 user_button, const Key key)
        {
            ASSERT(keyboard_id != UINT32_MAX);
            internal_mapping.MapBool(user_button, keyboard_id, gainput::UserButtonId(key));
        };
        inline void map_bool(const u32 user_button, const MouseInput key)
        {
            ASSERT(mouse_id != UINT32_MAX);
            internal_mapping.MapBool(user_button, mouse_id, gainput::UserButtonId(key));
        };

        inline void map_float(const u32 user_button, const MouseInput axis)
        {
            ASSERT(keyboard_id != UINT32_MAX);
            internal_mapping.MapFloat(user_button, mouse_id, gainput::UserButtonId(axis));
        };

        inline bool is_down(const u32 user_button)
        {
            return internal_mapping.GetBool(user_button);
        }

        inline bool was_down(const u32 user_button)
        {
            return internal_mapping.GetBoolWasDown(user_button);
        }

        inline float get_delta(const u32 user_axis)
        {
            return internal_mapping.GetFloatDelta(user_axis);
        }

    private:

        InputMapping(gainput::InputManager& manager, const gainput::DeviceId keyboard_id, gainput::DeviceId mouse_id) : internal_mapping{ manager }, keyboard_id{ keyboard_id }, mouse_id{ mouse_id } {};
        gainput::InputMap internal_mapping;
        gainput::DeviceId keyboard_id = UINT32_MAX;
        gainput::DeviceId mouse_id = UINT32_MAX;
    };

}