#include "pch.h"
#ifdef USE_D3D_RENDERER
#include "gfx/gfx.h"
#include "gfx/public_resources.h"
#include "dx_utils.h"
#include "dx_helpers.h"
#include "globals.h"
#include "timer.h"
#include "DirectXTex.h"

#include <filesystem>

#if _DEBUG
#define USE_DEBUG_DEVICE 1
#define BREAK_ON_DX_ERROR (USE_DEBUG_DEVICE && 1)
#define USE_GPU_VALIDATION 1
#endif


namespace zec
{
    using namespace dx12;

    namespace dx12
    {
        static void set_formats(SwapChain& swap_chain, const DXGI_FORMAT format)
        {
            swap_chain.format = format;
            if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
                swap_chain.non_sRGB_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            else if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                swap_chain.non_sRGB_format = DXGI_FORMAT_B8G8R8A8_UNORM;
            }
            else {
                swap_chain.non_sRGB_format = swap_chain.format;
            }
        }

        void prepare_full_screen_settings()
        {
            ASSERT(g_swap_chain.output != nullptr);

            DXGI_MODE_DESC desired_mode{};
            desired_mode.Format = g_swap_chain.non_sRGB_format;
            desired_mode.Width = g_swap_chain.width;
            desired_mode.Height = g_swap_chain.height;
            desired_mode.RefreshRate.Numerator = 0;
            desired_mode.RefreshRate.Denominator = 0;
            desired_mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
            desired_mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

            DXGI_MODE_DESC closest_match{};
            DXCall(g_swap_chain.output->FindClosestMatchingMode(&desired_mode, &closest_match, g_device));

            g_swap_chain.width = closest_match.Width;
            g_swap_chain.height = closest_match.Height;
            g_swap_chain.refresh_rate = closest_match.RefreshRate;
        }

        void recreate_swap_chain_rtv_descriptors(SwapChain& swap_chain)
        {
            // Re-create an RTV for each back buffer
            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                TextureHandle texture_handle = swap_chain.back_buffers[i];
                ID3D12Resource* resource = get_resource(g_textures, texture_handle);
                if (resource != nullptr) dx_destroy(&resource);

                const auto rtv_handle = TextureUtils::get_rtv(g_textures, texture_handle);
                DescriptorUtils::free_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtv_handle);

                DXCall(swap_chain.swap_chain->GetBuffer(UINT(i), IID_PPV_ARGS(&resource)));
                set_resource(g_textures, resource, texture_handle);

                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
                DescriptorHandle rtv = DescriptorUtils::allocate_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, &cpu_handle);
                TextureUtils::set_rtv(g_textures, g_swap_chain.back_buffers[i], rtv);

                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Format = swap_chain.format;
                rtvDesc.Texture2D.MipSlice = 0;
                rtvDesc.Texture2D.PlaneSlice = 0;
                g_device->CreateRenderTargetView(resource, &rtvDesc, cpu_handle);
                // TODO: Store MSAA info?

                resource->SetName(make_string(L"Back Buffer %llu", i).c_str());

                // Copy properties from swap chain to backbuffer textures, in case we need em
                TextureInfo& texture_info = TextureUtils::get_texture_info(g_textures, texture_handle);
                texture_info.width = swap_chain.width;
                texture_info.height = swap_chain.height;
                texture_info.depth = 1;
                texture_info.array_size = 1;
                texture_info.format = swap_chain.format;
                texture_info.num_mips = 1;


            }
            swap_chain.back_buffer_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();
        }

        void reset_swap_chain()
        {
            ASSERT(g_swap_chain.swap_chain != nullptr);

            if (g_swap_chain.output == nullptr) {
                g_swap_chain.fullscreen = false;
            }

            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                TextureHandle texture_handle = g_swap_chain.back_buffers[i];
                get_resource(g_textures, texture_handle)->Release();
                // TODO: Is this still necessary? Why not simply keep the current rtv and just re-create it? 
                // free_persistent_alloc(g_rtv_descriptor_heap, get_rtv(g_textures, texture_handle));
                // set_rtv(g_textures, texture_handle, INVALID_CPU_HANDLE);
            }

            set_formats(g_swap_chain, g_swap_chain.format);

            if (g_swap_chain.fullscreen) {
                prepare_full_screen_settings();
            }
            else {
                g_swap_chain.refresh_rate.Numerator = 60;
                g_swap_chain.refresh_rate.Denominator = 1;
            }

            DXCall(g_swap_chain.swap_chain->SetFullscreenState(g_swap_chain.fullscreen, g_swap_chain.output));

            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                DXCall(g_swap_chain.swap_chain->ResizeBuffers(
                    NUM_BACK_BUFFERS,
                    g_swap_chain.width,
                    g_swap_chain.height, g_swap_chain.non_sRGB_format,
                    DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
            }

            if (g_swap_chain.fullscreen) {
                DXGI_MODE_DESC mode;
                mode.Format = g_swap_chain.non_sRGB_format;
                mode.Width = g_swap_chain.width;
                mode.Height = g_swap_chain.height;
                mode.RefreshRate.Numerator = 0;
                mode.RefreshRate.Denominator = 0;
                mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
                mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
                DXCall(g_swap_chain.swap_chain->ResizeTarget(&mode));
            }

            recreate_swap_chain_rtv_descriptors(g_swap_chain);
        }

        void reset()
        {
            reset_swap_chain();
            process_destruction_queue(g_destruction_queue);
        }
    }

    void init_renderer(const RendererDesc& renderer_desc)
    {
        // Factory, Adaptor and Device initialization
        {
            ASSERT(g_adapter == nullptr);
            ASSERT(g_factory == nullptr);
            ASSERT(g_device == nullptr);
            constexpr int ADAPTER_NUMBER = 0;

            UINT factory_flags = 0;
        #ifdef USE_DEBUG_DEVICE
            ID3D12Debug* debug_ptr;
            DXCall(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_ptr)));
            debug_ptr->EnableDebugLayer();

            factory_flags = DXGI_CREATE_FACTORY_DEBUG;

        #ifdef USE_GPU_VALIDATION
            ID3D12Debug1* debug1;
            debug_ptr->QueryInterface(IID_PPV_ARGS(&debug1));
            debug1->SetEnableGPUBasedValidation(true);
            debug1->Release();
        #endif // USE_GPU_VALIDATION
            debug_ptr->Release();
        #endif // DEBUG

            DXCall(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&g_factory)));

            g_factory->EnumAdapters1(ADAPTER_NUMBER, &g_adapter);

            if (g_adapter == nullptr) {
                throw Exception(L"Unabled to located DXGI 1.4 adapter that supports D3D12.");
            }

            DXGI_ADAPTER_DESC1 desc{  };
            g_adapter->GetDesc1(&desc);
            write_log("Creating DX12 device on adapter '%ls'", desc.Description);

            DXCall(D3D12CreateDevice(g_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&g_device)));

            D3D_FEATURE_LEVEL feature_levels_arr[4] = {
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_12_1,
            };
            D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels = { };
            feature_levels.NumFeatureLevels = ARRAY_SIZE(feature_levels_arr);
            feature_levels.pFeatureLevelsRequested = feature_levels_arr;
            DXCall(g_device->CheckFeatureSupport(
                D3D12_FEATURE_FEATURE_LEVELS,
                &feature_levels,
                sizeof(feature_levels)
            ));
            g_supported_feature_level = feature_levels.MaxSupportedFeatureLevel;

            D3D_FEATURE_LEVEL min_feature_level = D3D_FEATURE_LEVEL_12_0;
            if (g_supported_feature_level < min_feature_level) {
                std::wstring majorLevel = to_string<int>(min_feature_level >> 12);
                std::wstring minorLevel = to_string<int>((min_feature_level >> 8) & 0xF);
                throw Exception(L"The device doesn't support the minimum feature level required to run this sample (DX" + majorLevel + L"." + minorLevel + L")");
            }

        #ifdef USE_DEBUG_DEVICE
            ID3D12InfoQueue* infoQueue;
            DXCall(g_device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

            D3D12_MESSAGE_ID disabledMessages[] =
            {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,

                // These happen when capturing with VS diagnostics
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            };

            D3D12_INFO_QUEUE_FILTER filter = { };
            filter.DenyList.NumIDs = ARRAY_SIZE(disabledMessages);
            filter.DenyList.pIDList = disabledMessages;
            infoQueue->AddStorageFilterEntries(&filter);
            infoQueue->Release();

        #endif // USE_DEBUG_DEVICE

        }

        // Initialize Memory Allocator
        {
            D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
            allocatorDesc.pDevice = g_device;
            allocatorDesc.pAdapter = g_adapter;

            DXCall(D3D12MA::CreateAllocator(&allocatorDesc, &g_allocator));
        }

        // Initialize command queues and command context pools
        {

            // Initialize graphics queue
            {
                D3D12_COMMAND_QUEUE_DESC queue_desc{ };
                queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

                DXCall(g_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&g_gfx_queue)));
                g_gfx_queue->SetName(L"Graphics queue");
            }

            // Initialize copy queue
            {
                D3D12_COMMAND_QUEUE_DESC queue_desc{ };
                queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

                DXCall(g_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&g_copy_queue)));
                g_copy_queue->SetName(L"Copy Queue");
            }


            dx12::CommandContextUtils::initialize_pools();
        }

        // Initialize descriptor heaps
        DescriptorUtils::init_descriptor_heaps();

        // Swapchain initialization
        {
            ASSERT(g_swap_chain.swap_chain == nullptr);

            for (size_t i = 0; i < NUM_BACK_BUFFERS; i++) {
                g_swap_chain.back_buffers[i] = TextureUtils::push_back(g_textures, Texture{});
            }

            SwapChainDesc swap_chain_desc{ };
            swap_chain_desc.width = renderer_desc.width;
            swap_chain_desc.height = renderer_desc.height;
            swap_chain_desc.fullscreen = renderer_desc.fullscreen;
            swap_chain_desc.vsync = renderer_desc.vsync;
            swap_chain_desc.output_window = renderer_desc.window;
            swap_chain_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

            g_adapter->EnumOutputs(0, &g_swap_chain.output);

            g_swap_chain.width = swap_chain_desc.width;
            g_swap_chain.height = swap_chain_desc.height;
            g_swap_chain.fullscreen = swap_chain_desc.fullscreen;
            g_swap_chain.vsync = swap_chain_desc.vsync;

            set_formats(g_swap_chain, swap_chain_desc.format);

            DXGI_SWAP_CHAIN_DESC d3d_swap_chain_desc = {  };
            d3d_swap_chain_desc.BufferCount = NUM_BACK_BUFFERS;
            d3d_swap_chain_desc.BufferDesc.Width = g_swap_chain.width;
            d3d_swap_chain_desc.BufferDesc.Height = g_swap_chain.height;
            d3d_swap_chain_desc.BufferDesc.Format = g_swap_chain.non_sRGB_format;
            d3d_swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            d3d_swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            d3d_swap_chain_desc.SampleDesc.Count = 1;
            d3d_swap_chain_desc.OutputWindow = swap_chain_desc.output_window;
            d3d_swap_chain_desc.Windowed = TRUE;
            d3d_swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

            IDXGISwapChain* d3d_swap_chain = nullptr;
            DXCall(g_factory->CreateSwapChain(g_gfx_queue, &d3d_swap_chain_desc, &d3d_swap_chain));
            DXCall(d3d_swap_chain->QueryInterface(IID_PPV_ARGS(&g_swap_chain.swap_chain)));
            d3d_swap_chain->Release();

            g_swap_chain.back_buffer_idx = g_swap_chain.swap_chain->GetCurrentBackBufferIndex();
            g_swap_chain.waitable_object = g_swap_chain.swap_chain->GetFrameLatencyWaitableObject();

            recreate_swap_chain_rtv_descriptors(g_swap_chain);
        }
        g_frame_fence = create_fence(g_fences, 0);

        g_upload_manager.init({
            g_device,
            g_allocator,
            g_copy_queue,
            &g_fences });
    }

    void destroy_renderer()
    {

        write_log("Destroying renderer");
        wait_for_gpu();

        // Swap Chain Destruction
        DescriptorHeap& rtv_heap = g_descriptor_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
        destroy(g_destruction_queue, g_textures, rtv_heap, g_swap_chain);

        g_root_signatures.destroy();
        g_pipelines.destroy();

        g_upload_manager.destroy();
        destroy(
            g_destruction_queue,
            g_descriptor_heaps,
            g_textures,
            g_current_frame_idx
        );
        destroy(g_destruction_queue, g_buffers);
        destroy(g_destruction_queue, g_fences);

        DescriptorUtils::destroy_descriptor_heaps();

        CommandContextUtils::destroy_pools();

        dx_destroy(&g_gfx_queue);
        dx_destroy(&g_copy_queue);

        destroy(g_destruction_queue);
        dx_destroy(&g_allocator);

        dx_destroy(&g_device);
        dx_destroy(&g_adapter);
        dx_destroy(&g_factory);
    }

    void wait_for_gpu()
    {
        for (size_t i = 0; i < ARRAY_SIZE(g_command_pools); i++) {
            wait(g_command_pools[i].fence, g_command_pools[i].last_used_fence_value);
            CommandContextUtils::reset(g_command_pools[i]);
        }
    }

    void begin_upload()
    {
        g_upload_manager.begin_upload();
    }

    void end_upload()
    {
        g_upload_manager.end_upload();
        g_gfx_queue->Wait(g_upload_manager.fence.d3d_fence, g_upload_manager.current_fence_value);
    }

    CommandContextHandle begin_frame()
    {
        if (g_current_cpu_frame != 0) {
            // Wait for the GPU to catch up so we can freely reset our frame's command allocator
            const u64 gpu_lag = g_current_cpu_frame - g_current_gpu_frame;
            ASSERT(gpu_lag <= RENDER_LATENCY);

            if (gpu_lag == RENDER_LATENCY) {
                wait(g_frame_fence, g_current_gpu_frame + 1);
                ++g_current_gpu_frame;
            }

            g_current_frame_idx = g_swap_chain.swap_chain->GetCurrentBackBufferIndex();

            for (size_t i = 0; i < ARRAY_SIZE(g_command_pools); i++) {
                CommandContextUtils::reset(g_command_pools[i]);
            }
        }

        // Process resources queued for destruction
        process_destruction_queue(g_destruction_queue);

        for (auto& descriptor_heap : g_descriptor_heaps) {
            DescriptorUtils::process_destruction_queue(descriptor_heap, g_current_frame_idx);
        }

        auto& heap = g_descriptor_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];


        CommandContextHandle command_context = gfx::cmd::provision(CommandQueueType::GRAPHICS);
        // TODO: Provide interface for creating barriers? Maybe that needs to be handled as part of the render graph though

        const TextureHandle backbuffer = get_current_back_buffer_handle(g_swap_chain, g_current_frame_idx);

        D3D12_RESOURCE_BARRIER barrier{  };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = get_resource(g_textures, backbuffer);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        CommandContextUtils::insert_resource_barrier(command_context, &barrier, 1);

        return command_context;
    }

    void end_frame(const CommandContextHandle command_context)
    {
        const TextureHandle backbuffer = get_current_back_buffer_handle(g_swap_chain, g_current_frame_idx);

        D3D12_RESOURCE_BARRIER barrier{  };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = get_resource(g_textures, backbuffer);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        CommandContextUtils::insert_resource_barrier(command_context, &barrier, 1);
        CommandContextUtils::return_and_execute(&command_context, 1);

        // Present the frame.
        if (g_swap_chain.swap_chain != nullptr) {
            u32 sync_intervals = g_swap_chain.vsync ? 1 : 0;
            DXCall(g_swap_chain.swap_chain->Present(sync_intervals, g_swap_chain.vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING));
        }

        // Increment our cpu frame couter to indicate submission
        signal(g_frame_fence, g_gfx_queue, g_current_cpu_frame++);
    }

    // ---------- Resource Queries ----------

    u32 get_shader_readable_texture_index(const TextureHandle handle)
    {
        return TextureUtils::get_srv_index(g_textures, handle);
    }

    TextureHandle get_current_back_buffer_handle()
    {
        return dx12::get_current_back_buffer_handle(g_swap_chain, g_current_frame_idx);
    }

    // ---------- Resource creation ----------

    BufferHandle create_buffer(BufferDesc desc)
    {
        ASSERT(desc.byte_size != 0);
        ASSERT(desc.usage != RESOURCE_USAGE_UNUSED);

        BufferHandle handle = { g_buffers.create_back() };
        Buffer& buffer = g_buffers.get(handle);

        init(buffer, desc, g_allocator);

        if (desc.usage & RESOURCE_USAGE_DYNAMIC && desc.data != nullptr) {
            for (size_t i = 0; i < RENDER_LATENCY; i++) {
                update_buffer(buffer, desc.data, desc.byte_size, i);
            }
        }
        else if (desc.data != nullptr) {
            g_upload_manager.queue_upload(desc, buffer.resource);
        }

        return handle;
    }

    MeshHandle create_mesh(MeshDesc mesh_desc)
    {
        Mesh mesh{};
        // Index buffer creation
        {
            ASSERT(mesh_desc.index_buffer_desc.usage == RESOURCE_USAGE_INDEX);
            mesh.index_buffer_handle = create_buffer(mesh_desc.index_buffer_desc);

            const Buffer& index_buffer = g_buffers[mesh.index_buffer_handle];
            mesh.index_buffer_view.BufferLocation = index_buffer.gpu_address;
            ASSERT(index_buffer.size < u64(UINT32_MAX));
            mesh.index_buffer_view.SizeInBytes = u32(index_buffer.size);

            switch (mesh_desc.index_buffer_desc.stride) {
            case 2u:
                mesh.index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
                break;
            case 4u:
                mesh.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
                break;
            default:
                throw std::runtime_error("Cannot create an index buffer that isn't u16 or u32");
            }

            mesh.index_count = mesh_desc.index_buffer_desc.byte_size / mesh_desc.index_buffer_desc.stride;
        }

        for (size_t i = 0; i < ARRAY_SIZE(mesh_desc.vertex_buffer_descs); i++) {
            const BufferDesc& attr_desc = mesh_desc.vertex_buffer_descs[i];
            if (attr_desc.usage == RESOURCE_USAGE_UNUSED) break;

            // TODO: Is this actually needed?
            ASSERT(attr_desc.usage == RESOURCE_USAGE_VERTEX);
            ASSERT(attr_desc.type == BufferType::DEFAULT);

            mesh.vertex_buffer_handles[i] = create_buffer(attr_desc);
            const Buffer& buffer = g_buffers[mesh.vertex_buffer_handles[i]];

            D3D12_VERTEX_BUFFER_VIEW& view = mesh.buffer_views[i];
            view.BufferLocation = buffer.gpu_address;
            view.StrideInBytes = attr_desc.stride;
            view.SizeInBytes = attr_desc.byte_size;
            mesh.num_vertex_buffers++;
        }

        // TODO: Support blend weights, indices
        return { u32(g_meshes.push_back(mesh)) };
    }

    TextureHandle create_texture(TextureDesc desc)
    {
        Texture texture = {
            .info = {
                .width = desc.width,
                .height = desc.height,
                .depth = desc.depth,
                .num_mips = desc.num_mips,
                .array_size = desc.array_size,
                .format = to_d3d_format(desc.format),
                .is_cubemap = desc.is_cubemap
            }
        };

        D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        D3D12_CLEAR_VALUE optimized_clear_value = { };

        if (desc.usage & RESOURCE_USAGE_DEPTH_STENCIL) {
            optimized_clear_value.Format = texture.info.format;
            optimized_clear_value.DepthStencil = { 1.0f, 0 };
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }

        if (desc.usage & RESOURCE_USAGE_RENDER_TARGET) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            optimized_clear_value.Format = texture.info.format;
            optimized_clear_value.Color[0] = 0.0f;
            optimized_clear_value.Color[1] = 0.0f;
            optimized_clear_value.Color[2] = 0.0f;
            optimized_clear_value.Color[3] = 1.0f;
            if (desc.usage & RESOURCE_USAGE_SHADER_READABLE) {
                initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            }
            else {
                initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
            }
        }

        D3D12_RESOURCE_DESC d3d_desc = {
            .Dimension = desc.is_3d ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = texture.info.width,
            .Height = texture.info.height,
            .DepthOrArraySize = desc.is_3d ? u16(texture.info.depth) : u16(texture.info.array_size),
            .MipLevels = u16(desc.num_mips),
            .Format = texture.info.format,
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
            DescriptorHandle srv = DescriptorUtils::allocate_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &cpu_handle);
            texture.srv = srv;

            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = { };

            if (desc.is_cubemap) {
                ASSERT(texture.info.array_size == 6);
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.TextureCube.MostDetailedMip = 0;
                srv_desc.TextureCube.MipLevels = texture.info.num_mips;
                srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
            }

            if (desc.usage & RESOURCE_USAGE_RENDER_TARGET) {
                g_device->CreateShaderResourceView(texture.resource, nullptr, cpu_handle);
            }
            else {
                g_device->CreateShaderResourceView(texture.resource, &srv_desc, cpu_handle);
            }
        }

        if (desc.usage & RESOURCE_USAGE_RENDER_TARGET) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
            DescriptorHandle rtv = DescriptorUtils::allocate_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, &cpu_handle);
            texture.rtv = rtv;

            g_device->CreateRenderTargetView(texture.resource, nullptr, cpu_handle);
        }

        if (desc.usage & RESOURCE_USAGE_DEPTH_STENCIL) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
            DescriptorHandle dsv = DescriptorUtils::allocate_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, &cpu_handle);
            texture.dsv = dsv;

            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
                .Format = texture.info.format,
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

    ResourceLayoutHandle create_resource_layout(const ResourceLayoutDesc& desc)
    {

        constexpr u64 MAX_DWORDS_IN_RS = 64;
        ASSERT(desc.num_constants < MAX_DWORDS_IN_RS);
        ASSERT(desc.num_constant_buffers < MAX_DWORDS_IN_RS / 2);
        ASSERT(desc.num_resource_tables < MAX_DWORDS_IN_RS);

        ASSERT(desc.num_constants < ARRAY_SIZE(desc.constants));
        ASSERT(desc.num_constant_buffers < ARRAY_SIZE(desc.constant_buffers));
        ASSERT(desc.num_resource_tables < ARRAY_SIZE(desc.tables));

        i8 remaining_dwords = i8(MAX_DWORDS_IN_RS);
        D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
        // In reality, inline descriptors use up 2 DWORDs so the limit is actually lower;
        D3D12_ROOT_PARAMETER root_parameters[MAX_DWORDS_IN_RS];

        u64 parameter_count = 0;
        for (u32 i = 0; i < desc.num_constants; i++) {
            D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count];
            parameter.ShaderVisibility = to_d3d_visibility(desc.constants[i].visibility);
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            parameter.Constants = {
                .ShaderRegister = i,
                .RegisterSpace = 0,
                .Num32BitValues = desc.constants[i].num_constants,
            };
            remaining_dwords -= 2 * desc.constants[i].num_constants;
            parameter_count++;
        }

        for (u32 i = 0; i < desc.num_constant_buffers; i++) {
            D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count];
            parameter.ShaderVisibility = to_d3d_visibility(desc.constant_buffers[i].visibility);
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            parameter.Descriptor = {
                .ShaderRegister = desc.num_constants + i, // Since the constants will immediately precede these
                .RegisterSpace = 0,
            };
            remaining_dwords--;
            parameter_count++;
        }

        for (u32 i = 0; i < desc.num_resource_tables; i++) {
            D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count];
            parameter.ShaderVisibility = to_d3d_visibility(desc.tables[i].visibility);
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.DescriptorTable = {};

            constexpr u64 num_ranges = ARRAY_SIZE(desc.tables[i].ranges);
            D3D12_DESCRIPTOR_RANGE d3d_ranges[num_ranges] = { };
            parameter.DescriptorTable.pDescriptorRanges = d3d_ranges;

            u32 srv_count = 0;
            u32 uav_count = 0;
            for (size_t range_idx = 0; range_idx < num_ranges; range_idx++) {
                const ResourceLayoutRangeDesc& range_desc = desc.tables[i].ranges[range_idx];
                if (range_desc.usage == ResourceAccess::UNUSED) {
                    parameter.DescriptorTable.NumDescriptorRanges = range_idx;
                    break;
                }
                bool is_srv_range = range_desc.usage == ResourceAccess::READ;
                auto range_type = (is_srv_range) ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                u32 base_shader_register = is_srv_range ? srv_count : uav_count;
                d3d_ranges[range_idx] = {
                    .RangeType = range_type,
                    .NumDescriptors = range_desc.count,
                    .BaseShaderRegister = base_shader_register,
                    // Every table gets placed in its own space... not sure this is a good idea? But let's keep it simple for now.
                    // In the future we might want to consider putting all unbounded tables in the zero-th space? but that's complicated
                    .RegisterSpace = i + 1,
                    .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
                };

                if (is_srv_range) {
                    srv_count += range_desc.count;
                }
                else {
                    uav_count += range_desc.count;
                }
            }

            remaining_dwords--;
            parameter_count++;
        }

        ASSERT(remaining_dwords > 0);
        D3D12_STATIC_SAMPLER_DESC static_sampler_descs[ARRAY_SIZE(desc.static_samplers)] = { };

        for (size_t i = 0; i < desc.num_static_samplers; i++) {
            const auto& sampler_desc = desc.static_samplers[i];
            D3D12_STATIC_SAMPLER_DESC& d3d_sampler_desc = static_sampler_descs[i];
            d3d_sampler_desc.Filter = dx12::to_d3d_filter(sampler_desc.filtering);
            d3d_sampler_desc.AddressU = dx12::to_d3d_address_mode(sampler_desc.wrap_u);
            d3d_sampler_desc.AddressV = dx12::to_d3d_address_mode(sampler_desc.wrap_v);
            d3d_sampler_desc.AddressW = dx12::to_d3d_address_mode(sampler_desc.wrap_w);
            d3d_sampler_desc.MipLODBias = 0.0f;
            d3d_sampler_desc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
            d3d_sampler_desc.MinLOD = 0.0f;
            d3d_sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
            d3d_sampler_desc.ShaderRegister = sampler_desc.binding_slot;
            d3d_sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        root_signature_desc.pParameters = root_parameters;
        root_signature_desc.NumParameters = desc.num_constants + desc.num_constant_buffers + desc.num_resource_tables;
        root_signature_desc.NumStaticSamplers = desc.num_static_samplers;
        root_signature_desc.pStaticSamplers = static_sampler_descs;
        root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3D12RootSignature* root_signature = nullptr;

        ID3DBlob* signature;
        ID3DBlob* error;
        DXCall(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        DXCall(g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

        if (error != nullptr) {
            print_blob(error);
            error->Release();
            throw std::runtime_error("Failed to create root signature");
        }
        signature != nullptr && signature->Release();

        return g_root_signatures.push_back(root_signature);
    }

    TextureHandle zec::load_texture_from_file(const char* file_path)
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


        Texture texture = {
            .info = {
                .width = u32(meta_data.width),
                .height = u32(meta_data.height),
                .depth = u32(meta_data.depth),
                .num_mips = u32(meta_data.mipLevels),
                .array_size = u32(meta_data.arraySize),
                .format = meta_data.format,
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
            .Format = texture.info.format,
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
        DescriptorHandle srv = DescriptorUtils::allocate_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &cpu_handle);
        texture.srv = srv;

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = texture.info.format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {
                .MostDetailedMip = 0,
                .MipLevels = texture.info.num_mips,
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

    void update_buffer(const BufferHandle buffer_id, const void* data, u64 byte_size)
    {
        Buffer& buffer = g_buffers[buffer_id];
        update_buffer(buffer, data, byte_size, g_current_frame_idx);
    }
}
#endif // USE_D3D_RENDERER