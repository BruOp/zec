#include "pch.h"
#include "utils/utils.h"
#include "gfx/public_resources.h"
#include "globals.h"
#include "command_context.h"
#include <DirectXTex.h>
#include "dx_utils.h"

namespace zec::gfx::textures
{
    TextureInfo& get_texture_info(const TextureHandle texture_handle)
    {
        return dx12::TextureUtils::get_texture_info(dx12::g_textures, texture_handle);
    };

    void save_texture(const TextureHandle texture_handle, const wchar_t* file_path, const ResourceUsage current_usage)
    {
        DirectX::ScratchImage scratch{ };
        TextureInfo& texture_info = get_texture_info(texture_handle);
        ID3D12Resource* resource = dx12::get_resource(dx12::g_textures, texture_handle);
        ID3D12CommandQueue* gfx_queue = dx12::CommandContextUtils::get_pool(CommandQueueType::GRAPHICS).queue;
        DXCall(DirectX::CaptureTexture(
            gfx_queue,
            resource,
            texture_info.is_cubemap,
            scratch,
            dx12::to_d3d_resource_state(current_usage),
            dx12::to_d3d_resource_state(current_usage)
        ));

        DXCall(DirectX::SaveToDDSFile(scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(), DirectX::DDS_FLAGS_NONE, file_path));
    }
}