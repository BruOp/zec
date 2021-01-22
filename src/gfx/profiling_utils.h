#pragma once
#include "pch.h"
#include "gfx/gfx.h"

#define ZEC_CONCAT_IMPL(x, y) x##y
#define ZEC_CONCAT(x, y) ZEC_CONCAT_IMPL(x, y)

#ifdef USE_D3D_RENDERER
#include "gfx/d3d12/command_context.h"

namespace zec
{
    class ScopedPixCPUEvent
    {
    public:
        ScopedPixCPUEvent(char const* description)
        {
            PIXBeginEvent(0, description);
        };

        ~ScopedPixCPUEvent()
        {
            PIXEndEvent();
        };
    };

    class ScopedPixGPUEvent
    {
    public:
        ScopedPixGPUEvent(char const* description, ID3D12GraphicsCommandList* cmd_list) : cmd_list{ cmd_list }
        {
            PIXBeginEvent(cmd_list, 0, description);
        }
        ScopedPixGPUEvent(char const* description, CommandContextHandle cmd_ctx) : cmd_list{ gfx::dx12::cmd_utils::get_command_list(cmd_ctx) }
        {
            PIXBeginEvent(cmd_list, 0, description);
        };

        ~ScopedPixGPUEvent()
        {
            PIXEndEvent(cmd_list);
        };
    private:
        ID3D12GraphicsCommandList* cmd_list;
    };


#ifndef PROFILE_FRAME
#define PROFILE_FRAME(NAME) OPTICK_FRAME(NAME);
#endif // PROFILE_FRAME

#ifndef PROFILE_EVENT
#define PROFILE_EVENT(NAME) ::zec::ScopedPixCPUEvent ZEC_CONCAT(pix_event_, __LINE__)(NAME); OPTICK_EVENT(NAME);
#endif // PROFILE_EVENT

#ifndef PROFILE_GPU_EVENT
#define PROFILE_GPU_EVENT(NAME, CMD_CTX) \
    ID3D12GraphicsCommandList* cmd_list{ gfx::dx12::cmd_utils::get_command_list(CMD_CTX) }; \
    ::zec::ScopedPixGPUEvent ZEC_CONCAT(pix_event_, __LINE__)(NAME, cmd_list); \
    OPTICK_GPU_CONTEXT(cmd_list); \
    OPTICK_GPU_EVENT(NAME);
#endif // PROFILE_GPU_EVENT

}

#endif // USE_D3D_RENDERER