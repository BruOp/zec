#pragma once
#include "pch.h"
#include "core/map.h"
#include "wrappers.h"
#include "resources.h"
#include "resource_managers.h"
#include "upload_manager.h"
#include "gfx/public.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

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

        void begin_upload();
        void end_upload();

        void begin_frame();
        void end_frame();

        // Resource creation
        BufferHandle create_buffer(BufferDesc buffer_desc);
        MeshHandle create_mesh(MeshDesc mesh_desc);

        ResourceLayoutHandle create_resource_layout(const ResourceLayoutDesc& desc);
        PipelineStateHandle  create_pipeline_state_object(const PipelineStateObjectDesc& desc);

        // Resource updates
        void update_buffer(const BufferHandle buffer_id, const void* data, u64 byte_size);

        // Resource Binding
        void set_active_resource_layout(const ResourceLayoutHandle resource_layout_id);
        void set_pipeline_state(const PipelineStateHandle pso_handle);
        void bind_constant_buffer(const BufferHandle& buffer_handle, u32 binding_slot);
        void draw_mesh(const MeshHandle mesh_id);

        u64 current_frame_idx = 0;
        // Total number of CPU frames completed (means that we've recorded and submitted commands for the frame)
        u64 current_cpu_frame = 0;
        // Total number of GPU frames completed (completed means that the GPU signals the fence)
        u64 current_gpu_frame = 0;

        dx12::DeviceContext device_context = {};

        D3D12MA::Allocator* allocator = nullptr;

        ID3D12GraphicsCommandList1* cmd_list = 0;
        ID3D12CommandAllocator* cmd_allocators[NUM_CMD_ALLOCATORS] = {};
        ID3D12CommandQueue* gfx_queue = 0;
        ID3D12CommandQueue* copy_queue = 0;

        dx12::ResourceDestructionQueue destruction_queue = {};
        dx12::FenceManager fence_manager = {};
        dx12::UploadManager upload_manager = { };


        dx12::SwapChain swap_chain = {};
        dx12::DescriptorHeap rtv_descriptor_heap = {};
        dx12::DescriptorHeap dsv_descriptor_heap = {};
        dx12::DescriptorHeap srv_descriptor_heap = {};

        Array<ID3D12RootSignature*> root_signatures = {};
        Array<ID3D12PipelineState*> pipelines = {};

        dx12::Fence frame_fence = { };

        dx12::ResourceList<dx12::Buffer, BufferHandle> buffers = { &destruction_queue };
        Array<dx12::Mesh> meshes = {};

        //SmallMap<dx12::RenderTexture> render_textures = {};

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