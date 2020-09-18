#include "pch.h"
#include "input_manager.h"

namespace zec
{
    void InputManager::init(u32 width, u32 height)
    {
        internal_manager.SetDisplaySize(width, height);
    }

    void InputManager::update()
    {
        ASSERT(keyboard_id != UINT32_MAX);
        ASSERT(mouse_id != UINT32_MAX);
        internal_manager.Update();
    }

    InputMapping InputManager::create_mapping()
    {
        return InputMapping{
            internal_manager,
            keyboard_id,
            mouse_id,
        };
    }

}
