#include "pch.h"
#include "swap_chain.h"
#include "descriptor_heap.h"

namespace zec
{
    struct RendererDesc;

    namespace dx12
    {
        struct SwapChain;
        struct DescriptorHeap;

        // ---------- Globals ----------
        extern u64 current_frame_idx;
        extern u64 current_cpu_frame;  // Total number of GPU frames completed (completed means that the GPU signals the fence)
        extern u64 current_gpu_frame;  // Total number of GPU frames completed (completed means that the GPU signals the fence)

        extern IDXGIFactory4* factory;
        extern IDXGIAdapter1* adapter;
        extern ID3D12Device* device;
        extern D3D_FEATURE_LEVEL supported_feature_level;

        extern ID3D12GraphicsCommandList1* cmd_list;
        extern ID3D12CommandQueue* gfx_queue;

        extern SwapChain swap_chain;

        extern DescriptorHeap rtv_descriptor_heap;

        static Array<IUnknown*> deferred_releases{ };

        // ---------- Helper functions For globals ----------
        void init_renderer(const RendererDesc& renderer_desc);
        void destroy_renderer();

        void start_frame();
        void end_frame();

        template<typename T>
        void queue_resource_destruction(T*& resource)
        {
            if (resource == nullptr) {
                return;
            }
            // So resource is a reference to a ptr of type T that is castable to IUnknown
            deferred_releases.push_back(resource);
            resource = nullptr;
        }

    } // namespace dx12
} // namespace zec