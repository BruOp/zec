#include "pch.h"
#include "window.h"

namespace zec
{
    Window::Window(
        HINSTANCE hinstance,
        LPCWSTR name,
        DWORD style,
        DWORD exStyle,
        i32 client_width,
        i32 client_height,
        LPCWSTR icon_resource,
        LPCWSTR small_icon_resource,
        LPCWSTR menu_resource
        //LPCWSTR accelResource
    ) : hinstance{ hinstance }, app_name{ name }, style{ style }, exStyle{ exStyle }
    {
        if (hinstance == nullptr)
            this->hinstance = GetModuleHandle(nullptr);

        /*INITCOMMONCONTROLSEX cce;
        cce.dwSize = sizeof(INITCOMMONCONTROLSEX);
        cce.dwICC = ICC_BAR_CLASSES | ICC_COOL_CLASSES | ICC_STANDARD_CLASSES | ICC_STANDARD_CLASSES;
        ::InitCommonControlsEx(&cce);*/

        make_window(icon_resource, small_icon_resource, menu_resource);
        set_client_area(client_width, client_height);
    }

    Window::~Window()
    {
        destroy();
    }

    void Window::message_loop()
    {
        MSG msg;

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }

    boolean Window::is_alive() const
    {
        return static_cast<boolean>(::IsWindow(hwnd));
    }

    boolean Window::is_minimized() const
    {
        // what
        return static_cast<boolean>(::IsIconic(hwnd));
    }

    void Window::show(bool to_show)
    {
        int cmd_show = to_show ? SW_SHOW : SW_HIDE;
        ::ShowWindow(hwnd, cmd_show);
    }

    void Window::set_client_area(i32 clientX, i32 clientY)
    {
        RECT rect;
        if (!::SetRect(&rect, 0, 0, clientX, clientY)) {
            throw Win32Exception(::GetLastError());
        }

        BOOL isMenu = (::GetMenu(hwnd) != nullptr);
        // Based on styles and client rect, determine the window size required
        if (!::AdjustWindowRectEx(&rect, style, isMenu, exStyle))
            throw Win32Exception(::GetLastError());

        // Use calculated window size from above to grow/shrink the window (note that NOMOVE is passed, so there's not position change)
        if (!::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE))
            throw Win32Exception(::GetLastError());
    }

    void Window::get_client_area(i32& clientX, i32& clientY) const
    {
        RECT rect;
        if (!::GetClientRect(hwnd, &rect))
            throw Win32Exception(::GetLastError());
        // GetClientRect returns 0 for top and left
        clientX = static_cast<i32>(rect.right);
        clientY = static_cast<i32>(rect.bottom);
    }

    void Window::destroy()
    {
        ::DestroyWindow(hwnd);
        ::UnregisterClass(app_name.c_str(), hinstance);
    }

    void Window::make_window(LPCWSTR icon_resource, LPCWSTR small_icon_resource, LPCWSTR menu_resource)
    {
        HICON h_icon = nullptr;
        if (icon_resource) {
            h_icon = reinterpret_cast<HICON>(::LoadImage(hinstance,
                icon_resource,
                IMAGE_ICON,
                0,
                0,
                LR_DEFAULTCOLOR
            ));
        }

        HICON hSmallIcon = nullptr;
        if (small_icon_resource) {
            h_icon = reinterpret_cast<HICON>(::LoadImage(hinstance,
                small_icon_resource,
                IMAGE_ICON,
                0,
                0,
                LR_DEFAULTCOLOR
            ));
        }

        HCURSOR hCursor = ::LoadCursorW(nullptr, IDC_ARROW);

        // Register the window class
        WNDCLASSEX wc = {
            sizeof(WNDCLASSEX),
            CS_DBLCLKS,
            wnd_proc,
            0,
            0,
            hinstance,
            h_icon,
            hCursor,
            nullptr,
            menu_resource,
            app_name.c_str(),
            hSmallIcon
        };

        if (!::RegisterClassEx(&wc))
            throw Win32Exception(::GetLastError());

        // Create the application's window
        hwnd = ::CreateWindowEx(exStyle,
            app_name.c_str(),
            app_name.c_str(),
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            NULL,
            NULL,
            hinstance,
            (void*)this
        );

        if (!hwnd)
            throw Win32Exception(::GetLastError());
    }

    LRESULT Window::message_handler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        for (u64 i = 0; i < message_callbacks.size; ++i) {
            Callback callback = message_callbacks[i];
            MsgFunction msgFunction = callback.function;
            msgFunction(callback.context, hWnd, uMsg, wParam, lParam);
        }

        switch (uMsg) {
            // Window is being destroyed
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

            // Window is being closed
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        }

        return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    LRESULT __stdcall Window::wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg) {
        case WM_NCCREATE:
        {
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
            return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
        }

        Window* pObj = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        if (pObj)
            return pObj->message_handler(hWnd, uMsg, wParam, lParam);
        else
            return ::DefWindowProc(hWnd, uMsg, wParam, lParam);

    }
}