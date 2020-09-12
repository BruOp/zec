#include "pch.h"
#include "renderer.h"
#include "d3dx12/d3dx12.h"
#include "gfx/public.h"
#include "gfx/constants.h"

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
            {
                D3D12_COMMAND_QUEUE_DESC queue_desc{ };
                queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

                DXCall(device_context.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&gfx_queue)));
                gfx_queue->SetName(L"Graphics queue");
            }

            // Initialize graphics queue
            {
                D3D12_COMMAND_QUEUE_DESC queue_desc{ };
                queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

                DXCall(device_context.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&copy_queue)));
                copy_queue->SetName(L"Copy Queue");
            }

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
        dsv_descriptor_heap_desc.heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsv_descriptor_heap_desc.num_persistent = 256;
        dsv_descriptor_heap_desc.num_temporary = 0;
        dsv_descriptor_heap_desc.is_shader_visible = false;
        dx12::init(dsv_descriptor_heap, dsv_descriptor_heap_desc, device_context);

        // Descriptor Heap for SRV, CBV and UAV resources
        dx12::DescriptorHeapDesc srv_descriptor_heap_desc{ };
        srv_descriptor_heap_desc.heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srv_descriptor_heap_desc.num_persistent = 1024;
        srv_descriptor_heap_desc.num_temporary = 1024;
        srv_descriptor_heap_desc.is_shader_visible = true;
        dx12::init(srv_descriptor_heap, srv_descriptor_heap_desc, device_context);

        // Swapchain initializaiton
        {
            ASSERT(swap_chain.swap_chain == nullptr);

            dx12::SwapChainDesc swap_chain_desc{ };
            swap_chain_desc.width = renderer_desc.width;
            swap_chain_desc.height = renderer_desc.height;
            swap_chain_desc.fullscreen = renderer_desc.fullscreen;
            swap_chain_desc.vsync = renderer_desc.vsync;
            swap_chain_desc.output_window = renderer_desc.window;
            swap_chain_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

            device_context.adapter->EnumOutputs(0, &swap_chain.output);

            swap_chain.width = swap_chain_desc.width;
            swap_chain.height = swap_chain_desc.height;
            swap_chain.fullscreen = swap_chain_desc.fullscreen;
            swap_chain.vsync = swap_chain_desc.vsync;

            set_formats(swap_chain, swap_chain_desc.format);

            DXGI_SWAP_CHAIN_DESC d3d_swap_chain_desc = {  };
            d3d_swap_chain_desc.BufferCount = NUM_BACK_BUFFERS;
            d3d_swap_chain_desc.BufferDesc.Width = swap_chain.width;
            d3d_swap_chain_desc.BufferDesc.Height = swap_chain.height;
            d3d_swap_chain_desc.BufferDesc.Format = swap_chain.non_sRGB_format;
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

        upload_manager.init({
            device_context.device,
            allocator,
            copy_queue,
            &fence_manager });
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

        for (size_t i = 0; i < root_signatures.size; ++i) {
            ID3D12RootSignature* root_signature = root_signatures[i];
            dx12::dx_destroy(&root_signature);
        }

        dx12::destroy(rtv_descriptor_heap);
        dx12::destroy(dsv_descriptor_heap);
        dx12::destroy(srv_descriptor_heap);
        upload_manager.destroy();
        buffers.destroy();
        fence_manager.destroy();

        dx12::dx_destroy(&cmd_list);
        for (u64 i = 0; i < RENDER_LATENCY; i++) {
            cmd_allocators[i]->Release();
        }
        dx12::dx_destroy(&gfx_queue);
        dx12::dx_destroy(&copy_queue);

        destruction_queue.destroy();
        dx12::dx_destroy(&allocator);

        dx12::dx_destroy(&device_context.device);
        dx12::dx_destroy(&device_context.adapter);
        dx12::dx_destroy(&device_context.factory);
    }

    void Renderer::begin_upload()
    {
        upload_manager.begin_upload();
    }

    void Renderer::end_upload()
    {
        upload_manager.end_upload();
        gfx_queue->Wait(upload_manager.fence.d3d_fence, upload_manager.current_fence_value);
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
        }

        // Process resources queued for destruction
        destruction_queue.process_queue();

        // TODO: Provide interface for creating barriers like this
        dx12::RenderTexture& render_target = swap_chain.back_buffers[current_frame_idx];

        D3D12_RESOURCE_BARRIER barrier{  };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = render_target.resource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd_list->ResourceBarrier(1, &barrier);
    }

    void Renderer::end_frame()
    {
        dx12::RenderTexture& render_target = swap_chain.back_buffers[current_frame_idx];

        D3D12_RESOURCE_BARRIER barrier{  };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = render_target.resource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd_list->ResourceBarrier(1, &barrier);

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

    BufferHandle Renderer::create_buffer(BufferDesc desc)
    {
        ASSERT(desc.byte_size != 0);
        ASSERT(desc.usage != BufferUsage::UNUSED);

        BufferHandle handle = { buffers.create_back() };
        dx12::Buffer& buffer = buffers.get(handle);

        dx12::init(buffer, desc, allocator);

        if (desc.usage & BufferUsage::DYNAMIC && desc.data != nullptr) {
            for (size_t i = 0; i < RENDER_LATENCY; i++) {
                dx12::update_buffer(buffer, desc.data, desc.byte_size, i);
            }
        }
        else if (desc.data != nullptr) {
            upload_manager.queue_upload(desc, buffer.resource);
        }

        return handle;
    }

    MeshHandle Renderer::create_mesh(MeshDesc mesh_desc)
    {
        dx12::Mesh mesh{};
        // Index buffer creation
        {
            ASSERT(mesh_desc.index_buffer_desc.usage == BufferUsage::INDEX);
            mesh.index_buffer_handle = create_buffer(mesh_desc.index_buffer_desc);

            const dx12::Buffer& index_buffer = buffers[mesh.index_buffer_handle];
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
            if (attr_desc.usage == BufferUsage::UNUSED) break;

            // TODO: Is this actually needed?
            ASSERT(attr_desc.usage == BufferUsage::VERTEX);
            ASSERT(attr_desc.type == BufferType::DEFAULT);

            mesh.vertex_buffer_handles[i] = create_buffer(attr_desc);
            const dx12::Buffer& buffer = buffers[mesh.vertex_buffer_handles[i]];

            D3D12_VERTEX_BUFFER_VIEW& view = mesh.buffer_views[i];
            view.BufferLocation = buffer.gpu_address;
            view.StrideInBytes = attr_desc.stride;
            view.SizeInBytes = attr_desc.byte_size;
            mesh.num_vertex_buffers++;
        }

        // TODO: Support blend weights, indices
        return { u32(meshes.push_back(mesh)) };
    }

    ResourceLayoutHandle Renderer::create_resource_layout(const ResourceLayoutDesc& desc)
    {
        D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
        D3D12_ROOT_PARAMETER root_parameters[ResourceLayoutDesc::MAX_ENTRIES];

        u32 cbv_count = 0;
        u32 srv_count = 0;
        u32 uav_count = 0;
        u32 unbounded_table_count = 0;

        for (size_t parameter_idx = 0; parameter_idx < ARRAY_SIZE(desc.entries); parameter_idx++) {
            const auto& entry = desc.entries[parameter_idx];

            if (entry.type == ResourceLayoutEntryType::INVALID) {
                root_signature_desc.NumParameters = parameter_idx;
                break;
            }
            D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_idx];
            parameter.ShaderVisibility = dx12::to_d3d_visibility(entry.visibility);
            if (entry.type == ResourceLayoutEntryType::CONSTANT_BUFFER) {
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                parameter.Descriptor = D3D12_ROOT_DESCRIPTOR{ cbv_count++ };
            }
            else if (entry.type == ResourceLayoutEntryType::CONSTANT) {
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                parameter.Constants = D3D12_ROOT_CONSTANTS{ 1, cbv_count++ };
            }
            else if (entry.type == ResourceLayoutEntryType::TABLE) {
                D3D12_ROOT_DESCRIPTOR_TABLE table = {};
                D3D12_DESCRIPTOR_RANGE d3d_ranges[ARRAY_SIZE(entry.ranges)] = {};

                ASSERT(entry.ranges[0].usage != ResourceLayoutRangeUsage::UNUSED);

                // Check if the table has a single range with an unbounded counter, add that parameter and continue
                if (entry.ranges[0].count == ResourceLayoutRangeDesc::UNBOUNDED_COUNT) {
                    constexpr u32 unbounded_table_size = 4096;
                    d3d_ranges[0].BaseShaderRegister = 0;
                    d3d_ranges[0].NumDescriptors = unbounded_table_size;
                    d3d_ranges[0].RangeType = entry.ranges[0].usage == ResourceLayoutRangeUsage::READ ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                    d3d_ranges[0].RegisterSpace = unbounded_table_count++;

                    table.pDescriptorRanges = d3d_ranges;
                    table.NumDescriptorRanges = 1;
                    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    parameter.DescriptorTable = table;
                    continue;
                }

                // For bounded tables, we loop through and add ranges for each table entry
                for (size_t range_idx = 0; range_idx < ARRAY_SIZE(entry.ranges); range_idx++) {
                    const auto& range = entry.ranges[range_idx];
                    if (range.usage == ResourceLayoutRangeUsage::UNUSED) {
                        break;
                    }

                    if (range.usage == ResourceLayoutRangeUsage::READ) {
                        // SRV
                        d3d_ranges[range_idx] = CD3DX12_DESCRIPTOR_RANGE(
                            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                            range.count,
                            srv_count
                        );
                        srv_count += range.count;
                    }
                    else {
                        // UAV
                        d3d_ranges[range_idx] = CD3DX12_DESCRIPTOR_RANGE(
                            D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                            range.count,
                            uav_count
                        );
                        uav_count += range.count;
                    }
                    table.NumDescriptorRanges = range_idx + 1;
                }
                table.pDescriptorRanges = d3d_ranges;
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                parameter.DescriptorTable = table;
            }
        }

        root_signature_desc.pParameters = root_parameters;
        root_signature_desc.NumStaticSamplers = 0; // TODO: Handle samplers lol
        root_signature_desc.pStaticSamplers = nullptr;
        root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3D12RootSignature* root_signature = nullptr;

        ID3DBlob* signature;
        ID3DBlob* error;
        DXCall(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        DXCall(device_context.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

        if (error != nullptr) {
            print_blob(error);
            error->Release();
            throw std::runtime_error("Failed to create root signature");
        }
        signature != nullptr && signature->Release();

        return { static_cast<u32>(root_signatures.push_back(root_signature)) };
    }

    void Renderer::update_buffer(const BufferHandle buffer_id, const void* data, u64 byte_size)
    {
        dx12::Buffer& buffer = buffers[buffer_id];
        dx12::update_buffer(buffer, data, byte_size, current_frame_idx);
    }

    void Renderer::set_active_resource_layout(const ResourceLayoutHandle resource_layout_id)
    {
        ID3D12RootSignature* root_signature = root_signatures[resource_layout_id.idx];
        cmd_list->SetGraphicsRootSignature(root_signature);
    }

    void Renderer::draw_mesh(const MeshHandle mesh_id)
    {
        const dx12::Mesh& mesh = meshes[mesh_id.idx];
        cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd_list->IASetIndexBuffer(&mesh.index_buffer_view);
        cmd_list->IASetVertexBuffers(0, mesh.num_vertex_buffers, mesh.buffer_views);
        cmd_list->DrawIndexedInstanced(mesh.index_count, 1, 0, 0, 0);
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