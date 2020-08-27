#include "pch.h"
#include "globals.h"
#include "gfx/public.h"
#include "gfx/constants.h"
#include "utils/utils.h"
#include "descriptor_heap.h"
#include "swap_chain.h"

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
    namespace dx12
    {
        // ---------- Global variables ----------
        u64 current_frame_idx = 0;
        u64 current_cpu_frame = 0;
        u64 current_gpu_frame = 0;

        IDXGIFactory4* factory = nullptr;
        IDXGIAdapter1* adapter = nullptr;
        ID3D12Device* device = nullptr;
        D3D_FEATURE_LEVEL supported_feature_level;

        ID3D12GraphicsCommandList1* cmd_list = nullptr;
        ID3D12CommandQueue* gfx_queue = nullptr;

        SwapChain swap_chain = {};

        DescriptorHeap rtv_descriptor_heap = {};

        Array<IUnknown*> deferred_releases = {};

        static Fence frame_fence{};
        static ID3D12CommandAllocator* cmd_allocators[NUM_CMD_ALLOCATORS] = {};

        // ---------- Helpers ----------
        void process_deferred_destruction()
        {
            for (size_t i = 0; i < deferred_releases.size; i++) {
                deferred_releases[i]->Release();
                deferred_releases[i] = nullptr;
            }
        }

        // ---------- Implementation ----------
        void init_renderer(const RendererDesc& renderer_desc)
        {
            // Factory, Adaptor and Device initialization
            {
                ASSERT(adapter == nullptr);
                ASSERT(factory == nullptr);
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

                DXCall(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)));

                factory->EnumAdapters1(ADAPTER_NUMBER, &adapter);

                if (adapter == nullptr) {
                    throw Exception(L"Unabled to located DXGI 1.4 adapter that supports D3D12.");
                }

                DXGI_ADAPTER_DESC1 desc{  };
                adapter->GetDesc1(&desc);
                write_log("Creating DX12 device on adapter '%ls'", desc.Description);

                DXCall(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

                D3D_FEATURE_LEVEL feature_levels_arr[4] = {
                    D3D_FEATURE_LEVEL_11_0,
                    D3D_FEATURE_LEVEL_11_1,
                    D3D_FEATURE_LEVEL_12_0,
                    D3D_FEATURE_LEVEL_12_1,
                };
                D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels = { };
                feature_levels.NumFeatureLevels = ARRAY_SIZE(feature_levels_arr);
                feature_levels.pFeatureLevelsRequested = feature_levels_arr;
                DXCall(device->CheckFeatureSupport(
                    D3D12_FEATURE_FEATURE_LEVELS,
                    &feature_levels,
                    sizeof(feature_levels)
                ));
                supported_feature_level = feature_levels.MaxSupportedFeatureLevel;

                D3D_FEATURE_LEVEL min_feature_level = D3D_FEATURE_LEVEL_12_0;
                if (supported_feature_level < min_feature_level) {
                    std::wstring majorLevel = to_string<int>(min_feature_level >> 12);
                    std::wstring minorLevel = to_string<int>((min_feature_level >> 8) & 0xF);
                    throw Exception(L"The device doesn't support the minimum feature level required to run this sample (DX" + majorLevel + L"." + minorLevel + L")");
                }

            #ifdef USE_DEBUG_DEVICE
                ID3D12InfoQueue* infoQueue;
                DXCall(device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

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

                // Initialize command allocators and command list
                // We need N allocators for N frames, since we cannot reset them and reclaim the associated memory
                // until the GPU is finished executing against them. So we have to use fences
                for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                    DXCall(device->CreateCommandAllocator(
                        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_allocators[i])
                    ));
                }

                // Command lists, however, can be reset as soon as we've submitted them.
                DXCall(device->CreateCommandList(
                    0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocators[0], nullptr, IID_PPV_ARGS(&cmd_list)
                ));
                DXCall(cmd_list->Close());
                cmd_list->SetName(L"Graphics command list");

                // Initialize graphics queue
                D3D12_COMMAND_QUEUE_DESC queue_desc{ };
                queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

                DXCall(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&gfx_queue)));
                gfx_queue->SetName(L"Graphics queue");

                // Set current frame index and reset command allocator and list appropriately
                ID3D12CommandAllocator* current_cmd_allocator = cmd_allocators[0];
                DXCall(current_cmd_allocator->Reset());
                DXCall(cmd_list->Reset(current_cmd_allocator, nullptr));
            }


            // RTV Descriptor Heap initialization
            DescriptorHeapDesc rtv_descriptor_heap_desc{ };
            rtv_descriptor_heap_desc.heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtv_descriptor_heap_desc.num_persistent = 256;
            rtv_descriptor_heap_desc.num_temporary = 0;
            rtv_descriptor_heap_desc.is_shader_visible = false;
            init(rtv_descriptor_heap, rtv_descriptor_heap_desc);

            // Swapchain initializaiton
            SwapChainDesc swap_chain_desc{ };
            swap_chain_desc.width = renderer_desc.width;
            swap_chain_desc.height = renderer_desc.height;
            swap_chain_desc.fullscreen = renderer_desc.fullscreen;
            swap_chain_desc.vsync = renderer_desc.vsync;
            swap_chain_desc.output_window = renderer_desc.window;

            init(swap_chain, swap_chain_desc);

            init(frame_fence, 0);
        }

        void destroy_renderer()
        {
            write_log("Destroying renderer");
            wait(frame_fence, current_cpu_frame);

            destroy(swap_chain);
            destroy(rtv_descriptor_heap);

            cmd_list->Release();
            for (u64 i = 0; i < RENDER_LATENCY; i++) {
                cmd_allocators[i]->Release();
            }
            gfx_queue->Release();

            destroy(frame_fence);

            // DXCall(device->QueryInterface(IID_PPV_ARGS(&debug_device)));
            // debug_device->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY);
            // debug_device->Release();

            process_deferred_destruction();

            device->Release();
            adapter->Release();
            factory->Release();

        }

        void reset_for_frame()
        {
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
            process_deferred_destruction();

            // Process shader resource views queued for creation

        }

        void start_frame()
        {
            // Don't need to reset if we haven't recorded anything yet.
            if (current_cpu_frame != 0) {
                reset_for_frame();
            }
        }

        void end_frame()
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
    }
}