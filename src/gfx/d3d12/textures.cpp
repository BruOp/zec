#include "pch.h"
#include "gfx/public_resources.h"
#include "globals.h"

namespace zec::gfx::textures
{
    TextureInfo get_texture_info(const TextureHandle texture_handle)
    {
        return dx12::TextureUtils::get_texture_info(dx12::g_textures, texture_handle);
    };
}