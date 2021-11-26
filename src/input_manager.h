#pragma once

#include <memory>
#include <bitset>

#include "core/zec_types.h"
#include "utils/assert.h"
#include "timer.h"

struct tagMSG;
typedef tagMSG MSG;

namespace DirectX
{
    class Mouse;
    class Keyboard;
}

namespace zec::input
{
    struct InputState;

    enum struct InputKey : u32
    {
        ESCAPE = 0u,
        LEFT_SHIFT,
        UP,
        DOWN,
        LEFT,
        RIGHT,
        W,
        S,
        A,
        D,
        E,
        Q,
        MOUSE_LEFT,
        MOUSE_RIGHT,
        MOUSE_MIDDLE,
        NUM_KEYS,
        INVALID,
    };

    enum struct InputAxis: u8
    {
        MOUSE_X = 0u,
        MOUSE_Y,
        MOUSE_SCROLL,
        NUM_AXES,
    };

    enum struct AxisMode : u8
    {
        RELATIVE = 0u,
        ABSOLUTE,
        NUM_AXES_MODES,
    };

    struct InputState
    {
        std::bitset<static_cast<size_t>(InputKey::NUM_KEYS)> data = {};
        float axis_values[static_cast<size_t>(InputAxis::NUM_AXES)] = {};
        AxisMode axis_modes[static_cast<size_t>(InputAxis::NUM_AXES)] = {};

        bool button_is_down(const InputKey key) const { return data[static_cast<size_t>(key)]; };
        float get_axis_value(const InputAxis axis) const { return axis_values[static_cast<size_t>(axis)]; };
        AxisMode get_axis_mode(const InputAxis axis) const { return axis_modes[static_cast<size_t>(axis)]; };
    };

    class InputManager
    {
        friend class InputMapping;
    public:
        InputManager();
        InputManager(const u32 width, const u32 height);

        void update(const TimeData& time_data);
        void handle_msg(const MSG& msg);

        void set_dimensions(const u32 width, const u32 height);
        void set_axis_mode(const InputAxis axis, const AxisMode mode);

        InputState get_state() const;
    private:
        void reset();

        u32 width = 0;
        u32 height = 0;
        InputState previous_state = {};
        InputState pending_state = {};
    };
}
