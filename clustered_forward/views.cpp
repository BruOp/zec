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
            };
            BufferHandle buffer_handle = gfx::buffers::create(buffer_desc);

            view_cb_handles[idx] = buffer_handle;
        }
        camera_indices[idx] = camera_idx;
        return { u32(idx) };
    }

    const BufferHandle SceneViewManager::get_view_constant_buffer(const SceneViewHandle view_handle)
    {
        return view_cb_handles[view_handle.idx];
    }

    void SceneViewManager::update()
    {
        for (size_t i = 0; i < size; i++) {

            // Basically each scene view will need to calculate it's visibility lists
        }
    }

    void SceneViewManager::copy()
    {
        for (size_t i = 0; i < size; i++) {
            const auto& camera = scene->perspective_cameras[camera_indices[i]];
            auto& view_constants = view_constant_data[i];
            view_constants.VP = camera.projection * camera.view;
            view_constants.invVP = invert(camera.projection * mat4(to_mat3(camera.view), {}));
            view_constants.camera_position = camera.position;

            auto view_cb_handle = view_cb_handles[i];
            gfx::buffers::update(view_cb_handle, &view_constants, sizeof(view_constants));
        }
    }

}
