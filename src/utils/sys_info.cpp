#include "pch.h"
#include "sys_info.h"

namespace zec
{
    SysInfo g_sys_info{ };

    const SysInfo& get_sys_info()
    {
        if (g_sys_info.initialized) {
            return g_sys_info;
        }

        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);

        g_sys_info.page_size = u32(system_info.dwPageSize);
        g_sys_info.initialized = true;
        return g_sys_info;
    };
}