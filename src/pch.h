//=================================================================================================
//
//  Modified version of MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

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

#pragma warning(disable : 4324) // structure was padded due to alignment specifier

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define STRICT              // Use strict declarations for Windows types

// Windows Header Files:
#include <windows.h>
//#include <commctrl.h>
#include <psapi.h>
#include <process.h>
#include <wrl.h>

// DirectX Includes
#ifdef _DEBUG
#ifndef D3D_DEBUG_INFO
#define D3D_DEBUG_INFO
#endif
#endif

#include <d3d12.h>
#include <d3dx12/d3dx12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>

// DirectX Math
//#include <DirectXMath.h>
//#include <DirectXPackedVector.h>
// #include <DirectXCollision.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

#pragma warning(push)
#pragma warning(disable : 4005)
#include <wincodec.h>
#pragma warning(pop)

// C++ Standard Library Header Files
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <complex>
#include <random>
