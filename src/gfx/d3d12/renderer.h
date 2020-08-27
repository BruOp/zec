#pragma once
#include "pch.h"
#include "wrappers.h"
#include "resource_managers.h"
#include "gfx/public.h"

namespace zec
{
    struct RendererDesc;

    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer();

        void init(const RendererDesc& renderer_desc);
        void destroy();

        void begin_frame();
        void end_frame();

        u64 current_frame_idx = 0;
        // Total number of CPU frames completed (means that we've recorded and submitted commands for the frame)
        u64 current_cpu_frame = 0;
        // Total number of GPU frames completed (completed means that the GPU signals the fence)
        u64 current_gpu_frame = 0;

        dx12::DeviceContext device_context = {};

        ID3D12GraphicsCommandList1* cmd_list = 0;
        ID3D12CommandAllocator* cmd_allocators[NUM_CMD_ALLOCATORS] = {};
        ID3D12CommandQueue* gfx_queue = 0;

        dx12::ResourceDestructionQueue destruction_queue = {};
        dx12::FenceManager fence_manager = {};

        dx12::SwapChain swap_chain = {};
        dx12::DescriptorHeap rtv_descriptor_heap = {};
        dx12::Fence frame_fence = { };

    private:
        void reset();

        void reset_swap_chain();
        void recreate_swap_chain_rtv_descriptors();
        void prepare_full_screen_settings();

        /*CommandContext get_command_context();*/
    };


    //namespace dx12
    //{
        //enum struct CommandContextType : u8
        //{
        //    Graphics = 0u,
        //    Upload,
        //    Compute
        //};

        //struct BaseCommandContext
        //{
        //    CommandContextType type;

        //    //virtual void begin() = 0;
        //    //virtual void end() = 0;
        //    //virtual void insert_resource_barrier(ResourceBarrierDesc& barrier_desc) = 0;
        //};

        //template<typename CmdList>
        //void begin_recording(CmdList* cmd_list);

        //template<typename CmdList>
        //void end_recording(CmdList* cmd_list);

        //// TODO: Additional context types

        //class CommandContextManager
        //{

        //}
    //}
}