#include "pch.h"
#include "gfx/ui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "gfx/constants.h"
#include "globals.h"
#include "command_context.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace zec::gfx::dx12;

namespace zec::ui
{
    static bool g_is_ui_initialized = false;

    struct UIState
    {
        DescriptorRangeHandle srv_handle = {};
    };

    static UIState g_ui_state;

    void window_callback(void* context, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
    {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, w_param, l_param);
    }

    void initialize(Window& window)
    {
        IMGUI_CHECKVERSION();
        ASSERT_MSG(!g_is_ui_initialized, "UI can only be initialized once!");
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(window.hwnd);

        window.register_message_callback(window_callback, nullptr);

        DescriptorHeap& srv_heap = g_descriptor_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        g_ui_state.srv_handle = descriptor_utils::allocate_descriptors(srv_heap, 1, &cpu_handle);
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = descriptor_utils::get_gpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, g_ui_state.srv_handle);
        ImGui_ImplDX12_Init(
            g_device,
            RENDER_LATENCY,
            g_swap_chain.format,
            srv_heap.heap,
            cpu_handle,
            gpu_handle);
        g_is_ui_initialized = true;
    }

    void destroy()
    {
        ASSERT(g_is_ui_initialized);
        descriptor_utils::free_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, g_ui_state.srv_handle);
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_is_ui_initialized = false;
    }

    void begin_frame()
    {
        ASSERT(g_is_ui_initialized);
        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void end_frame(const CommandContextHandle handle)
    {
        ASSERT(g_is_ui_initialized);
        ID3D12GraphicsCommandList* cmd_list = cmd_utils::get_command_list(handle);
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list);
    }
}