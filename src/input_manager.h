#pragma once

#include "core/zec_types.h"
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

        class InputManager
        {
            friend class InputMapping;
        public:
            InputManager();
            InputManager(const u32 width, const u32 height);
            
            void set_dimensions(const u32 width, const u32 height);

            void update(const TimeData& time_data);

            void handle_msg(const MSG& msg);

        private:
            gainput::InputManager internal_manager = {};
            gainput::DeviceId keyboard_id = UINT32_MAX;
            gainput::DeviceId mouse_id = UINT32_MAX;
        };

        class InputMapping
        {
        public:
            InputMapping(InputManager* input_manager) :
                input_manager{ input_manager },
                internal_mapping{ input_manager->internal_manager }
            {};

            void map_bool(const u32 user_button, const Key key);
            void map_bool(const u32 user_button, const MouseInput key);

            void map_float(const u32 user_button, const MouseInput axis);

            bool button_is_down(const u32 user_button);

            bool button_was_down(const u32 user_button);

            float get_axis_delta(const u32 user_axis);
        private:
            InputManager* input_manager = nullptr;
            gainput::InputMap internal_mapping;
        };

        InputMapping create_mapping(InputManager& input_manager);
    }
}