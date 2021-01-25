#pragma once
#include "core/zec_math.h"
#include "camera.h"
#include "scene.h"

namespace zec
{
    struct FrameConstantData
    {
        u32 brdf_LUT = UINT32_MAX;
        float time;
    };

    struct ViewConstantData
    {
        mat4 VP;
        mat4 invVP;
        vec3 camera_position;
        float padding;
    };

    RESOURCE_HANDLE(SceneViewHandle);

    class SceneViewManager;

    class FreeList
    {
    public:
        inline bool has_free_idx()
        {
            return free_list.size > 0;
        }

        u32 pop()
        {
            if (free_list.size == 0) {
                throw std::runtime_error("Cannot pop");
            }
            return free_list.pop_back();
        }

        void mark_freed(const u32 index, const size_t current_frame_idx)
        {
            ASSERT(current_frame_idx < RENDER_LATENCY);
            pending_items[current_frame_idx].push_back(index);
        }

        void process_pending_items(const size_t current_frame_idx)
        {
            auto& pending_list = pending_items[current_frame_idx];
            if (pending_list.size == 0) {
                return;
            }

            // Check if we have enough room to add all the pending items
            if (free_list.capacity < (free_list.size + pending_list.size)) {
                free_list.reserve(free_list.size + pending_list.size);
            }
            // We can now just memcpy directly into our free list
            memory::copy(free_list.data + free_list.size, pending_list.data, pending_list.size * sizeof(u32));
            free_list.size += pending_list.size;
            pending_list.empty();
        }

    private:
        Array<u32> free_list;
        Array<u32> pending_items[RENDER_LATENCY];
    };

    class SceneViewManager
    {
    public:
        SceneViewManager() = default;
        //~SceneViewManager()
        //{
        //    //for (auto& scene_view : scene_views) {
        //    //    TODO: Add destruction of buffers
        //    //    destroy(scene_view.view_cb_handle);
        //    //}
        //}

        void set_scene(RenderScene* _scene)
        {
            scene = _scene;
        }

        SceneViewHandle create_new_view(u32 camera_idx);

        const BufferHandle get_view_constant_buffer(const SceneViewHandle view_handle);

        void update();
        void copy();

    private:
        static constexpr size_t capacity = 32;
        size_t size = 0;

        RenderScene* scene = nullptr;
        u32 camera_indices[capacity] = {};
        Array<u32> entity_visibility_lists[capacity] = {};
        Array<u32> light_visibility_lists[capacity] = {};
        ViewConstantData view_constant_data[capacity] = {};
        BufferHandle view_cb_handles[capacity] = {};
        FixedArray<u32, capacity> free_indices = {};
    };
}
