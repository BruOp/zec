#include "pch.h"
#include "input_manager.h"

namespace zec::input
{
    static InputState* g_input_state = nullptr;

    void initialize(u32 width, u32 height)
    {
        g_input_state = new InputState;
        g_input_state->internal_manager.SetDisplaySize(width, height);
        auto keyboard = g_input_state->internal_manager.CreateDevice<gainput::InputDeviceKeyboard>();
        g_input_state->keyboard_id = keyboard;
        g_input_state->mouse_id = g_input_state->internal_manager.CreateDevice<gainput::InputDeviceMouse>();
        g_input_state->is_initialized = true;
    }

    void destroy()
    {
        if (g_input_state != nullptr) {
            delete g_input_state;
            g_input_state = nullptr;
        }
    }

    void handle_msg(const MSG& msg)
    {
        if (g_input_state->is_initialized) {
            g_input_state->internal_manager.HandleMessage(msg);
        }
    }

    void update(const TimeData& time_data)
    {
        ASSERT(g_input_state->is_initialized);
        g_input_state->internal_manager.Update();
    }

    InputMapping create_mapping()
    {
        return InputMapping{
            g_input_state->internal_manager
        };
    }

    void map_bool(InputMapping& mapping, const u32 user_button, const Key key)
    {
        mapping.internal_mapping.MapBool(user_button, g_input_state->keyboard_id, gainput::UserButtonId(key));
    }

    void map_bool(InputMapping& mapping, const u32 user_button, const MouseInput key)
    {
        mapping.internal_mapping.MapBool(user_button, g_input_state->mouse_id, gainput::UserButtonId(key));
    }

    void map_float(InputMapping& mapping, const u32 user_button, const MouseInput axis)
    {
        mapping.internal_mapping.MapFloat(user_button, g_input_state->mouse_id, gainput::UserButtonId(axis));
    }

    bool button_is_down(InputMapping& mapping, const u32 user_button)
    {
        return mapping.internal_mapping.GetBool(user_button);
    }

    bool button_was_down(InputMapping& mapping, const u32 user_button)
    {
        return mapping.internal_mapping.GetBoolWasDown(user_button);
    }

    float get_axis_delta(InputMapping& mapping, const u32 user_axis)
    {
        return mapping.internal_mapping.GetFloatDelta(user_axis);
    }

}
