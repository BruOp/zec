//=================================================================================================
//
//	MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include <minwindef.h>

#include "utils/exceptions.h"
#include "core/array.h"

namespace zec
{
    class Window
    {

    public:

        typedef void (*MsgFunction)(void*, HWND, UINT, WPARAM, LPARAM);

        Window(HINSTANCE hinstance,
            LPCWSTR name = L"Example",
            DWORD style = WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU,
            DWORD exStyle = WS_EX_APPWINDOW,
            i32 clientWidth = 1280,
            i32 clientHeight = 720,
            LPCWSTR iconResource = NULL,
            LPCWSTR smallIconResource = NULL,
            LPCWSTR menuResource = NULL
            //LPCWSTR accelResource = NULL
        );
        ~Window();

        //HWND get_hwnd() const;
        //HMENU get_menu() const;
        //HINSTANCE get_hinstance() const;
        void message_loop();

        bool is_alive() const;
        bool is_minimized() const;
        //BOOL has_focus() const;
        //LONG_PTR get_window_style() const;
        //LONG_PTR get_extended_style() const;
        //void set_window_style(DWORD newStyle);
        //void set_extended_style(DWORD newExStyle);
        //void maximize();
        //void set_position(INT posX, INT posY);
        //void get_position(INT& posX, INT& posY) const;
        void show(bool show = true);
        void set_client_area(i32 clientX, i32 clientY);
        void get_client_area(i32& clientX, i32& clientY) const;
        //void set_title(LPCWSTR title);
        //void set_scroll_ranges(INT scrollRangeX,
        //    INT scrollRangeY,
        //    INT posX,
        //    INT posY);
        void destroy();

        //INT	create_message_box(LPCWSTR message, LPCWSTR title = NULL, UINT type = MB_OK);

        void register_message_callback(MsgFunction msgFunction, void* context);

        HWND get_hwnd() { return hwnd; }
        operator HWND() { return hwnd; }		//conversion operator

        HWND hwnd;			        // The window handle
        HINSTANCE hinstance;		// The HINSTANCE of the application
        std::wstring app_name;		// The name of the application

    private:
        void make_window(LPCWSTR iconResource, LPCWSTR smallIconResource, LPCWSTR menuResource);

        LRESULT	message_handler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        static LRESULT WINAPI wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        DWORD style;			    // The current window style
        DWORD exStyle;		        // The extended window style
        //HACCEL accelTable;		    // Accelerator table handle

        struct Callback
        {
            MsgFunction function;
            void* context;
        };

        VirtualArray<Callback> message_callbacks;		    // Message callback list
    };

}
