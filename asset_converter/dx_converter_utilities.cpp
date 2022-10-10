#include "converter_utilities.h"
#include <DirectXTex.h>
#include <gfx/d3d12/dx_utils.h>
#include <gfx/d3d12/dx_helpers.h>
#include "utils/utils.h"
#include <filesystem>

namespace zec::assets
{
    using namespace zec::gfx::dx12;

    void load_texture_from_file(const char* file_path, assets::TextureDesc& out_desc, void* out_data)
    {
        std::wstring path = ansi_to_wstring(file_path);
        const std::filesystem::path filesystem_path{ file_path };

        DirectX::ScratchImage image;
        const auto& extension = filesystem_path.extension();

        if (extension.compare(L".dds") == 0 || extension.compare(L".DDS") == 0) {
            DXCall(DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
        }
        else if (extension.compare(L".hdr") == 0 || extension.compare(L".HDR") == 0) {
            DXCall(DirectX::LoadFromHDRFile(path.c_str(), nullptr, image));
        }
        else if (extension.compare(L".png") == 0 || extension.compare(L".PNG") == 0) {
            DXCall(DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image));
        }
        else {
            throw std::runtime_error("Wasn't able to load file!");
        }

        const DirectX::TexMetadata meta_data = image.GetMetadata();
        TextureDesc texture_desc{
            .width = u32(meta_data.width),
            .height = u32(meta_data.height),
            .depth = u32(meta_data.depth),
            .num_mips = u32(meta_data.mipLevels),
            .array_size = u32(meta_data.arraySize),
            .is_cubemap = u16(meta_data.IsCubemap()),
            .format = from_d3d_format(meta_data.format),
            .usage = RESOURCE_USAGE_SHADER_READABLE,
        };

        ID3D12Resource* resource = g_context.textures.resources[texture_handle];
        bool is_3d = meta_data.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;
        const D3D12_RESOURCE_DESC d3d_desc = {
            .Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(meta_data.dimension),
            .Alignment = 0,
            .Width = u32(meta_data.width),
            .Height = u32(meta_data.height),
            .DepthOrArraySize = is_3d ? u16(meta_data.depth) : u16(meta_data.arraySize),
            .MipLevels = u16(meta_data.mipLevels),
            .Format = meta_data.format,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
             },
             .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
             .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        DXGI_FORMAT d3d_format = meta_data.format;
        u32 num_subresources = meta_data.mipLevels * meta_data.arraySize;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * num_subresources);
        u32* num_rows = (u32*)_alloca(sizeof(u32) * num_subresources);
        u64* row_sizes = (u64*)_alloca(sizeof(u64) * num_subresources);

        u64 mem_size = 0;
        g_context.device->GetCopyableFootprints(
            &d3d_desc,
            0,
            u32(num_subresources),
            0,
            layouts, num_rows, row_sizes, &mem_size);

        // Create staging resources
        ID3D12Resource* staging_resource = nullptr;
        D3D12MA::Allocation* allocation = nullptr;

        // Get a GPU upload buffer
        D3D12MA::ALLOCATION_DESC upload_alloc_desc = {};
        upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(mem_size);
        DXCall(g_context.allocator->CreateResource(
            &upload_alloc_desc,
            &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &allocation,
            IID_PPV_ARGS(&staging_resource)
        ));

        void* mapped_ptr = nullptr;
        DXCall(staging_resource->Map(0, NULL, &mapped_ptr));

        for (u64 array_idx = 0; array_idx < meta_data.arraySize; ++array_idx) {

            for (u64 mip_idx = 0; mip_idx < meta_data.mipLevels; ++mip_idx) {
                const u64 subresource_idx = mip_idx + (array_idx * meta_data.mipLevels);

                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subresource_layout = layouts[subresource_idx];
                const u64 subresource_height = num_rows[subresource_idx];
                const u64 subresource_pitch = subresource_layout.Footprint.RowPitch;
                const u64 subresource_depth = subresource_layout.Footprint.Depth;
                u8* dst_subresource_mem = static_cast<u8*>(mapped_ptr) + subresource_layout.Offset;

                for (u64 z = 0; z < subresource_depth; ++z) {
                    const DirectX::Image* sub_image = image.GetImage(mip_idx, array_idx, z);
                    ASSERT(sub_image != nullptr);
                    const u8* src_subresource_mem = sub_image->pixels;

                    for (u64 y = 0; y < subresource_height; ++y) {
                        memory::copy(dst_subresource_mem, src_subresource_mem, zec::min(subresource_pitch, sub_image->rowPitch));
                        dst_subresource_mem += subresource_pitch;
                        src_subresource_mem += sub_image->rowPitch;
                    }
                }
            }
        }
        image.Release();
        ID3D12GraphicsCommandList* cmd_list = get_command_list(cmd_ctx);
        for (u64 subresource_idx = 0; subresource_idx < num_subresources; ++subresource_idx) {
            D3D12_TEXTURE_COPY_LOCATION dst = { };
            dst.pResource = resource;
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = u32(subresource_idx);
            D3D12_TEXTURE_COPY_LOCATION src = { };
            src.pResource = staging_resource;
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = layouts[subresource_idx];

            cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
        g_context.upload_store.push(cmd_ctx, staging_resource, allocation);

        return texture_handle;
    }

}