#include "gfx/ui.h"
#include <Windows.h>
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "gfx/constants.h"
#include "window.h"
#include "gfx/rhi.h"

#include "render_context.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace zec::rhi;
using namespace zec::rhi::dx12;

namespace zec::ui
{
    struct UIRenderState
    {
        DescriptorRangeHandle srv_handle;
    };

    void window_callback(void* context, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
    {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, w_param, l_param);
    }

    UIRenderer::~UIRenderer()
    {
        ASSERT(state == nullptr);
    }

    void UIRenderer::init(Renderer& in_renderer, Window& window)
    {
        IMGUI_CHECKVERSION();
        ASSERT_MSG(renderer == nullptr && state == nullptr, "UI can only be initialized once!");

        renderer = &in_renderer;

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(window.hwnd);

        window.register_message_callback(window_callback, nullptr);

        RenderContext& render_context = renderer->get_render_context();
        // TODO: Use allocator for this
        state = new (zec::memory::alloc(sizeof(UIRenderState))) UIRenderState{};
        ASSERT(!is_valid(state->srv_handle));
        state->srv_handle = render_context.descriptor_heap_manager.allocate_descriptors(HeapType::CBV_SRV_UAV, 1);
        ID3D12DescriptorHeap* heap = render_context.descriptor_heap_manager.get_d3d_heaps(HeapType::CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = render_context.descriptor_heap_manager.get_cpu_descriptor_handle(state->srv_handle);
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = render_context.descriptor_heap_manager.get_gpu_descriptor_handle(state->srv_handle);
        ImGui_ImplDX12_Init(
            render_context.device,
            RENDER_LATENCY,
            render_context.swap_chain.format,
            heap,
            cpu_handle,
            gpu_handle);
    }

    void UIRenderer::destroy()
    {
        ASSERT(state != nullptr);
        auto& context = renderer->get_render_context();
        context.descriptor_heap_manager.free_descriptors(renderer->get_current_frame_idx(), state->srv_handle);
        // TODO: Use allocator for this
        zec::memory::free_mem(state);
        state = nullptr;

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void UIRenderer::begin_frame()
    {
        ASSERT(state != nullptr);
        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void UIRenderer::end_frame()
    {
        ASSERT(state != nullptr);
        ImGui::Render();
    }

    void UIRenderer::draw_frame(rhi::CommandContextHandle handle)
    {
        ASSERT(state != nullptr);
        const RenderContext& render_context = renderer->get_render_context();
        ID3D12GraphicsCommandList* cmd_list = get_command_list(render_context, handle);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list);
    }
}