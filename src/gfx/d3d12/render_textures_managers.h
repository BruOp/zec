#pragma once
#include "pch.h"
#include "gfx/gfx.h"

#include "core/array.h"
#include "resources.h"

namespace zec
{
    namespace dx12
    {
        class RenderTextureManager
        {
            Array<RenderTexture> render_textures;
        };
    }
}