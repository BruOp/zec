#include "pch.h"
#include "resources.h"
#include "globals.h"
#include "descriptor_heap.h"

namespace zec
{
    namespace dx12
    {
        void destroy(Texture& texture)
        {
            queue_resource_destruction(texture.resource);
        }

        void destroy(RenderTexture& render_texture)
        {
            destroy(render_texture.texture);
            free_persistent_alloc(rtv_descriptor_heap, render_texture.rtv);
        }
    }
}
