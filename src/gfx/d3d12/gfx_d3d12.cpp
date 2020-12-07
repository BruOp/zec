#include "pch.h"
#ifdef USE_D3D_RENDERER
#include "gfx/gfx.h"
#include "gfx/public_resources.h"
#include "dx_utils.h"
#include "dx_helpers.h"
#include "globals.h"
#include "timer.h"
#include "textures.h"

#if _DEBUG
#define USE_DEBUG_DEVICE 1
#define BREAK_ON_DX_ERROR (USE_DEBUG_DEVICE && 1)
#define USE_GPU_VALIDATION 1
#endif


namespace zec::gfx
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

                const auto rtv_handle = texture_utils::get_rtv(g_textures, texture_handle);
                descriptor_utils::free_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtv_handle);

                DXCall(swap_chain.swap_chain->GetBuffer(UINT(i), IID_PPV_ARGS(&resource)));
                set_resource(g_textures, resource, texture_handle);

                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
                DescriptorRangeHandle rtv = descriptor_utils::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, &cpu_handle);
                texture_utils::set_rtv(g_textures, g_swap_chain.back_buffers[i], rtv);

                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Format = swap_chain.format;
                rtvDesc.Texture2D.MipSlice = 0;
                rtvDesc.Texture2D.PlaneSlice = 0;
                g_device->CreateRenderTargetView(resource, &rtvDesc, cpu_handle);
                // TODO: Store MSAA info?

                resource->SetName(make_string(L"Back Buffer %llu", i).c_str());

                // Copy properties from swap chain to backbuffer textures, in case we need em
                TextureInfo& texture_info = texture_utils::get_texture_info(g_textures, texture_handle);
                texture_info.width = swap_chain.width;
                texture_info.height = swap_chain.height;
                texture_info.depth = 1;
                texture_info.array_size = 1;
                texture_info.format = from_d3d_format(swap_chain.format);
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
            wait_for_gpu();
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

            DXCall(D3D12CreateDevice(g_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)));

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

            D3D_FEATURE_LEVEL min_feature_level = D3D_FEATURE_LEVEL_11_0;
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

            // Initialize compute queue
            {
                D3D12_COMMAND_QUEUE_DESC queue_desc{ };
                queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

                DXCall(g_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&g_compute_queue)));
                g_compute_queue->SetName(L"Compute queue");
            }

            // Initialize copy queue
            {
                D3D12_COMMAND_QUEUE_DESC queue_desc{ };
                queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

                DXCall(g_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&g_copy_queue)));
                g_copy_queue->SetName(L"Copy Queue");
            }


            dx12::cmd_utils::initialize_pools();
        }

        // Initialize descriptor heaps
        descriptor_utils::init_descriptor_heaps();

        // Swapchain initialization
        {
            ASSERT(g_swap_chain.swap_chain == nullptr);

            for (size_t i = 0; i < NUM_BACK_BUFFERS; i++) {
                g_swap_chain.back_buffers[i] = texture_utils::push_back(g_textures, Texture{});
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
            .type = CommandQueueType::COPY,
            .device = g_device,
            .allocator = g_allocator,
            });
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

        descriptor_utils::destroy_descriptor_heaps();

        cmd_utils::destroy_pools();

        dx_destroy(&g_gfx_queue);
        dx_destroy(&g_compute_queue);
        dx_destroy(&g_copy_queue);

        destroy(g_destruction_queue);
        dx_destroy(&g_allocator);

        dx_destroy(&g_device);
        dx_destroy(&g_adapter);
        dx_destroy(&g_factory);
    }

    u64 get_current_frame_idx()
    {
        return g_current_frame_idx;
    }

    u64 get_current_cpu_frame()
    {
        return g_current_cpu_frame;
    }

    void wait_for_gpu()
    {
        for (size_t i = 0; i < ARRAY_SIZE(g_command_pools); i++) {
            wait(g_command_pools[i].fence, g_command_pools[i].last_used_fence_value);
            cmd_utils::reset(g_command_pools[i]);
        }
    }

    void begin_upload()
    {
        g_upload_manager.begin_upload();
    }

    CmdReceipt end_upload()
    {
        return g_upload_manager.end_upload();
    }

    CommandContextHandle begin_frame()
    {
        reset_for_frame();

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

        cmd_utils::insert_resource_barrier(command_context, &barrier, 1);

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

        cmd_utils::insert_resource_barrier(command_context, &barrier, 1);
        cmd_utils::return_and_execute(&command_context, 1);

        present_frame();
    }

    void reset_for_frame()
    {
        g_current_frame_idx = g_swap_chain.swap_chain->GetCurrentBackBufferIndex();

        if (g_current_cpu_frame != 0) {
            // Wait for the GPU to catch up so we can freely reset our frame's command allocator
            const u64 gpu_lag = g_current_cpu_frame - g_current_gpu_frame;
            ASSERT(gpu_lag <= RENDER_LATENCY);

            if (gpu_lag == RENDER_LATENCY) {
                wait(g_frame_fence, g_current_gpu_frame + 1);
                ++g_current_gpu_frame;
            }

            for (size_t i = 0; i < ARRAY_SIZE(g_command_pools); i++) {
                cmd_utils::reset(g_command_pools[i]);
            }
        }

        // Process resources queued for destruction
        // TODO: I don't think this works properly at all atm
        //process_destruction_queue(g_destruction_queue);

        for (auto& descriptor_heap : g_descriptor_heaps) {
            descriptor_utils::process_destruction_queue(descriptor_heap, g_current_frame_idx);
        }
    }

    CmdReceipt present_frame()
    {
        // Present the frame.
        if (g_swap_chain.swap_chain != nullptr) {
            u32 sync_intervals = g_swap_chain.vsync ? 1 : 0;
            DXCall(g_swap_chain.swap_chain->Present(sync_intervals, g_swap_chain.vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING));
        }

        // Increment our cpu frame couter to indicate submission
        signal(g_frame_fence, g_gfx_queue, g_current_cpu_frame);
        return CmdReceipt{
            .queue_type = CommandQueueType::GRAPHICS,
            .fence_value = g_current_cpu_frame++ };
    }

    TextureHandle get_current_back_buffer_handle()
    {
        return dx12::get_current_back_buffer_handle(g_swap_chain, g_current_frame_idx);
    }
}
#endif // USE_D3D_RENDERER