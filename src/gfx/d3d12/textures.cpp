#include "pch.h"
#include <filesystem>
#include "textures.h"
#include "dx_helpers.h"
#include "upload_manager.h"
#include "globals.h"

namespace zec::gfx::dx12
{
    namespace TextureUtils
    {
        TextureHandle push_back(TextureList& list, const Texture& texture)
        {
            ASSERT(list.resources.size < UINT32_MAX);
            TextureHandle handle = { u32(list.resources.push_back(texture.resource)) };
            list.allocations.push_back(texture.allocation);
            list.srvs.push_back(texture.srv);
            list.uavs.push_back(texture.uav);
            list.rtvs.push_back(texture.rtv);
            list.infos.push_back(texture.info);
            list.render_target_infos.push_back(texture.render_target_info);

            if (DescriptorUtils::is_valid(texture.dsv)) {
                list.dsv_infos.push_back({ handle, texture.dsv });
            };

            return handle;
        }

        DescriptorRangeHandle get_dsv(TextureList& texture_list, TextureHandle handle)
        {
            for (size_t i = 0; i < texture_list.dsv_infos.size; i++) {
                const auto& dsv_info = texture_list.dsv_infos[i];
                if (dsv_info.handle == handle) { return dsv_info.dsv; }
            }
            throw std::runtime_error("This texture was not created as a depth stencil buffer");
        }
    }
}

using namespace zec::gfx::dx12;

namespace zec::gfx::textures
{
    TextureHandle create(TextureDesc desc)
    {
        Texture texture = {
            .info = {
                .width = desc.width,
                .height = desc.height,
                .depth = desc.depth,
                .num_mips = desc.num_mips,
                .array_size = desc.array_size,
                .format = desc.format,
                .is_cubemap = desc.is_cubemap
            }
        };

        DXGI_FORMAT d3d_format = to_d3d_format(texture.info.format);
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
        switch (desc.usage) {
        case RESOURCE_USAGE_SHADER_READABLE:
            initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            break;
        case RESOURCE_USAGE_COMPUTE_WRITABLE:
            initial_state = D3D12_RESOURCE_STATE_COMMON;
            break;
        case RESOURCE_USAGE_RENDER_TARGET:
            initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
            break;
        case RESOURCE_USAGE_DEPTH_STENCIL:
            initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            break;
        default:
            ASSERT(desc.initial_state != RESOURCE_USAGE_UNUSED);
            ASSERT(desc.initial_state & desc.usage);
            initial_state = to_d3d_resource_state(desc.initial_state);
            break;
        }

        if (desc.usage & RESOURCE_USAGE_COMPUTE_WRITABLE) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
        }

        D3D12_CLEAR_VALUE optimized_clear_value = { };
        if (desc.usage & RESOURCE_USAGE_DEPTH_STENCIL) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            optimized_clear_value.Format = d3d_format;
            optimized_clear_value.DepthStencil = { 1.0f, 0 };
        }

        if (desc.usage & RESOURCE_USAGE_RENDER_TARGET) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            optimized_clear_value.Format = d3d_format;
            optimized_clear_value.Color[0] = 0.0f;
            optimized_clear_value.Color[1] = 0.0f;
            optimized_clear_value.Color[2] = 0.0f;
            optimized_clear_value.Color[3] = 1.0f;
        }

        u16 depth_or_array_size = u16(desc.array_size);
        ASSERT(!desc.is_cubemap || desc.array_size == 6);
        if (desc.is_3d) {
            // Basically not handling an array of 3D textures for now.
            ASSERT(!desc.is_cubemap);
            ASSERT(desc.array_size == 1);
            depth_or_array_size = u16(desc.depth);
        }

        D3D12_RESOURCE_DESC d3d_desc = {
            .Dimension = desc.is_3d ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = texture.info.width,
            .Height = texture.info.height,
            .DepthOrArraySize = depth_or_array_size,
            .MipLevels = u16(desc.num_mips),
            .Format = d3d_format,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
             },
             .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
             .Flags = flags
        };

        D3D12MA::ALLOCATION_DESC alloc_desc = {
            .HeapType = D3D12_HEAP_TYPE_DEFAULT
        };

        const bool depth_or_rt = bool(desc.usage & (RESOURCE_USAGE_DEPTH_STENCIL | RESOURCE_USAGE_RENDER_TARGET));
        DXCall(g_allocator->CreateResource(
            &alloc_desc,
            &d3d_desc,
            initial_state,
            depth_or_rt ? &optimized_clear_value : nullptr,
            &texture.allocation,
            IID_PPV_ARGS(&texture.resource)
        ));

        if (desc.usage & RESOURCE_USAGE_SHADER_READABLE) {

            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
            DescriptorRangeHandle srv = DescriptorUtils::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, &cpu_handle);
            texture.srv = srv;

            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
                .Format = d3d_desc.Format,
                .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2D = {
                    .MostDetailedMip = 0,
                    .MipLevels = u32(-1),
                    .ResourceMinLODClamp = 0.0f,
                 },
            };

            if (desc.is_cubemap) {
                ASSERT(texture.info.array_size == 6);
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.TextureCube.MostDetailedMip = 0;
                srv_desc.TextureCube.MipLevels = texture.info.num_mips;
                srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
            }

            // TODO: 3D Texture support
            ASSERT(!desc.is_3d);

            if (desc.usage & RESOURCE_USAGE_RENDER_TARGET) {
                g_device->CreateShaderResourceView(texture.resource, nullptr, cpu_handle);
            }
            else {
                g_device->CreateShaderResourceView(texture.resource, &srv_desc, cpu_handle);
            }
        }

        if (desc.usage & RESOURCE_USAGE_COMPUTE_WRITABLE) {
            constexpr u32 MAX_NUM_MIPS = 16;
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[MAX_NUM_MIPS] = {};
            ASSERT(texture.info.num_mips < MAX_NUM_MIPS);
            DescriptorRangeHandle uav = DescriptorUtils::allocate_descriptors(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                texture.info.num_mips,
                cpu_handles
            );
            texture.uav = uav;

            // TODO: 3D texture support
            ASSERT(!desc.is_3d);

            for (u32 i = 0; i < texture.info.num_mips; i++) {
                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
                    .Format = d3d_format,
                    .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
                };

                if (desc.is_cubemap || desc.array_size > 1) {
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                    uav_desc.Texture2DArray = {
                        .MipSlice = i,
                        .FirstArraySlice = 0,
                        .ArraySize = texture.info.array_size,
                        .PlaneSlice = 0
                    };
                }
                else {
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    uav_desc.Texture2D = {
                        .MipSlice = i,
                        .PlaneSlice = 0
                    };
                }
                g_device->CreateUnorderedAccessView(texture.resource, nullptr, &uav_desc, cpu_handles[i]);
            }
        }

        if (desc.usage & RESOURCE_USAGE_RENDER_TARGET) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
            DescriptorRangeHandle rtv = DescriptorUtils::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, &cpu_handle);
            texture.rtv = rtv;

            g_device->CreateRenderTargetView(texture.resource, nullptr, cpu_handle);
        }

        if (desc.usage & RESOURCE_USAGE_DEPTH_STENCIL) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
            DescriptorRangeHandle dsv = DescriptorUtils::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, texture.info.array_size, &cpu_handle);
            texture.dsv = dsv;

            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
                .Format = d3d_format,
                .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                .Flags = D3D12_DSV_FLAG_NONE,
                .Texture2D = {
                    .MipSlice = 0,
                },
            };

            g_device->CreateDepthStencilView(texture.resource, &dsv_desc, cpu_handle);
        }

        return TextureUtils::push_back(g_textures, texture);
    }

    u32 get_shader_readable_index(const TextureHandle handle)
    {
        return TextureUtils::get_srv_index(g_textures, handle);
    }

    u32 get_shader_writable_index(const TextureHandle handle)
    {
        return TextureUtils::get_uav_index(g_textures, handle);
    }

    TextureInfo& get_texture_info(const TextureHandle texture_handle)
    {
        return dx12::TextureUtils::get_texture_info(dx12::g_textures, texture_handle);
    };

    TextureHandle load_from_file(const char* file_path, const bool force_srgb)
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
        else {
            throw std::runtime_error("Wasn't able to load file!");
        }

        const DirectX::TexMetadata meta_data = image.GetMetadata();
        bool is_3d = meta_data.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;

        DXGI_FORMAT d3d_format = meta_data.format;
        if (force_srgb) {
            d3d_format = to_srgb_format(d3d_format);
        }

        Texture texture = {
            .info = {
                .width = u32(meta_data.width),
                .height = u32(meta_data.height),
                .depth = u32(meta_data.depth),
                .num_mips = u32(meta_data.mipLevels),
                .array_size = u32(meta_data.arraySize),
                .format = from_d3d_format(d3d_format),
                .is_cubemap = u16(meta_data.IsCubemap())
            }
        };

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_RESOURCE_DESC d3d_desc = {
            .Dimension = is_3d ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = texture.info.width,
            .Height = texture.info.height,
            .DepthOrArraySize = is_3d ? u16(texture.info.depth) : u16(texture.info.array_size),
            .MipLevels = u16(meta_data.mipLevels),
            .Format = d3d_format,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
             },
             .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
             .Flags = flags
        };

        D3D12MA::ALLOCATION_DESC alloc_desc = {
            .HeapType = D3D12_HEAP_TYPE_DEFAULT
        };

        DXCall(g_allocator->CreateResource(
            &alloc_desc,
            &d3d_desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            &texture.allocation,
            IID_PPV_ARGS(&texture.resource)
        ));

        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
        DescriptorRangeHandle srv = DescriptorUtils::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, &cpu_handle);
        texture.srv = srv;

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = d3d_format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {
                .MostDetailedMip = 0,
                .MipLevels = u32(-1),
                .ResourceMinLODClamp = 0.0f,
             },
        };

        if (meta_data.IsCubemap()) {
            ASSERT(texture.info.array_size == 6);
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.TextureCube.MostDetailedMip = 0;
            srv_desc.TextureCube.MipLevels = texture.info.num_mips;
            srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
        }

        g_device->CreateShaderResourceView(texture.resource, &srv_desc, cpu_handle);

        texture.resource->SetName(path.c_str());

        TextureUploadDesc texture_upload_desc{
            .data = &image,
            .d3d_texture_desc = d3d_desc,
            .num_subresources = meta_data.mipLevels * meta_data.arraySize,
            .is_cube_map = meta_data.IsCubemap(),
        };
        g_upload_manager.queue_upload(texture_upload_desc, texture);

        return TextureUtils::push_back(g_textures, texture);
    }

    void save_to_file(const TextureHandle texture_handle, const wchar_t* file_path, const ResourceUsage current_usage)
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