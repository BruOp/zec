#include "input_manager.h"

namespace zec::input
{
    InputManager::InputManager() :
        internal_manager{ },
        keyboard_id{ internal_manager.CreateDevice<gainput::InputDeviceKeyboard>() },
        mouse_id{ internal_manager.CreateDevice<gainput::InputDeviceMouse>() }
    { }

    InputManager::InputManager(const u32 width, const u32 height) : InputManager()
    {
        set_dimensions(width, height);
    }

    void InputManager::set_dimensions(const u32 width, const u32 height)
    {
        internal_manager.SetDisplaySize(width, height);
    }

    void InputManager::update(const TimeData& time_data)
    {
        internal_manager.Update();
    }

    void InputManager::handle_msg(const MSG& msg)
    {
        internal_manager.HandleMessage(msg);
    }

    InputMapping create_mapping(InputManager& input_manager)
    {
        return InputMapping{ &input_manager };
    }

    void InputMapping::map_bool(const u32 user_button, const Key key)
    {
        ASSERT(input_manager != nullptr);
        internal_mapping.MapBool(user_button, input_manager->keyboard_id, gainput::UserButtonId(key));
    }

    void InputMapping::map_bool(const u32 user_button, const MouseInput key)
    {
        ASSERT(input_manager != nullptr);
        internal_mapping.MapBool(user_button, input_manager->mouse_id, gainput::UserButtonId(key));
    }

    void InputMapping::map_float(const u32 user_button, const MouseInput axis)
    {
        ASSERT(input_manager != nullptr);
        internal_mapping.MapFloat(user_button, input_manager->mouse_id, gainput::UserButtonId(axis));
    }

    bool InputMapping::button_is_down(const u32 user_button)
    {
        return internal_mapping.GetBool(user_button);
    }

    bool InputMapping::button_was_down(const u32 user_button)
    {
        return internal_mapping.GetBoolWasDown(user_button);
    }

    float InputMapping::get_axis_delta(const u32 user_axis)
    {
        return internal_mapping.GetFloatDelta(user_axis);
    }
}
