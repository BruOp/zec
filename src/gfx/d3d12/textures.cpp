#include "pch.h"
#include "textures.h"

namespace zec::gfx::dx12
{
    TextureHandle TextureList::push_back(const Texture& texture)
    {
        ASSERT(count < UINT32_MAX);
        count++;
        TextureHandle handle = { u32(resources.push_back(texture.resource)) };
        allocations.push_back(texture.allocation);
        srvs.push_back(texture.srv);
        uavs.push_back(texture.uav);
        rtvs.push_back(texture.rtv);
        infos.push_back(texture.info);
        render_target_infos.push_back(texture.render_target_info);

        if (descriptor_utils::is_valid(texture.dsv)) {
            dsv_infos.push_back({ handle, texture.dsv });
        };

        return handle;
    }

    DescriptorRangeHandle& DSVStore::operator[](const TextureHandle handle)
    {
        for (size_t i = 0; i < data.size; i++) {
            auto& dsv_info = data[i];
            if (dsv_info.handle == handle) { return dsv_info.dsv; }
        }
        throw std::runtime_error("This texture was not created as a depth stencil buffer");
    }
    const DescriptorRangeHandle& DSVStore::operator[](const TextureHandle handle) const
    {
        for (size_t i = 0; i < data.size; i++) {
            const auto& dsv_info = data[i];
            if (dsv_info.handle == handle) { return dsv_info.dsv; }
        }
        throw std::runtime_error("This texture was not created as a depth stencil buffer");
    }
}

using namespace zec::gfx::dx12;

namespace zec::gfx::textures
{
    //TextureHandle load_from_file(const char* file_path, const bool force_srgb)
    //{
    //    std::wstring path = ansi_to_wstring(file_path);
    //    const std::filesystem::path filesystem_path{ file_path };

    //    DirectX::ScratchImage image;
    //    const auto& extension = filesystem_path.extension();

    //    if (extension.compare(L".dds") == 0 || extension.compare(L".DDS") == 0) {
    //        DXCall(DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
    //    }
    //    else if (extension.compare(L".hdr") == 0 || extension.compare(L".HDR") == 0) {
    //        DXCall(DirectX::LoadFromHDRFile(path.c_str(), nullptr, image));
    //    }
    //    else if (extension.compare(L".png") == 0 || extension.compare(L".PNG") == 0) {
    //        DXCall(DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image));
    //    }
    //    else {
    //        throw std::runtime_error("Wasn't able to load file!");
    //    }

    //    const DirectX::TexMetadata meta_data = image.GetMetadata();
    //    bool is_3d = meta_data.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;

    //    DXGI_FORMAT d3d_format = meta_data.format;
    //    if (force_srgb) {
    //        d3d_format = to_srgb_format(d3d_format);
    //    }

    //    Texture texture = {
    //        .info = {
    //            .width = u32(meta_data.width),
    //            .height = u32(meta_data.height),
    //            .depth = u32(meta_data.depth),
    //            .num_mips = u32(meta_data.mipLevels),
    //            .array_size = u32(meta_data.arraySize),
    //            .format = from_d3d_format(d3d_format),
    //            .is_cubemap = u16(meta_data.IsCubemap())
    //        }
    //    };

    //    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    //    D3D12_RESOURCE_DESC d3d_desc = {
    //        .Dimension = is_3d ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D,
    //        .Alignment = 0,
    //        .Width = texture.info.width,
    //        .Height = texture.info.height,
    //        .DepthOrArraySize = is_3d ? u16(texture.info.depth) : u16(texture.info.array_size),
    //        .MipLevels = u16(meta_data.mipLevels),
    //        .Format = d3d_format,
    //        .SampleDesc = {
    //            .Count = 1,
    //            .Quality = 0,
    //         },
    //         .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
    //         .Flags = flags
    //    };

    //    D3D12MA::ALLOCATION_DESC alloc_desc = {
    //        .HeapType = D3D12_HEAP_TYPE_DEFAULT
    //    };

    //    DXCall(g_allocator->CreateResource(
    //        &alloc_desc,
    //        &d3d_desc,
    //        D3D12_RESOURCE_STATE_COMMON,
    //        nullptr,
    //        &texture.allocation,
    //        IID_PPV_ARGS(&texture.resource)
    //    ));

    //    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
    //    DescriptorRangeHandle srv = descriptor_utils::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, &cpu_handle);
    //    texture.srv = srv;

    //    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
    //        .Format = d3d_format,
    //        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
    //        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    //        .Texture2D = {
    //            .MostDetailedMip = 0,
    //            .MipLevels = u32(-1),
    //            .ResourceMinLODClamp = 0.0f,
    //         },
    //    };

    //    if (meta_data.IsCubemap()) {
    //        ASSERT(texture.info.array_size == 6);
    //        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    //        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    //        srv_desc.TextureCube.MostDetailedMip = 0;
    //        srv_desc.TextureCube.MipLevels = texture.info.num_mips;
    //        srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
    //    }

    //    device->CreateShaderResourceView(texture.resource, &srv_desc, cpu_handle);

    //    texture.resource->SetName(path.c_str());

    //    TextureUploadDesc texture_upload_desc{
    //        .data = &image,
    //        .d3d_texture_desc = d3d_desc,
    //        .num_subresources = meta_data.mipLevels * meta_data.arraySize,
    //        .is_cube_map = meta_data.IsCubemap(),
    //    };
    //    upload_manager.queue_upload(texture_upload_desc, texture);

    //    return texture_utils::push_back(g_textures, texture);
    //}

    //void save_to_file(const TextureHandle texture_handle, const wchar_t* file_path, const ResourceUsage current_usage)
    //{
    //    DirectX::ScratchImage scratch{ };
    //    const TextureInfo& texture_info = get_texture_info(texture_handle);
    //    ID3D12Resource* resource = dx12::get_resource(dx12::g_textures, texture_handle);
    //    ID3D12CommandQueue* gfx_queue = dx12::cmd_utils::get_pool(CommandQueueType::GRAPHICS).queue;
    //    DXCall(DirectX::CaptureTexture(
    //        gfx_queue,
    //        resource,
    //        texture_info.is_cubemap,
    //        scratch,
    //        dx12::to_d3d_resource_state(current_usage),
    //        dx12::to_d3d_resource_state(current_usage)
    //    ));

    //    DXCall(DirectX::SaveToDDSFile(scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(), DirectX::DDS_FLAGS_NONE, file_path));
    //}
}