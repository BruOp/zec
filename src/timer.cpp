#include "timer.h"
#include <Windows.h>
#include "utils/utils.h"

namespace zec
{
    void init_time_data(TimeData& time_data)
    {
        LARGE_INTEGER large_int;
        Win32Call(QueryPerformanceFrequency(&large_int));
        i64 frequency = large_int.QuadPart;

        Win32Call(QueryPerformanceCounter(&large_int));
        i64 start = large_int.QuadPart;

        time_data.start = start;
        time_data.frequency = frequency;
        time_data.frequency_d = static_cast<double>(frequency);
    }

    // Updates the passed time_data struct
    void update_time_data(TimeData& time_data)
    {
        LARGE_INTEGER large_int;
        Win32Call(QueryPerformanceCounter(&large_int));
        i64 current_time = large_int.QuadPart - time_data.start;
        time_data.delta = current_time - time_data.elapsed;
        time_data.delta_f = static_cast<float>(time_data.delta);
        time_data.delta_d = static_cast<double>(time_data.delta);

        time_data.delta_seconds = time_data.delta / time_data.frequency;
        time_data.delta_seconds_d = time_data.delta / time_data.frequency_d;
        time_data.delta_seconds_f = static_cast<float>(time_data.delta_seconds_d);

        time_data.delta_milliseconds_d = time_data.delta_seconds_d * 1000.0;
        time_data.delta_milliseconds = static_cast<i64>(time_data.delta_milliseconds_d);
        time_data.delta_milliseconds_f = static_cast<float>(time_data.delta_milliseconds_d);

        time_data.delta_microseconds_d = time_data.delta_milliseconds_d * 1000.0;
        time_data.delta_microseconds = static_cast<i64>(time_data.delta_microseconds_d);
        time_data.delta_microseconds_f = static_cast<float>(time_data.delta_microseconds_d);

        time_data.elapsed = current_time;
        time_data.elapsed_f = static_cast<float>(time_data.elapsed);
        time_data.elapsed_d = static_cast<double>(time_data.elapsed);

        time_data.elapsed_seconds = time_data.elapsed / time_data.frequency;
        time_data.elapsed_seconds_d = time_data.elapsed_d / time_data.frequency_d;
        time_data.elapsed_seconds_f = static_cast<float>(time_data.elapsed_seconds_d);

        time_data.elapsed_milliseconds_d = time_data.elapsed_seconds_d * 1000.0;
        time_data.elapsed_milliseconds = static_cast<i64>(time_data.elapsed_milliseconds_d);
        time_data.elapsed_milliseconds_f = static_cast<float>(time_data.elapsed_milliseconds_d);

        time_data.elapsed_microseconds_d = time_data.elapsed_milliseconds_d * 1000.0;
        time_data.elapsed_microseconds = static_cast<i64>(time_data.elapsed_microseconds_d);
        time_data.elapsed_microseconds_f = static_cast<float>(time_data.elapsed_microseconds_d);
    }
}