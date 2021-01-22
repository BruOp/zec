#include "views.h"

namespace zec
{
    SceneViewHandle SceneViewManager::create_new_view(u32 camera_idx)
    {
        ASSERT(size < capacity);

        size_t idx = size;
        if (free_indices.size != 0) {
            idx = free_indices.pop_back();
        }
        else {
            constexpr BufferDesc buffer_desc = {
                .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
                .type = BufferType::DEFAULT,
                .byte_size = sizeof(ViewConstantData),
                .stride = 0,
                .data = nullptr,
            };
            BufferHandle buffer_handle = gfx::buffers::create(buffer_desc);

            view_cb_handles[idx] = buffer_handle;
        }

        camera_indices[idx] = camera_idx;
        return { idx };
    };

    void SceneViewManager::update()
    {
        for (size_t i = 0; i < capacity; i++) {

            // Basically each scene view will need to calculate it's visibility lists
            // Also update the ViewConstantData array
        }
    }

    void SceneViewManager::copy()
    {
        for (size_t i = 0; i < capacity; i++) {
            const auto& view_data = view_constant_data[i];
            auto& view_cb_handle = view_cb_handles[i];
            gfx::buffers::update(view_cb_handle, &view_data, sizeof(view_data));
        }
    }

}
