#include "sys_info.h"
#include <windows.h>
#include "core/zec_types.h"

namespace zec
{
    SysInfo g_sys_info{ };

    const SysInfo& get_sys_info()
    {
        if (g_sys_info.is_initialized) {
            return g_sys_info;
        }

        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);

        g_sys_info.page_size = u32(system_info.dwPageSize);
        g_sys_info.is_initialized = true;
        return g_sys_info;
    };
}