#include "pch.h"

namespace zec
{
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

        // ---------- Helper functions For globals ----------
        void initialize_renderer();
        void destroy_renderer();

        void start_frame();
        void end_frame();
    } // namespace dx12
} // namespace zec