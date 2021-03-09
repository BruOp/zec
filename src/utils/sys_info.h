#pragma once

namespace zec
{
    struct SysInfo
    {
        bool is_initialized = false;
        size_t page_size = 0;
    };

    extern SysInfo g_sys_info;

    const SysInfo& get_sys_info();
}