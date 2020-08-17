#pragma once
#include "pch.h"

//  Adapted from:
//	MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/

namespace zec
{
    struct TimeData
    {
        i64 start = 0;
        i64 frequency = 0;
        i64 elapsed = 0;
        i64 delta = 0;

        float elapsed_f = 0.0f;
        float delta_f = 0.0f;

        double elapsed_d = 0.0;
        double delta_d = 0.0;
        double frequency_d = 0.0;

        // -------- unit specific --------
        // seconds

        i64 elapsed_seconds = 0;
        i64 delta_seconds = 0;

        float elapsed_seconds_f = 0.0f;
        float delta_seconds_f = 0.0f;

        double elapsed_seconds_d = 0.0;
        double delta_seconds_d = 0.0;

        // milliseconds

        i64 elapsed_milliseconds;
        i64 delta_milliseconds;

        float elapsed_milliseconds_f = 0.0f;
        float delta_milliseconds_f = 0.0f;

        double elapsed_milliseconds_d = 0.0;
        double delta_milliseconds_d = 0.0;

        // microseconds

        i64 elapsed_microseconds;
        i64 delta_microseconds;

        float elapsed_microseconds_f = 0.0f;
        float delta_microseconds_f = 0.0f;

        double elapsed_microseconds_d = 0.0;
        double delta_microseconds_d = 0.0;
    };

    void init_time_data(TimeData& time_data);

    void update_time_data(TimeData& time_data);
}