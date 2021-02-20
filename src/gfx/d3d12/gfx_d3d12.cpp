#pragma once
#include "pch.h"
#include "core/zec_math.h"
#include "gfx/public_resources.h"
#include "gfx/gfx.h"

#include "shader_utils.h"
#include "render_context.h"
#include "upload_manager.h"
#include "meshes.h"

#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include "DirectXTex.h"
#include <filesystem>

#if _DEBUG
#define USE_DEBUG_DEVICE 1
#define BREAK_ON_DX_ERROR (USE_DEBUG_DEVICE && 1)
#define USE_GPU_VALIDATION 1
#endif

namespace zec::gfx
{
    using namespace dx12;
    static RenderContext g_context{};

    namespace dx12
    {
        // Used in the profiler, don't use it elsewhere I guess
        ID3D12GraphicsCommandList* get_command_list(const CommandContextHandle cmd_ctx)
        {
            auto& pool = g_context.command_pools[cmd_utils::get_command_queue_type(cmd_ctx)];
            return pool.get_graphics_command_list(cmd_ctx);
        };

        RenderContext& get_render_context()
        {
            return g_context;
        }

        void register_startup_callback(RenderContext::StartupCallback callback)
        {
            ASSERT(!g_context.is_initialized);
            g_context.statup_callbacks.push_back(callback);
        }

        void register_destruction_callback(RenderContext::DestructionCallback callback)
        {
            g_context.destruction_callbacks.push_back(callback);
        }

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

        void recreate_swap_chain_rtv_descriptors(SwapChain& swap_chain)
        {
            // Re-create an RTV for each back buffer
            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                TextureHandle texture_handle = swap_chain.back_buffers[i];
                ID3D12Resource* resource = g_context.textures.resources[texture_handle];
                if (resource != nullptr) resource->Release();
                g_context.textures.resources[texture_handle] = nullptr;

                const auto rtv_handle = g_context.textures.rtvs[texture_handle];
                auto& descriptor_heap = g_context.descriptor_heaps[HeapTypes::RTV];
                descriptor_heap.free_descriptors(rtv_handle);

                DXCall(swap_chain.swap_chain->GetBuffer(UINT(i), IID_PPV_ARGS(&resource)));
                g_context.textures.resources[texture_handle] = resource;

                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
                DescriptorRangeHandle rtv = descriptor_heap.allocate_descriptors(1, &cpu_handle);
                g_context.textures.rtvs[texture_handle] = rtv;

                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Format = swap_chain.format;
                rtvDesc.Texture2D.MipSlice = 0;
                rtvDesc.Texture2D.PlaneSlice = 0;
                g_context.device->CreateRenderTargetView(resource, &rtvDesc, cpu_handle);
                // TODO: Store MSAA info?

                resource->SetName(make_string(L"Back Buffer %llu", i).c_str());

                // Copy properties from swap chain to backbuffer textures, in case we need em
                TextureInfo texture_info = {
                    .width = swap_chain.width,
                    .height = swap_chain.height,
                    .depth = 1,
                    .num_mips = 1,
                    .array_size = 1,
                    .format = from_d3d_format(swap_chain.format),
                };
                g_context.textures.infos[texture_handle] = texture_info;
            }
            swap_chain.back_buffer_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();
        }

        void reset_swap_chain(SwapChain& swap_chain)
        {
            ASSERT(swap_chain.swap_chain != nullptr);

            if (swap_chain.output == nullptr) {
                swap_chain.fullscreen = false;
            }

            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                // Release resources
                TextureHandle texture_handle = swap_chain.back_buffers[i];
                ID3D12Resource* resource = g_context.textures.resources[texture_handle];
                if (resource != nullptr) resource->Release();
                g_context.textures.resources[texture_handle] = nullptr;
                // Free descriptors
                DescriptorHeap& descriptor_heap = g_context.descriptor_heaps[HeapTypes::RTV];
                const auto rtv_handle = g_context.textures.rtvs[texture_handle];
                descriptor_heap.free_descriptors(rtv_handle);
                g_context.textures.rtvs[texture_handle] = INVALID_HANDLE;
            }

            /*set_formats(swap_chain, swap_chain.format);

            if (swap_chain.fullscreen) {
                prepare_full_screen_settings();
            }

            DXCall(swap_chain.swap_chain->SetFullscreenState(swap_chain.fullscreen, nullptr));*/

            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                DXCall(swap_chain.swap_chain->ResizeBuffers(
                    NUM_BACK_BUFFERS,
                    swap_chain.width,
                    swap_chain.height,
                    swap_chain.non_sRGB_format,
                    DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
            }

            /*if (swap_chain.fullscreen) {
                DXGI_MODE_DESC mode;
                mode.Format = swap_chain.non_sRGB_format;
                mode.Width = swap_chain.width;
                mode.Height = swap_chain.height;
                mode.RefreshRate.Numerator = 0;
                mode.RefreshRate.Denominator = 0;
                mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
                mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
                DXCall(swap_chain.swap_chain->ResizeTarget(&mode));
            }*/

            recreate_swap_chain_rtv_descriptors(swap_chain);
        }
    }

    void init_renderer(const RendererDesc& renderer_desc)
    {
        ASSERT(!g_context.is_initialized);
        ASSERT(g_context.adapter == nullptr);
        ASSERT(g_context.factory == nullptr);
        ASSERT(g_context.device == nullptr);

        g_context.config_state = {
            .width = renderer_desc.width,
            .height = renderer_desc.height,
            .fullscreen = renderer_desc.fullscreen,
            .vsync = renderer_desc.vsync,
            .msaa = renderer_desc.msaa,
        };

        {
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

            DXCall(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&g_context.factory)));

            {
                UINT i = 0;
                IDXGIAdapter1* p_adapter;
                u32 best_video_memory = 0;
                u32 best_adapter_idx = UINT32_MAX; // Which adapter has most video memory
                while (g_context.factory->EnumAdapters1(i, &p_adapter) != DXGI_ERROR_NOT_FOUND) {
                    DXGI_ADAPTER_DESC1 desc{};
                    p_adapter->GetDesc1(&desc);
                    write_log(L"Found adapter %s with %u dedicated video memory.", desc.Description, desc.DedicatedVideoMemory);
                    if (desc.DedicatedVideoMemory > best_video_memory) {
                        best_adapter_idx = i;
                    }
                    ++i;
                    p_adapter->Release();
                }

                if (best_adapter_idx != UINT32_MAX) {
                    g_context.factory->EnumAdapters1(best_adapter_idx, &g_context.adapter);
                    DXGI_ADAPTER_DESC1 desc{  };
                    g_context.adapter->GetDesc1(&desc);
                    write_log("Creating DX12 device on adapter '%ls'", desc.Description);
                }
                else {
                    throw Exception(L"Unabled to located DXGI 1.4 adapter that supports D3D12.");
                }
            }

            DXCall(D3D12CreateDevice(g_context.adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_context.device)));

            D3D_FEATURE_LEVEL feature_levels_arr[4] = {
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_12_1,
            };
            D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels = { };
            feature_levels.NumFeatureLevels = ARRAY_SIZE(feature_levels_arr);
            feature_levels.pFeatureLevelsRequested = feature_levels_arr;
            DXCall(g_context.device->CheckFeatureSupport(
                D3D12_FEATURE_FEATURE_LEVELS,
                &feature_levels,
                sizeof(feature_levels)
            ));
            g_context.supported_feature_level = feature_levels.MaxSupportedFeatureLevel;

            D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
            DXCall(g_context.device->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS,
                &options,
                sizeof(options)
            ));

            D3D_FEATURE_LEVEL min_feature_level = D3D_FEATURE_LEVEL_11_0;
            if (g_context.supported_feature_level < min_feature_level) {
                std::wstring majorLevel = to_string<int>(min_feature_level >> 12);
                std::wstring minorLevel = to_string<int>((min_feature_level >> 8) & 0xF);
                throw Exception(L"The g_context.device doesn't support the minimum feature level required to run this sample (DX" + majorLevel + L"." + minorLevel + L")");
            }

        #ifdef USE_DEBUG_DEVICE
            ID3D12InfoQueue* infoQueue;
            DXCall(g_context.device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

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
            allocatorDesc.pDevice = g_context.device;
            allocatorDesc.pAdapter = g_context.adapter;

            DXCall(D3D12MA::CreateAllocator(&allocatorDesc, &g_context.allocator));
        }

        // Initialize command queues and command context pools
        {
            constexpr CommandQueueType queue_types[] = {
                CommandQueueType::GRAPHICS,
                CommandQueueType::ASYNC_COMPUTE,
                CommandQueueType::COPY,
            };
            for (const CommandQueueType queue_type : queue_types) {
                // Initialize Queue
                g_context.command_queues[queue_type].initialize(queue_type, g_context.device);

                // Initialize Pool

                g_context.command_pools[queue_type].initialize(queue_type, g_context.device);

            }

        }

        // Initialize descriptor heaps
        {
            // Create a heap of a specific type
            constexpr DescriptorHeapDesc descs[] = {
                {
                .size = DescriptorHeapDesc::MAX_SIZE,
                .type = HeapTypes::SRV,
                },
                {
                .size = 128,
                .type = HeapTypes::RTV,
                },
                {
                .size = 128,
                .type = HeapTypes::DSV
                },
                {
                .size = 128,
                .type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
                },
            };

            for (const DescriptorHeapDesc& desc : descs) {
                DescriptorHeap& heap = g_context.descriptor_heaps[u64(desc.type)];
                heap.num_allocated = 0;
                heap.capacity = desc.size;
                heap.is_shader_visible = desc.type == HeapTypes::SRV;
                u32 total_num_descriptors = heap.capacity;
                D3D12_DESCRIPTOR_HEAP_DESC d3d_desc{ };
                d3d_desc.NumDescriptors = total_num_descriptors;
                d3d_desc.Type = desc.type;
                d3d_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

                if (heap.is_shader_visible) {
                    d3d_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                }

                DXCall(g_context.device->CreateDescriptorHeap(
                    &d3d_desc, IID_PPV_ARGS(&heap.heap)
                ));

                heap.cpu_start = heap.heap->GetCPUDescriptorHandleForHeapStart();

                if (heap.is_shader_visible) {
                    heap.gpu_start = heap.heap->GetGPUDescriptorHandleForHeapStart();
                }

                heap.descriptor_size = g_context.device->GetDescriptorHandleIncrementSize(desc.type);

            }
        }


        OPTICK_GPU_INIT_D3D12(g_context.device, &g_context.command_queues[CommandQueueType::GRAPHICS].queue, 1);

        // Swapchain initialization
        {
            ASSERT(g_context.swap_chain.swap_chain == nullptr);

            for (size_t i = 0; i < NUM_BACK_BUFFERS; i++) {
                g_context.swap_chain.back_buffers[i] = g_context.textures.store_texture(Texture{});
            }

            SwapChainDesc swap_chain_desc{ };
            swap_chain_desc.width = renderer_desc.width;
            swap_chain_desc.height = renderer_desc.height;
            swap_chain_desc.fullscreen = renderer_desc.fullscreen;
            swap_chain_desc.vsync = renderer_desc.vsync;
            swap_chain_desc.output_window = renderer_desc.window;
            swap_chain_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

            g_context.adapter->EnumOutputs(0, &g_context.swap_chain.output);

            g_context.swap_chain.width = swap_chain_desc.width;
            g_context.swap_chain.height = swap_chain_desc.height;
            g_context.swap_chain.fullscreen = swap_chain_desc.fullscreen;
            g_context.swap_chain.vsync = swap_chain_desc.vsync;

            set_formats(g_context.swap_chain, swap_chain_desc.format);

            DXGI_SWAP_CHAIN_DESC d3d_swap_chain_desc = {  };
            d3d_swap_chain_desc.BufferCount = NUM_BACK_BUFFERS;
            d3d_swap_chain_desc.BufferDesc.Width = g_context.swap_chain.width;
            d3d_swap_chain_desc.BufferDesc.Height = g_context.swap_chain.height;
            d3d_swap_chain_desc.BufferDesc.Format = g_context.swap_chain.non_sRGB_format;
            d3d_swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            d3d_swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            d3d_swap_chain_desc.SampleDesc.Count = 1;
            d3d_swap_chain_desc.OutputWindow = swap_chain_desc.output_window;
            d3d_swap_chain_desc.Windowed = TRUE;
            d3d_swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

            IDXGISwapChain* d3d_swap_chain = nullptr;
            ID3D12CommandQueue* queue = g_context.command_queues[CommandQueueType::GRAPHICS].queue;
            DXCall(g_context.factory->CreateSwapChain(queue, &d3d_swap_chain_desc, &d3d_swap_chain));
            DXCall(d3d_swap_chain->QueryInterface(IID_PPV_ARGS(&g_context.swap_chain.swap_chain)));
            d3d_swap_chain->Release();

            g_context.swap_chain.back_buffer_idx = g_context.swap_chain.swap_chain->GetCurrentBackBufferIndex();
            g_context.swap_chain.waitable_object = g_context.swap_chain.swap_chain->GetFrameLatencyWaitableObject();

            recreate_swap_chain_rtv_descriptors(g_context.swap_chain);
        }
        g_context.frame_fence = create_fence(g_context.device, 0);

        g_context.is_initialized = true;
        for (auto callback : g_context.statup_callbacks) {
            callback(&g_context);
        }
    };

    void destroy_renderer()
    {
        for (auto callback : g_context.destruction_callbacks) {
            callback();
        }

        // Chain Destruction
        destroy(g_context.swap_chain);

        for (ID3D12RootSignature* root_signature : g_context.root_signatures) {
            root_signature->Release();
        }
        g_context.root_signatures.ptrs.empty();

        for (ID3D12PipelineState* pipeline : g_context.pipelines) {
            pipeline->Release();
        }
        g_context.pipelines.ptrs.empty();

        g_context.upload_store.destroy();

        // Destroy textures
        {
            g_context.textures.destroy(
                [](ID3D12Resource* resource, D3D12MA::Allocation* allocation) {
                    resource->Release();
                    if (allocation) allocation->Release();
                },
                [](D3D12_DESCRIPTOR_HEAP_TYPE heap_type, DescriptorRangeHandle handle) {
                    g_context.descriptor_heaps[heap_type].free_descriptors(handle);
                });
        }

        destroy(g_context.destruction_queue, g_context.current_frame_idx, g_context.frame_fence);

        for (auto& heap : g_context.descriptor_heaps) {
            ASSERT(heap.num_free == heap.num_allocated);
            heap.heap->Release();
        }


        for (auto& pool : g_context.command_pools) {
            pool.destroy();
        }

        for (auto& queue : g_context.command_queues) {
            queue.destroy();
        }

        g_context.destruction_queue.flush();
        g_context.async_destruction_queue.flush();
        dx_destroy(&g_context.allocator);

    #ifdef USE_DEBUG_DEVICE 
        ID3D12DebugDevice* debug_device = nullptr;
        DXCall(g_context.device->QueryInterface(&debug_device));
        if (debug_device) {
            DXCall(debug_device->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL));
            debug_device->Release();
        }
    #endif // USE_DEBUG_DEVICE

        dx_destroy(&g_context.device);
        dx_destroy(&g_context.adapter);
        dx_destroy(&g_context.factory);
    };

    RenderConfigState get_config_state()
    {
        return g_context.config_state;
    };

    u64 get_current_frame_idx()
    {
        return g_context.current_frame_idx;
    };

    void flush_gpu()
    {
        for (size_t i = 0; i < size_t(CommandQueueType::NUM_COMMAND_CONTEXT_POOLS); i++) {
            auto& queue = g_context.command_queues[i];
            queue.flush();
            g_context.command_pools[i].reset(queue.last_used_fence_value);
        }

        u64 last_submitted_frame = g_context.current_cpu_frame - 1;
        if (last_submitted_frame > g_context.current_gpu_frame) {
            wait(g_context.frame_fence, last_submitted_frame);
            g_context.current_gpu_frame = last_submitted_frame;
        }
    }

    CommandContextHandle begin_frame()
    {
        CommandContextHandle cmd_ctx = gfx::cmd::provision(CommandQueueType::GRAPHICS);
        const TextureHandle backbuffer = get_current_back_buffer_handle();

        D3D12_RESOURCE_BARRIER barrier{  };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = g_context.textures.resources[backbuffer];
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        ID3D12GraphicsCommandList* cmd_list = get_command_list(cmd_ctx);
        cmd_list->ResourceBarrier(1, &barrier);

        return cmd_ctx;
    };

    void end_frame(CommandContextHandle cmd_ctx)
    {
        const TextureHandle backbuffer = get_current_back_buffer_handle();

        D3D12_RESOURCE_BARRIER barrier{  };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = g_context.textures.resources[backbuffer];
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        ID3D12GraphicsCommandList* cmd_list = get_command_list(cmd_ctx);
        cmd_list->ResourceBarrier(1, &barrier);

        cmd::return_and_execute(&cmd_ctx, 1);
    };

    void reset_for_frame()
    {
        g_context.current_frame_idx = g_context.swap_chain.swap_chain->GetCurrentBackBufferIndex();
        u64 queue_completed_fence_values[size_t(CommandQueueType::NUM_COMMAND_CONTEXT_POOLS)] = { 0 };
        if (g_context.current_cpu_frame != 0) {
            // Wait for the GPU to catch up so we can freely reset our frame's command allocator
            const u64 gpu_lag = g_context.current_cpu_frame - g_context.current_gpu_frame;
            ASSERT(gpu_lag <= RENDER_LATENCY);

            if (gpu_lag == RENDER_LATENCY) {
                wait(g_context.frame_fence, g_context.current_gpu_frame + 1);
                ++g_context.current_gpu_frame;
            }

            for (size_t i = 0; i < g_context.command_pools.size; i++) {
                auto& queue = g_context.command_queues[i];
                auto& command_pool = g_context.command_pools[i];
                queue_completed_fence_values[i] = get_completed_value(queue.fence);
                command_pool.reset(queue_completed_fence_values[i]);
            }

        }

        // Process resources queued for destruction
        process_destruction_queue(g_context.destruction_queue, g_context.current_frame_idx);

        process_destruction_queue(g_context.async_destruction_queue, queue_completed_fence_values);
    }

    CmdReceipt present_frame()
    {
        // Present the frame.
        SwapChain& swap_chain = g_context.swap_chain;
        if (swap_chain.swap_chain != nullptr) {
            u32 sync_intervals = swap_chain.vsync ? 1 : 0;

            OPTICK_GPU_FLIP(swap_chain.swap_chain);
            DXCall(swap_chain.swap_chain->Present(sync_intervals, swap_chain.vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING));
        }

        // Increment our cpu frame couter to indicate submission
        ID3D12CommandQueue* queue = g_context.command_queues[CommandQueueType::GRAPHICS].queue;
        signal(g_context.frame_fence, queue, g_context.current_cpu_frame);
        return CmdReceipt{
            .queue_type = CommandQueueType::GRAPHICS,
            .fence_value = g_context.current_cpu_frame++ };
    }

    TextureHandle get_current_back_buffer_handle()
    {
        return g_context.swap_chain.back_buffers[g_context.current_frame_idx];
    };

    void on_window_resize(u32 width, u32 height) { };

    namespace pipelines
    {
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

            // Tables -- Note that we actually map them to ranges
            if (desc.num_resource_tables > 0) {
                ASSERT(desc.num_resource_tables < ResourceLayoutDesc::MAX_ENTRIES);

                D3D12_DESCRIPTOR_RANGE srv_ranges[ResourceLayoutDesc::MAX_ENTRIES] = {};
                D3D12_DESCRIPTOR_RANGE uav_ranges[ResourceLayoutDesc::MAX_ENTRIES] = {};
                u32 srv_count = 0;
                u32 uav_count = 0;

                D3D12_ROOT_DESCRIPTOR_TABLE srv_table = {};
                D3D12_ROOT_DESCRIPTOR_TABLE uav_table = {};

                for (u32 i = 0; i < desc.num_resource_tables; i++) {
                    const auto& table_desc = desc.tables[i];
                    ASSERT(table_desc.usage != ResourceAccess::UNUSED);

                    auto range_type = table_desc.usage == ResourceAccess::READ ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                    // Since we only support bindless, they all start at a base shader register of zero and are in a different space.
                    if (table_desc.usage == ResourceAccess::READ) {

                        srv_ranges[srv_count++] = {
                            .RangeType = range_type,
                            .NumDescriptors = table_desc.count,
                            .BaseShaderRegister = 0,
                            .RegisterSpace = i + 1,
                            .OffsetInDescriptorsFromTableStart = 0
                        };
                    }
                    else {
                        uav_ranges[uav_count++] = {
                            .RangeType = range_type,
                            .NumDescriptors = table_desc.count,
                            .BaseShaderRegister = 0,
                            .RegisterSpace = i + 1,
                            .OffsetInDescriptorsFromTableStart = 0
                        };
                    }
                }

                if (srv_count > 0) {
                    for (size_t srv_table_idx = 0; srv_table_idx < srv_count; srv_table_idx++) {
                        D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count++];
                        parameter.ShaderVisibility = to_d3d_visibility(ShaderVisibility::ALL);
                        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                        parameter.DescriptorTable = {
                            .NumDescriptorRanges = 1,
                            .pDescriptorRanges = &srv_ranges[srv_table_idx],
                        };
                        remaining_dwords--;
                    }
                }

                if (uav_count > 0) {
                    for (size_t uav_table_idx = 0; uav_table_idx < uav_count; uav_table_idx++) {
                        D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count++];
                        parameter.ShaderVisibility = to_d3d_visibility(ShaderVisibility::ALL);
                        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                        parameter.DescriptorTable = {
                            .NumDescriptorRanges = 1,
                            .pDescriptorRanges = &uav_ranges[uav_table_idx],
                        };
                        remaining_dwords--;
                    }
                }
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
            HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

            if (error != nullptr) {
                print_blob(error);
                error->Release();
                throw std::runtime_error("Failed to create root signature");
            }
            DXCall(hr);

            hr = g_context.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature));

            if (error != nullptr) {
                print_blob(error);
                error->Release();
                throw std::runtime_error("Failed to create root signature");
            }
            DXCall(hr);
            signature != nullptr && signature->Release();

            return g_context.root_signatures.push_back(root_signature);

        };

        PipelineStateHandle  create_pipeline_state_object(const PipelineStateObjectDesc& desc)
        {
            ASSERT(is_valid(desc.resource_layout));

            ID3D12RootSignature* root_signature = g_context.root_signatures[desc.resource_layout];
            ID3D12PipelineState* pipeline = nullptr;

            if (desc.used_stages & PIPELINE_STAGE_COMPUTE) {
                ASSERT(desc.used_stages == PIPELINE_STAGE_COMPUTE);

                IDxcBlob* compute_shader = shader_utils::compile_shader(desc.shader_file_path.c_str(), PIPELINE_STAGE_COMPUTE);

                D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
                pso_desc.CS.BytecodeLength = compute_shader->GetBufferSize();
                pso_desc.CS.pShaderBytecode = compute_shader->GetBufferPointer();
                pso_desc.pRootSignature = root_signature;
                DXCall(g_context.device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));
                compute_shader->Release();

                return { g_context.pipelines.push_back(pipeline) };
            }

            // Create the input assembly desc
            std::string semantic_names[MAX_NUM_MESH_VERTEX_BUFFERS];
            D3D12_INPUT_ELEMENT_DESC d3d_elements[MAX_NUM_MESH_VERTEX_BUFFERS] = {};
            u32 num_input_elements = 0;
            {
                for (size_t i = 0; i < MAX_NUM_MESH_VERTEX_BUFFERS; i++) {
                    D3D12_INPUT_ELEMENT_DESC& d3d_desc = d3d_elements[i];
                    const auto& input_entry = desc.input_assembly_desc.elements[i];
                    if (input_entry.attribute_type == MESH_ATTRIBUTE_INVALID) {
                        break;
                    }

                    switch (input_entry.attribute_type) {
                    case MESH_ATTRIBUTE_POSITION:
                        semantic_names[i] = "POSITION";
                        break;
                    case MESH_ATTRIBUTE_NORMAL:
                        semantic_names[i] = "NORMAL";
                        break;
                    case MESH_ATTRIBUTE_TEXCOORD:
                        semantic_names[i] = "TEXCOORD";
                        break;
                    case MESH_ATTRIBUTE_BLENDINDICES:
                        semantic_names[i] = "BLENDINDICES";
                        break;
                    case MESH_ATTRIBUTE_BLENDWEIGHTS:
                        semantic_names[i] = "BLENDWEIGHT";
                        break;
                    case MESH_ATTRIBUTE_COLOR:
                        semantic_names[i] = "COLOR";
                        break;
                    default:
                        throw std::runtime_error("Unsupported mesh attribute type!");
                    }

                    d3d_desc.SemanticName = semantic_names[i].c_str();
                    d3d_desc.SemanticIndex = input_entry.semantic_index;
                    d3d_desc.Format = to_d3d_format(input_entry.format);
                    d3d_desc.InputSlot = input_entry.input_slot;
                    d3d_desc.AlignedByteOffset = input_entry.aligned_byte_offset;
                    d3d_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                    d3d_desc.InstanceDataStepRate = 0;
                    num_input_elements++;
                };
            }

            // Create a pipeline state object description, then create the object
            D3D12_RASTERIZER_DESC rasterizer_desc = to_d3d_rasterizer_desc(desc.raster_state_desc);

            const auto& blend_desc = desc.blend_state;
            D3D12_BLEND_DESC d3d_blend_desc = { };
            d3d_blend_desc.AlphaToCoverageEnable = blend_desc.alpha_to_coverage_enable;
            d3d_blend_desc.IndependentBlendEnable = blend_desc.independent_blend_enable;
            for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
                d3d_blend_desc.RenderTarget[i] = to_d3d_blend_desc(desc.blend_state.render_target_blend_descs[i]);
            }

            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
            pso_desc.InputLayout = { d3d_elements, num_input_elements };
            pso_desc.pRootSignature = root_signature;
            pso_desc.RasterizerState = rasterizer_desc;
            pso_desc.BlendState = d3d_blend_desc;
            pso_desc.DepthStencilState = to_d3d_depth_stencil_desc(desc.depth_stencil_state);
            pso_desc.SampleMask = UINT_MAX;
            pso_desc.PrimitiveTopologyType = to_d3d_topology_type(desc.topology_type);
            pso_desc.SampleDesc.Count = 1;
            if (desc.depth_buffer_format != BufferFormat::INVALID) {
                pso_desc.DSVFormat = to_d3d_format(desc.depth_buffer_format);
            }
            for (size_t i = 0; i < ARRAY_SIZE(desc.rtv_formats); i++) {
                if (desc.rtv_formats[i] == BufferFormat::INVALID) break;
                pso_desc.RTVFormats[i] = to_d3d_format(desc.rtv_formats[i]);
                pso_desc.NumRenderTargets = i + 1;
            }

            IDxcBlob* vertex_shader = nullptr;
            IDxcBlob* pixel_shader = nullptr;
            if (desc.used_stages & PIPELINE_STAGE_VERTEX) {
                vertex_shader = shader_utils::compile_shader(desc.shader_file_path.c_str(), PIPELINE_STAGE_VERTEX);
                pso_desc.VS.BytecodeLength = vertex_shader->GetBufferSize();
                pso_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
            }
            if (desc.used_stages & PIPELINE_STAGE_PIXEL) {
                pixel_shader = shader_utils::compile_shader(desc.shader_file_path.c_str(), PIPELINE_STAGE_PIXEL);
                pso_desc.PS.BytecodeLength = pixel_shader->GetBufferSize();
                pso_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
            }

            DXCall(g_context.device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));

            if (vertex_shader != nullptr) vertex_shader->Release();
            if (pixel_shader != nullptr) pixel_shader->Release();

            return { g_context.pipelines.push_back(pipeline) };

        };
    }

    namespace buffers
    {
        BufferHandle create(BufferDesc desc)
        {
            ASSERT(desc.byte_size != 0);
            ASSERT(desc.usage != u16(RESOURCE_USAGE_UNUSED));
            ASSERT(desc.type != BufferType::RAW || desc.stride == sizeof(u32));

            u16 vertex_or_index = u16(RESOURCE_USAGE_VERTEX) | u16(RESOURCE_USAGE_INDEX);
            if (desc.usage & vertex_or_index) {
                // ASSERT it's only one or the other
                ASSERT((desc.usage & vertex_or_index) != vertex_or_index);
            }

            // Calculate space
            // There are three different sizes to keep track of here:
            // desc.bytes_size  => the minimum size (in bytes) that we desire our buffer to occupy
            // buffer.info.per_frame_size => the above size that's been potentially grown to match alignment requirements
            // buffer.info.total_size     => If the buffer is used "dynamically" then we actually allocate enough memory
            //                               to update different regions of the buffer for different frames
            //                               (so RENDER_LATENCY * buffer.info.size)
            // Allocate resources

            u32 per_frame_size = desc.byte_size;
            if (desc.usage & RESOURCE_USAGE_VERTEX) {
                per_frame_size = align_to(per_frame_size, dx12::VERTEX_BUFFER_ALIGNMENT);
            }
            else if (desc.usage & RESOURCE_USAGE_INDEX) {
                per_frame_size = align_to(per_frame_size, dx12::INDEX_BUFFER_ALIGNMENT);
            }
            else if (desc.usage & RESOURCE_USAGE_CONSTANT) {
                per_frame_size = align_to(per_frame_size, dx12::CONSTANT_BUFFER_ALIGNMENT);
            }

            u32 is_cpu_accessible = desc.usage & RESOURCE_USAGE_DYNAMIC;
            u32 total_size = per_frame_size;
            if (is_cpu_accessible) {
                total_size = RENDER_LATENCY * per_frame_size;
            }

            Buffer buffer{
                .info = {
                    .total_size = total_size,
                    .per_frame_size = per_frame_size,
                    .stride = desc.stride,
                    .cpu_accessible = is_cpu_accessible,
                }
            };

            D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_STATES initial_resource_state = D3D12_RESOURCE_STATE_COMMON;
            if (is_cpu_accessible) {
                heap_type = D3D12_HEAP_TYPE_UPLOAD;
                initial_resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
            }

            D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_NONE;
            if (desc.usage & RESOURCE_USAGE_COMPUTE_WRITABLE) {
                resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer.info.total_size, resource_flags);
            D3D12MA::ALLOCATION_DESC alloc_desc = {};
            alloc_desc.HeapType = heap_type;

            HRESULT res = (g_context.allocator->CreateResource(
                &alloc_desc,
                &resource_desc,
                initial_resource_state,
                NULL,
                &buffer.allocation,
                IID_PPV_ARGS(&buffer.resource)
            ));
            if (res == DXGI_ERROR_DEVICE_REMOVED) {
                debug_print(GetDXErrorString(g_context.device->GetDeviceRemovedReason()));
            }
            DXCall(res);

            buffer.info.gpu_address = buffer.resource->GetGPUVirtualAddress();

            // Create the SRV
            if (desc.usage & RESOURCE_USAGE_SHADER_READABLE) {
                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
                buffer.srv = g_context.descriptor_heaps[HeapTypes::SRV].allocate_descriptors(1, &cpu_handle);

                bool is_raw = desc.type == BufferType::RAW;

                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
                    .Format = is_raw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN,
                    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Buffer = {
                        .FirstElement = 0,
                        .NumElements = desc.byte_size / desc.stride,
                        .StructureByteStride = is_raw ? 0 : desc.stride,
                        .Flags = is_raw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE
                    },
                };

                g_context.device->CreateShaderResourceView(buffer.resource, &srv_desc, cpu_handle);
            }

            // Create the UAV
            if (desc.usage & RESOURCE_USAGE_COMPUTE_WRITABLE) {
                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
                buffer.uav = g_context.descriptor_heaps[HeapTypes::UAV].allocate_descriptors(1, &cpu_handle);

                bool is_raw = desc.type == BufferType::RAW;

                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
                    .Format = is_raw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN,
                    .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                    .Buffer = {
                        .FirstElement = 0,
                        .NumElements = desc.byte_size / desc.stride,
                        .StructureByteStride = is_raw ? 0 : desc.stride,
                        .Flags = is_raw ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE,
                    },
                };

                g_context.device->CreateUnorderedAccessView(buffer.resource, nullptr, &uav_desc, cpu_handle);
            }

            // We map the buffer if we're going to be doing CPU writes to it.
            if (buffer.info.cpu_accessible) {
                buffer.resource->Map(0, nullptr, &buffer.info.cpu_address);
            }

            return g_context.buffers.store_buffer(buffer);
        };

        u32 get_shader_readable_index(const BufferHandle handle)
        {
            DescriptorRangeHandle descriptor = g_context.buffers.srvs[handle];
            return descriptor.get_offset();
        };
        u32 get_shader_writable_index(const BufferHandle handle)
        {
            DescriptorRangeHandle descriptor = g_context.buffers.uavs[handle];
            return descriptor.get_offset();
        };

        // Only use this for CPU accessible buffers. Otherwise, use the other form of this function that takes a CommandContext
        void set_data(const BufferHandle handle, const void* data, const u64 data_byte_size)
        {
            ASSERT(data != nullptr);
            ASSERT(is_valid(handle));
            BufferInfo& buffer_info = g_context.buffers.infos[handle];
            ASSERT(buffer_info.cpu_accessible);

            for (size_t i = 0; i < RENDER_LATENCY; i++) {
                void* cpu_address = buffer_info.get_cpu_address(i);
                memory::copy(cpu_address, data, data_byte_size);
            }

        };

        // Only use this for GPU exclusive buffers.
        void set_data(CommandContextHandle cmd_ctx, const BufferHandle handle, const void* data, const u64 data_byte_size)
        {
            ASSERT(data != nullptr);
            ASSERT(is_valid(cmd_ctx));
            ASSERT(cmd_utils::get_command_queue_type(cmd_ctx) != CommandQueueType::ASYNC_COMPUTE);

            const BufferInfo& buffer_info = g_context.buffers.infos[handle];

            // If it's not CPU accessible we'll have to create a new resource on the UPLOAD stack as a staging buffer
            ID3D12Resource* destination_resource = g_context.buffers.resources[handle];

            // Create upload resource and schedule copy
            const u64 upload_buffer_size = GetRequiredIntermediateSize(destination_resource, 0, 1);
            D3D12MA::Allocation* allocation = nullptr;
            ID3D12Resource* staging_resource = nullptr;
            D3D12MA::ALLOCATION_DESC upload_alloc_desc = {};
            D3D12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
            upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
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
            memory::copy(mapped_ptr, data, data_byte_size);
            staging_resource->Unmap(0, NULL);

            get_command_list(cmd_ctx)->CopyBufferRegion(destination_resource, 0, staging_resource, 0, data_byte_size);
            g_context.upload_store.push(cmd_ctx, staging_resource, allocation);
        };


        void update(const BufferHandle buffer_handle, const void* data, u64 byte_size)
        {
            ASSERT(is_valid(buffer_handle));
            BufferInfo& buffer_info = g_context.buffers.infos[buffer_handle];
            ASSERT(buffer_info.cpu_accessible);

            void* cpu_address = buffer_info.get_cpu_address(g_context.current_frame_idx);

            memory::copy(
                cpu_address,
                data,
                byte_size
            );
        }
        
        void destroy(BufferHandle handle)
        {
            g_context.buffers.destroy_buffer(
                handle,
                [](ID3D12Resource* resource, D3D12MA::Allocation* allocation) {
                    g_context.destruction_queue.enqueue(g_context.current_frame_idx, resource, allocation);
                },
                [](D3D12_DESCRIPTOR_HEAP_TYPE heap_type, DescriptorRangeHandle handle) {
                    g_context.descriptor_heaps[heap_type].free_descriptors(handle);
                }
            );
        };

        void destroy(CmdReceipt cmd_receipt, BufferHandle handle)
        {
            ID3D12Resource* resource = g_context.buffers.resources[handle];
            D3D12MA::Allocation* allocation = g_context.buffers.allocations[handle];
            g_context.async_destruction_queue.enqueue(cmd_receipt, resource, allocation);

            g_context.descriptor_heaps[HeapTypes::SRV].free_descriptors(g_context.buffers.srvs[handle]);
            g_context.descriptor_heaps[HeapTypes::UAV].free_descriptors(g_context.buffers.uavs[handle]);
            g_context.buffers.destroy_buffer(handle);
        };
    }

    namespace textures
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
            /*if (desc.initial_state != RESOURCE_USAGE_UNUSED);
            ASSERT(desc.initial_state & desc.usage);*/
            // TODO: Is this needed?
            //initial_state = to_d3d_resource_state(desc.initial_state);

            if (desc.usage & RESOURCE_USAGE_COMPUTE_WRITABLE) {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
            }

            D3D12_CLEAR_VALUE optimized_clear_value = { };
            if (desc.usage & RESOURCE_USAGE_DEPTH_STENCIL) {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
                optimized_clear_value.Format = d3d_format;
                optimized_clear_value.DepthStencil = { desc.clear_depth, desc.clear_stencil };
            }

            if (desc.usage & RESOURCE_USAGE_RENDER_TARGET) {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
                optimized_clear_value.Format = d3d_format;
                optimized_clear_value.Color[0] = desc.clear_color[0];
                optimized_clear_value.Color[1] = desc.clear_color[1];
                optimized_clear_value.Color[2] = desc.clear_color[2];
                optimized_clear_value.Color[3] = desc.clear_color[3];
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

            D3D12_RESOURCE_STATES initial_state = desc.initial_state == RESOURCE_USAGE_UNUSED
                ? D3D12_RESOURCE_STATE_COMMON
                : to_d3d_resource_state(desc.initial_state);

            const bool depth_or_rt = bool(desc.usage & (RESOURCE_USAGE_DEPTH_STENCIL | RESOURCE_USAGE_RENDER_TARGET));
            DXCall(g_context.allocator->CreateResource(
                &alloc_desc,
                &d3d_desc,
                initial_state,
                depth_or_rt ? &optimized_clear_value : nullptr,
                &texture.allocation,
                IID_PPV_ARGS(&texture.resource)
            ));

            // Allocate SRV
            if (desc.usage & RESOURCE_USAGE_SHADER_READABLE) {

                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
                DescriptorRangeHandle srv = g_context.descriptor_heaps[HeapTypes::SRV].allocate_descriptors(1, &cpu_handle);
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
                    g_context.device->CreateShaderResourceView(texture.resource, nullptr, cpu_handle);
                }
                else {
                    g_context.device->CreateShaderResourceView(texture.resource, &srv_desc, cpu_handle);
                }
            }

            // Allocate UAV
            if (desc.usage & RESOURCE_USAGE_COMPUTE_WRITABLE) {
                constexpr u32 MAX_NUM_MIPS = 16;
                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[MAX_NUM_MIPS] = {};
                ASSERT(texture.info.num_mips < MAX_NUM_MIPS);
                DescriptorRangeHandle uav = g_context.descriptor_heaps[HeapTypes::SRV]
                    .allocate_descriptors(texture.info.num_mips, cpu_handles);
                texture.uav = uav;

                // Cannot support 3D texture arrays
                ASSERT(!desc.is_3d || desc.array_size == 1);

                for (u32 i = 0; i < texture.info.num_mips; i++) {
                    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
                        .Format = d3d_format,
                        .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
                    };

                    if (desc.is_3d) {
                        uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                        uav_desc.Texture3D = {
                            .MipSlice = i,
                            .FirstWSlice = 0,
                            .WSize = desc.depth,
                        };
                    }
                    else if (desc.is_cubemap || desc.array_size > 1) {
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
                    g_context.device->CreateUnorderedAccessView(texture.resource, nullptr, &uav_desc, cpu_handles[i]);
                }
            }

            // Allocate RTV
            if (desc.usage & RESOURCE_USAGE_RENDER_TARGET) {
                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
                DescriptorRangeHandle rtv = g_context.descriptor_heaps[HeapTypes::RTV].allocate_descriptors(1, &cpu_handle);
                texture.rtv = rtv;

                g_context.device->CreateRenderTargetView(texture.resource, nullptr, cpu_handle);
            }

            // Allocate DSV
            if (desc.usage & RESOURCE_USAGE_DEPTH_STENCIL) {
                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
                DescriptorRangeHandle dsv = g_context.descriptor_heaps[HeapTypes::DSV].allocate_descriptors(texture.info.array_size, &cpu_handle);
                texture.dsv = dsv;

                D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
                    .Format = d3d_format,
                    .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                    .Flags = D3D12_DSV_FLAG_NONE,
                    .Texture2D = {
                        .MipSlice = 0,
                    },
                };

                g_context.device->CreateDepthStencilView(texture.resource, &dsv_desc, cpu_handle);
            }

            return g_context.textures.push_back(texture);
        };

        u32 get_shader_readable_index(const TextureHandle handle)
        {
            DescriptorRangeHandle descriptor = g_context.textures.srvs[handle];
            return descriptor.get_offset();
        };
        u32 get_shader_writable_index(const TextureHandle handle)
        {
            DescriptorRangeHandle descriptor = g_context.textures.uavs[handle];
            return descriptor.get_offset();
        };

        const TextureInfo& get_texture_info(const TextureHandle texture_handle)
        {
            return g_context.textures.infos[texture_handle];
        };

        TextureHandle create_from_file(CommandContextHandle cmd_ctx, const char* file_path)
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
            TextureHandle texture_handle = textures::create(texture_desc);
            set_debug_name(texture_handle, path.c_str());

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

        /*TextureHandle load_from_file(const char* file_path, const bool force_srgb = false);
        void save_to_file(const TextureHandle texture_handle, const wchar_t* file_path, const ResourceUsage current_usage);*/

    }

    namespace cmd
    {
        // ---------- Command Contexts ----------
        CommandContextHandle provision(CommandQueueType type)
        {
            CommandContextHandle cmd_ctx = g_context.command_pools[type].provision();
            if (type != CommandQueueType::COPY) {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(cmd_ctx);
                auto& heap = g_context.descriptor_heaps[HeapTypes::SRV];
                cmd_list->SetDescriptorHeaps(1, &heap.heap);
            }
            return cmd_ctx;
        };

        CmdReceipt return_and_execute(CommandContextHandle* context_handles, const size_t num_contexts)
        {
            constexpr size_t MAX_NUM_SIMULTANEOUS_COMMAND_LIST_EXECUTION = 128; // Arbitrary limit 
            ASSERT((num_contexts < MAX_NUM_SIMULTANEOUS_COMMAND_LIST_EXECUTION) && num_contexts > 0);

            ID3D12GraphicsCommandList** cmd_lists = static_cast<ID3D12GraphicsCommandList**>(_alloca(num_contexts * sizeof(ID3D12GraphicsCommandList*)));

            CommandQueueType queue_type = cmd_utils::get_command_queue_type(context_handles[0]);
            CommandContextPool& pool = g_context.command_pools[queue_type];
            CommandQueue& queue = g_context.command_queues[queue_type];

            // Collect command lists
            for (size_t i = 0; i < num_contexts; i++) {
                // Make sure all contexts are using the same pool/queue
                ASSERT(cmd_utils::get_command_queue_type(context_handles[i]) == queue_type);

                cmd_lists[i] = pool.get_graphics_command_list(context_handles[i]);
                DXCall(cmd_lists[i]->Close());
            }

            u64 fence_value = queue.execute(cmd_lists, num_contexts);
            CmdReceipt receipt = {
                .queue_type = queue_type,
                .fence_value = fence_value,
            };
            // Check whether this command context was used for any upload tasks,
            // and if so put any resources in that context onto our async destruction queue
            for (size_t i = 0; i < num_contexts; i++) {
                if (g_context.upload_store.get_staged_uploads(context_handles[i])) {
                    Array<UploadContextStore::Upload>* uploads = g_context.upload_store.get_staged_uploads(context_handles[i]);
                    for (UploadContextStore::Upload& upload : *uploads) {
                        queue_destruction(g_context.async_destruction_queue, receipt, upload.resource, upload.allocation);
                    }
                    g_context.upload_store.clear_staged_uploads(context_handles[i]);
                }
            }
            // Return to pools
            for (size_t i = 0; i < num_contexts; i++) {
                pool.return_to_pool(receipt.fence_value, context_handles[i]);
            }

            return receipt;
        };

        bool check_status(const CmdReceipt receipt)
        {
            return g_context.command_queues[receipt.queue_type].check_fence_status(receipt.fence_value);
        };

        void flush_queue(const CommandQueueType type)
        {
            g_context.command_queues[type].flush();
        };

        void cpu_wait(const CmdReceipt receipt)
        {
            g_context.command_queues[receipt.queue_type].cpu_wait(receipt.fence_value);
        };

        void gpu_wait(const CommandQueueType queue_to_insert_wait, const CmdReceipt receipt_to_wait_on)
        {
            CommandQueue& queue_to_wait_on = g_context.command_queues[receipt_to_wait_on.queue_type];
            g_context.command_queues[queue_to_insert_wait].insert_wait_on_queue(queue_to_wait_on, receipt_to_wait_on.fence_value);
        };

        namespace graphics
        {
            //--------- Resource Binding ----------
            void set_active_resource_layout(const CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                ID3D12RootSignature* root_signature = g_context.root_signatures[resource_layout_id];

                cmd_list->SetGraphicsRootSignature(root_signature);
            };

            void set_pipeline_state(const CommandContextHandle ctx, const PipelineStateHandle pso_handle)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                ID3D12PipelineState* pso = g_context.pipelines[pso_handle];
                cmd_list->SetPipelineState(pso);
            };

            void bind_resource_table(const CommandContextHandle ctx, const u32 resource_layout_entry_idx)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                const DescriptorHeap& heap = g_context.descriptor_heaps[HeapTypes::SRV];
                D3D12_GPU_DESCRIPTOR_HANDLE handle = heap.gpu_start;
                cmd_list->SetGraphicsRootDescriptorTable(resource_layout_entry_idx, handle);
            }

            void bind_constants(const CommandContextHandle ctx, const void* data, const u32 num_constants, const u32 binding_slot)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                cmd_list->SetGraphicsRoot32BitConstants(binding_slot, num_constants, data, 0);
            };

            void bind_constant_buffer(const CommandContextHandle ctx, const BufferHandle& buffer_handle, const u32 binding_slot)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                const BufferInfo& buffer_info = g_context.buffers.infos[buffer_handle];
                cmd_list->SetGraphicsRootConstantBufferView(binding_slot, buffer_info.get_gpu_address(g_context.current_frame_idx));
            };

            // Draw / Dispatch
            void draw_lines(const CommandContextHandle ctx, const BufferHandle vertices)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);

                const BufferInfo& buffer_info = g_context.buffers.infos[vertices];
                u32 stride = sizeof(float) * 3;
                D3D12_VERTEX_BUFFER_VIEW vb_view = {
                    .BufferLocation = buffer_info.gpu_address,
                    .SizeInBytes = u32(buffer_info.per_frame_size),
                    .StrideInBytes = stride,
                };
                cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
                cmd_list->IASetVertexBuffers(0, 1, &vb_view);
                cmd_list->DrawInstanced(u32(buffer_info.per_frame_size) / stride, 1, 0, 0);
            };

            void draw_mesh(const CommandContextHandle ctx, const BufferHandle index_buffer_id)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                const BufferInfo& buffer_info = g_context.buffers.infos[index_buffer_id];

                D3D12_INDEX_BUFFER_VIEW view{
                    .BufferLocation = buffer_info.gpu_address,
                    .SizeInBytes = u32(buffer_info.per_frame_size),
                    .Format = buffer_info.stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
                };
                cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                cmd_list->IASetIndexBuffer(&view);
                cmd_list->DrawIndexedInstanced(u32(buffer_info.per_frame_size / buffer_info.stride), 1, 0, 0, 0);
            }
        }

        namespace compute
        {
            //--------- Resource Binding ----------
            void set_active_resource_layout(const CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                ID3D12RootSignature* root_signature = g_context.root_signatures[resource_layout_id];

                CommandQueueType queue_type = cmd_utils::get_command_queue_type(ctx);

                cmd_list->SetComputeRootSignature(root_signature);
            };

            void set_pipeline_state(const CommandContextHandle ctx, const PipelineStateHandle pso_handle)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                ID3D12PipelineState* pso = g_context.pipelines[pso_handle];
                cmd_list->SetPipelineState(pso);
            };

            void bind_resource_table(const CommandContextHandle ctx, const u32 resource_layout_entry_idx)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                const DescriptorHeap& heap = g_context.descriptor_heaps[HeapTypes::SRV];
                D3D12_GPU_DESCRIPTOR_HANDLE handle = heap.gpu_start;

                cmd_list->SetComputeRootDescriptorTable(resource_layout_entry_idx, handle);
            }

            void bind_constants(const CommandContextHandle ctx, const void* data, const u32 num_constants, const u32 binding_slot)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                cmd_list->SetComputeRoot32BitConstants(binding_slot, num_constants, data, 0);
            };

            void bind_constant_buffer(const CommandContextHandle ctx, const BufferHandle& buffer_handle, const u32 binding_slot)
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                const BufferInfo& buffer_info = g_context.buffers.infos[buffer_handle];
                cmd_list->SetComputeRootConstantBufferView(binding_slot, buffer_info.get_gpu_address(g_context.current_frame_idx));
            };

            void dispatch(
                const CommandContextHandle ctx,
                const u32 thread_group_count_x,
                const u32 thread_group_count_y,
                const u32 thread_group_count_z
            )
            {
                ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
                cmd_list->Dispatch(thread_group_count_x, thread_group_count_y, thread_group_count_z);
            }
        }

        // Misc
        void clear_render_target(const CommandContextHandle ctx, const TextureHandle render_texture, const float* clear_color)
        {
            ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
            DescriptorRangeHandle rtv = g_context.textures.rtvs[render_texture];
            // TODO: Descriptors
            auto cpu_handle = g_context.descriptor_heaps[HeapTypes::RTV].get_cpu_descriptor_handle(rtv);
            cmd_list->ClearRenderTargetView(cpu_handle, clear_color, 0, nullptr);
        };

        void clear_depth_target(const CommandContextHandle ctx, const TextureHandle depth_stencil_buffer, const float depth_value, const u8 stencil_value)
        {
            ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
            const DescriptorRangeHandle dsv = g_context.textures.dsv_infos[depth_stencil_buffer];
            // TODO: Descriptors
            auto cpu_handle = g_context.descriptor_heaps[HeapTypes::DSV].get_cpu_descriptor_handle(dsv);
            // I don't actually think this will clear stencil. We should probably change that.
            cmd_list->ClearDepthStencilView(cpu_handle, D3D12_CLEAR_FLAG_DEPTH, depth_value, stencil_value, 0, nullptr);
        };

        void set_viewports(const CommandContextHandle ctx, const Viewport* viewports, const u32 num_viewports)
        {
            ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
            cmd_list->RSSetViewports(num_viewports, reinterpret_cast<const D3D12_VIEWPORT*>(viewports));
        };

        void set_scissors(const CommandContextHandle ctx, const Scissor* scissors, const u32 num_scissors)
        {
            ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
            D3D12_RECT rects[16];
            for (size_t i = 0; i < num_scissors; i++) {
                rects[i].left = LONG(scissors[i].left);
                rects[i].right = LONG(scissors[i].right);
                rects[i].top = LONG(scissors[i].top);
                rects[i].bottom = LONG(scissors[i].bottom);
            }
            cmd_list->RSSetScissorRects(num_scissors, rects);
        };

        void set_render_targets(
            const CommandContextHandle ctx,
            TextureHandle* render_textures,
            const u32 num_render_targets,
            const TextureHandle depth_target
        )
        {
            ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
            D3D12_CPU_DESCRIPTOR_HANDLE dsv;
            D3D12_CPU_DESCRIPTOR_HANDLE* dsv_ptr = nullptr;
            if (is_valid(depth_target)) {
                DescriptorRangeHandle descriptor_handle = g_context.textures.dsv_infos[depth_target];
                dsv = g_context.descriptor_heaps[HeapTypes::DSV].get_cpu_descriptor_handle(descriptor_handle);
                dsv_ptr = &dsv;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE rtvs[16] = {};
            for (size_t i = 0; i < num_render_targets; i++) {
                DescriptorRangeHandle descriptor_handle = g_context.textures.rtvs[render_textures[i]];
                rtvs[i] = g_context.descriptor_heaps[HeapTypes::RTV].get_cpu_descriptor_handle(descriptor_handle);
            }
            cmd_list->OMSetRenderTargets(num_render_targets, rtvs, FALSE, dsv_ptr);
        };

        void transition_textures(
            const CommandContextHandle ctx,
            TextureTransitionDesc* transition_descs,
            u64 num_transitions
        )
        {
            constexpr u64 MAX_NUM_BARRIERS = 16;
            ASSERT(num_transitions <= MAX_NUM_BARRIERS);
            D3D12_RESOURCE_BARRIER barriers[MAX_NUM_BARRIERS];
            for (size_t i = 0; i < num_transitions; i++) {
                D3D12_RESOURCE_BARRIER& barrier = barriers[i];
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = g_context.textures.resources[transition_descs[i].texture];
                barrier.Transition.StateBefore = to_d3d_resource_state(transition_descs[i].before);
                barrier.Transition.StateAfter = to_d3d_resource_state(transition_descs[i].after);
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            }
            ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
            cmd_list->ResourceBarrier(u32(num_transitions), barriers);
        };

        void transition_resources(
            const CommandContextHandle ctx,
            ResourceTransitionDesc* transition_descs,
            u64 num_transitions
        )
        {
            constexpr u64 MAX_NUM_BARRIERS = 16;
            ASSERT(num_transitions <= MAX_NUM_BARRIERS);
            D3D12_RESOURCE_BARRIER barriers[MAX_NUM_BARRIERS];
            for (size_t i = 0; i < num_transitions; i++) {
                D3D12_RESOURCE_BARRIER& barrier = barriers[i];
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                ID3D12Resource* resource = nullptr;
                if (transition_descs[i].type == ResourceTransitionType::TEXTURE) {
                    resource = g_context.textures.resources[transition_descs[i].texture];
                }
                else {
                    resource = g_context.buffers.resources[transition_descs[i].buffer];
                }
                barrier.Transition.pResource = resource;
                barrier.Transition.StateBefore = to_d3d_resource_state(transition_descs[i].before);
                barrier.Transition.StateAfter = to_d3d_resource_state(transition_descs[i].after);
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            }
            ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
            cmd_list->ResourceBarrier(u32(num_transitions), barriers);
        };
    }

    void set_debug_name(const ResourceLayoutHandle handle, const wchar* name)
    {
        g_context.root_signatures[handle]->SetName(name);
    };
    void set_debug_name(const PipelineStateHandle handle, const wchar* name)
    {
        g_context.pipelines[handle]->SetName(name);
    };
    void set_debug_name(const BufferHandle handle, const wchar* name)
    {
        g_context.buffers.resources[handle]->SetName(name);
    };
    void set_debug_name(const TextureHandle handle, const wchar* name)
    {
        g_context.textures.resources[handle]->SetName(name);
    };
}
