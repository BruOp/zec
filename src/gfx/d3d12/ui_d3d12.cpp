#include "pch.h"
#include "gfx/ui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "gfx/constants.h"
#include "globals.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace zec::ui
{
    struct UIState
    {
        dx12::DescriptorHandle srv_handle = {};
    };

    static UIState g_ui_state;

    void window_callback(void* context, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
    {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, w_param, l_param);
    }

    void initialize(Window& window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(window.hwnd);

        window.register_message_callback(window_callback, nullptr);

        dx12::DescriptorHeap& srv_heap = dx12::g_descriptor_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        g_ui_state.srv_handle = dx12::DescriptorUtils::allocate_descriptor(srv_heap, &cpu_handle);
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = dx12::DescriptorUtils::get_gpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, g_ui_state.srv_handle);
        ImGui_ImplDX12_Init(
            dx12::g_device,
            RENDER_LATENCY,
            dx12::g_swap_chain.format,
            srv_heap.heap,
            cpu_handle,
            gpu_handle);
    }

    void destroy()
    {
        dx12::DescriptorUtils::free_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, g_ui_state.srv_handle);
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void begin_frame()
    {
        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void end_frame(ID3D12GraphicsCommandList* cmd_list)
    {
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list);
    }
}