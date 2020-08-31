#include "pch.h"
#include "renderer.h"
#include "gfx/public.h"

#if _DEBUG
#define USE_DEBUG_DEVICE 1
#define BREAK_ON_DX_ERROR (USE_DEBUG_DEVICE && 1)
#define USE_GPU_VALIDATION 1
#else
#define USE_DEBUG_DEVICE 0
#define BREAK_ON_DX_ERROR 0
#define USE_GPU_VALIDATION 0
#endif


namespace zec
{
    static void set_formats(dx12::SwapChain& swap_chain, const DXGI_FORMAT format)
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

    Renderer::~Renderer()
    {
        ASSERT(device_context.device == nullptr);
        ASSERT(device_context.adapter == nullptr);
        ASSERT(device_context.factory == nullptr);
    }

    void Renderer::init(const RendererDesc& renderer_desc)
    {
        // Factory, Adaptor and Device initialization
        {
            ASSERT(device_context.adapter == nullptr);
            ASSERT(device_context.factory == nullptr);
            ASSERT(device_context.device == nullptr);
            constexpr int ADAPTER_NUMBER = 0;

        #ifdef USE_DEBUG_DEVICE
            ID3D12Debug* debug_ptr;
            DXCall(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_ptr)));
            debug_ptr->EnableDebugLayer();

            UINT factory_flags = DXGI_CREATE_FACTORY_DEBUG;

        #ifdef USE_GPU_VALIDATION
            ID3D12Debug1* debug1;
            debug_ptr->QueryInterface(IID_PPV_ARGS(&debug1));
            debug1->SetEnableGPUBasedValidation(true);
            debug1->Release();
        #endif // USE_GPU_VALIDATION
            debug_ptr->Release();
        #endif // DEBUG

            DXCall(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&device_context.factory)));

            device_context.factory->EnumAdapters1(ADAPTER_NUMBER, &device_context.adapter);

            if (device_context.adapter == nullptr) {
                throw Exception(L"Unabled to located DXGI 1.4 adapter that supports D3D12.");
            }

            DXGI_ADAPTER_DESC1 desc{  };
            device_context.adapter->GetDesc1(&desc);
            write_log("Creating DX12 device on adapter '%ls'", desc.Description);

            DXCall(D3D12CreateDevice(device_context.adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_context.device)));

            D3D_FEATURE_LEVEL feature_levels_arr[4] = {
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_12_1,
            };
            D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels = { };
            feature_levels.NumFeatureLevels = ARRAY_SIZE(feature_levels_arr);
            feature_levels.pFeatureLevelsRequested = feature_levels_arr;
            DXCall(device_context.device->CheckFeatureSupport(
                D3D12_FEATURE_FEATURE_LEVELS,
                &feature_levels,
                sizeof(feature_levels)
            ));
            device_context.supported_feature_level = feature_levels.MaxSupportedFeatureLevel;

            D3D_FEATURE_LEVEL min_feature_level = D3D_FEATURE_LEVEL_12_0;
            if (device_context.supported_feature_level < min_feature_level) {
                std::wstring majorLevel = to_string<int>(min_feature_level >> 12);
                std::wstring minorLevel = to_string<int>((min_feature_level >> 8) & 0xF);
                throw Exception(L"The device doesn't support the minimum feature level required to run this sample (DX" + majorLevel + L"." + minorLevel + L")");
            }

        #ifdef USE_DEBUG_DEVICE
            ID3D12InfoQueue* infoQueue;
            DXCall(device_context.device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

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
            allocatorDesc.pDevice = device_context.device;
            allocatorDesc.pAdapter = device_context.adapter;

            DXCall(D3D12MA::CreateAllocator(&allocatorDesc, &allocator));
        }

        // Initialize command allocators and command list
        {
            // We need N allocators for N frames, since we cannot reset them and reclaim the associated memory
            // until the GPU is finished executing against them. So we have to use fences
            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                DXCall(device_context.device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_allocators[i])
                ));
            }

            // Command lists, however, can be reset as soon as we've submitted them.
            DXCall(device_context.device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocators[0], nullptr, IID_PPV_ARGS(&cmd_list)
            ));
            DXCall(cmd_list->Close());
            cmd_list->SetName(L"Graphics command list");

            // Initialize graphics queue
            D3D12_COMMAND_QUEUE_DESC queue_desc{ };
            queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            DXCall(device_context.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&gfx_queue)));
            gfx_queue->SetName(L"Graphics queue");

            // Set current frame index and reset command allocator and list appropriately
            ID3D12CommandAllocator* current_cmd_allocator = cmd_allocators[0];
            DXCall(current_cmd_allocator->Reset());
            DXCall(cmd_list->Reset(current_cmd_allocator, nullptr));
        }

        // RTV Descriptor Heap initialization
        dx12::DescriptorHeapDesc rtv_descriptor_heap_desc{ };
        rtv_descriptor_heap_desc.heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_descriptor_heap_desc.num_persistent = 256;
        rtv_descriptor_heap_desc.num_temporary = 0;
        rtv_descriptor_heap_desc.is_shader_visible = false;
        dx12::init(rtv_descriptor_heap, rtv_descriptor_heap_desc, device_context);

        // Descriptor Heap for depth texture views
        dx12::DescriptorHeapDesc dsv_descriptor_heap_desc{ };
        rtv_descriptor_heap_desc.heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        rtv_descriptor_heap_desc.num_persistent = 256;
        rtv_descriptor_heap_desc.num_temporary = 0;
        rtv_descriptor_heap_desc.is_shader_visible = false;
        dx12::init(dsv_descriptor_heap, rtv_descriptor_heap_desc, device_context);

        // Descriptor Heap for SRV, CBV and UAV resources
        dx12::DescriptorHeapDesc cbv_descriptor_heap_desc{ };
        cbv_descriptor_heap_desc.heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbv_descriptor_heap_desc.num_persistent = 1024;
        cbv_descriptor_heap_desc.num_temporary = 1024;
        cbv_descriptor_heap_desc.is_shader_visible = true;
        dx12::init(srv_descriptor_heap, cbv_descriptor_heap_desc, device_context);

        // Swapchain initializaiton
        {
            ASSERT(swap_chain.swap_chain == nullptr);

            dx12::SwapChainDesc swap_chain_desc{ };
            swap_chain_desc.width = renderer_desc.width;
            swap_chain_desc.height = renderer_desc.height;
            swap_chain_desc.fullscreen = renderer_desc.fullscreen;
            swap_chain_desc.vsync = renderer_desc.vsync;
            swap_chain_desc.output_window = renderer_desc.window;

            device_context.adapter->EnumOutputs(0, &swap_chain.output);

            swap_chain.width = swap_chain_desc.width;
            swap_chain.height = swap_chain_desc.height;
            swap_chain.fullscreen = swap_chain_desc.fullscreen;
            swap_chain.vsync = swap_chain_desc.vsync;

            swap_chain.format = swap_chain_desc.format;
            if (swap_chain_desc.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
                swap_chain.non_sRGB_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            else if (swap_chain_desc.format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                swap_chain.non_sRGB_format = DXGI_FORMAT_B8G8R8A8_UNORM;
            }
            else {
                swap_chain.non_sRGB_format = swap_chain.format;
            }

            DXGI_SWAP_CHAIN_DESC d3d_swap_chain_desc{  };
            d3d_swap_chain_desc.BufferCount = NUM_BACK_BUFFERS;
            d3d_swap_chain_desc.BufferDesc.Width = swap_chain.width;
            d3d_swap_chain_desc.BufferDesc.Height = swap_chain.height;
            d3d_swap_chain_desc.BufferDesc.Format = swap_chain.format;
            d3d_swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            d3d_swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            d3d_swap_chain_desc.SampleDesc.Count = 1;
            d3d_swap_chain_desc.OutputWindow = swap_chain_desc.output_window;
            d3d_swap_chain_desc.Windowed = TRUE;
            d3d_swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

            IDXGISwapChain* d3d_swap_chain = nullptr;
            DXCall(device_context.factory->CreateSwapChain(gfx_queue, &d3d_swap_chain_desc, &d3d_swap_chain));
            DXCall(d3d_swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain.swap_chain)));
            d3d_swap_chain->Release();

            swap_chain.back_buffer_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();
            swap_chain.waitable_object = swap_chain.swap_chain->GetFrameLatencyWaitableObject();

            recreate_swap_chain_rtv_descriptors();
        }
        fence_manager.init(device_context.device, &destruction_queue);
        frame_fence = fence_manager.create_fence(0);
    }

    void Renderer::destroy()
    {
        write_log("Destroying renderer");
        wait(frame_fence, current_cpu_frame);

        // Swap Chain Destruction
        {
            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                // Do not need to destroy the resource ourselves, since it's managed by the swap chain (??)
                swap_chain.back_buffers[i].resource->Release();
                free_persistent_alloc(rtv_descriptor_heap, swap_chain.back_buffers[i].rtv);
            }

            swap_chain.swap_chain->Release();
            swap_chain.output->Release();
        }

        dx12::destroy(rtv_descriptor_heap);
        fence_manager.destroy();

        cmd_list->Release();
        for (u64 i = 0; i < RENDER_LATENCY; i++) {
            cmd_allocators[i]->Release();
        }
        gfx_queue->Release();

        destruction_queue.destroy();

        allocator->Release();

        device_context.device->Release();
        device_context.adapter->Release();
        device_context.factory->Release();
        device_context.device = nullptr;
        device_context.adapter = nullptr;
        device_context.factory = nullptr;
    }

    void Renderer::begin_frame()
    {
        if (current_cpu_frame != 0) {
            // Wait for the GPU to catch up so we can freely reset our frame's command allocator
            const u64 gpu_lag = current_cpu_frame - current_gpu_frame;
            ASSERT(gpu_lag <= RENDER_LATENCY);
            if (gpu_lag == RENDER_LATENCY) {
                wait(frame_fence, current_gpu_frame + 1);
                ++current_gpu_frame;
            }

            current_frame_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();

            DXCall(cmd_allocators[current_frame_idx]->Reset());
            DXCall(cmd_list->Reset(cmd_allocators[current_frame_idx], nullptr));

            // Reset Heaps

            // Process resources queued for destruction
            destruction_queue.process_queue();

            // Process shader resource views queued for creation
        }
    }

    void Renderer::end_frame()
    {
        DXCall(cmd_list->Close());

        // endframe uploads

        ID3D12CommandList* command_lists[] = { cmd_list };
        gfx_queue->ExecuteCommandLists(ARRAY_SIZE(command_lists), command_lists);

        // Present the frame.
        if (swap_chain.swap_chain != nullptr) {
            u32 sync_intervals = swap_chain.vsync ? 1 : 0;
            DXCall(swap_chain.swap_chain->Present(sync_intervals, swap_chain.vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING));
        }

        // Increment our cpu frame couter to indicate submission
        ++current_cpu_frame;
        signal(frame_fence, gfx_queue, current_cpu_frame);
    }

    void Renderer::prepare_full_screen_settings()
    {
        ASSERT(swap_chain.output != nullptr);

        DXGI_MODE_DESC desired_mode{};
        desired_mode.Format = swap_chain.non_sRGB_format;
        desired_mode.Width = swap_chain.width;
        desired_mode.Height = swap_chain.height;
        desired_mode.RefreshRate.Numerator = 0;
        desired_mode.RefreshRate.Denominator = 0;
        desired_mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        desired_mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

        DXGI_MODE_DESC closest_match{};
        DXCall(swap_chain.output->FindClosestMatchingMode(&desired_mode, &closest_match, device_context.device));

        swap_chain.width = closest_match.Width;
        swap_chain.height = closest_match.Height;
        swap_chain.refresh_rate = closest_match.RefreshRate;
    }

    void Renderer::reset()
    {
        reset_swap_chain();
        destruction_queue.process_queue();
    }

    void Renderer::reset_swap_chain()
    {
        ASSERT(swap_chain.swap_chain != nullptr);

        if (swap_chain.output == nullptr) {
            swap_chain.fullscreen = false;
        }

        for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
            swap_chain.back_buffers[i].resource->Release();
            free_persistent_alloc(rtv_descriptor_heap, swap_chain.back_buffers[i].rtv);
        }

        set_formats(swap_chain, swap_chain.format);

        if (swap_chain.fullscreen) {
            prepare_full_screen_settings();
        }
        else {
            swap_chain.refresh_rate.Numerator = 60;
            swap_chain.refresh_rate.Denominator = 1;
        }

        DXCall(swap_chain.swap_chain->SetFullscreenState(swap_chain.fullscreen, swap_chain.output));
        // TODO Backbuffers
        for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
            DXCall(swap_chain.swap_chain->ResizeBuffers(
                NUM_BACK_BUFFERS,
                swap_chain.width,
                swap_chain.height, swap_chain.non_sRGB_format,
                DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
        }

        if (swap_chain.fullscreen) {
            DXGI_MODE_DESC mode;
            mode.Format = swap_chain.non_sRGB_format;
            mode.Width = swap_chain.width;
            mode.Height = swap_chain.height;
            mode.RefreshRate.Numerator = 0;
            mode.RefreshRate.Denominator = 0;
            mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
            mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            DXCall(swap_chain.swap_chain->ResizeTarget(&mode));
        }

        recreate_swap_chain_rtv_descriptors();
    }

    void Renderer::recreate_swap_chain_rtv_descriptors()
    {
        // Re-create an RTV for each back buffer
        for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
            // RTV Descriptor heap only has one heap -> only one handle is issued
            ASSERT(rtv_descriptor_heap.num_heaps == 1);
            swap_chain.back_buffers[i].rtv = allocate_persistent_descriptor(rtv_descriptor_heap).handles[0];
            DXCall(swap_chain.swap_chain->GetBuffer(UINT(i), IID_PPV_ARGS(&swap_chain.back_buffers[i].resource)));

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Format = swap_chain.format;
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;
            device_context.device->CreateRenderTargetView(swap_chain.back_buffers[i].resource, &rtvDesc, swap_chain.back_buffers[i].rtv);

            swap_chain.back_buffers[i].resource->SetName(make_string(L"Back Buffer %llu", i).c_str());

            // Copy properties from swap chain to backbuffer textures, in case we need em
            swap_chain.back_buffers[i].width = swap_chain.width;
            swap_chain.back_buffers[i].height = swap_chain.height;
            swap_chain.back_buffers[i].depth = 1;
            swap_chain.back_buffers[i].array_size = 1;
            swap_chain.back_buffers[i].format = swap_chain.format;
            swap_chain.back_buffers[i].num_mips = 1;

        }
        swap_chain.back_buffer_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();
    }
}