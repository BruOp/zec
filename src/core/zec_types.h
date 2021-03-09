#pragma once

// Standard int typedefs
#include <stdint.h>
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;
typedef wchar_t wchar;


#define UNCOPIABLE( T ) \
        T(const T&) = delete; \
        T& operator=(const T&) = delete;

#define UNMOVABLE( T ) \
        T(T&&) = delete; \
        T& operator=(T&&) = delete;

