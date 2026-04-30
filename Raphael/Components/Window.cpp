#include "Window.h"

namespace raphael
{
    LRESULT WINAPI Window::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Window* window = nullptr;

        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            window = static_cast<Window*>(cs->lpCreateParams);
            ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
        else
        {
            window = reinterpret_cast<Window*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
        }

        if (window)
            return window->handleMessage(hWnd, msg, wParam, lParam);

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    void Window::createWindow()
    {
        // Implementation for creating application window
        WNDCLASSEXW wc = {
                sizeof(wc), CS_CLASSDC, StaticWndProc, 0L, 0L,
                GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                L"Raphael Engine", nullptr
        };

        ::RegisterClassExW(&wc);

        // Adjust window size so the client area matches the desired dimensions
        RECT rect = { 0, 0, static_cast<LONG>(m_windowInfo.width), static_cast<LONG>(m_windowInfo.height) };
        ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        auto hwnd = ::CreateWindowW(
            wc.lpszClassName, L"Raphael Engine", WS_OVERLAPPEDWINDOW,
            100, 100, rect.right - rect.left, rect.bottom - rect.top,
            nullptr, nullptr, wc.hInstance, this
        );

        // Show window
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);

        m_windowInfo.hWnd = hwnd;
    }

    void Window::destroyWindow()
    {
        // Implementation for destroying application window
        if (m_windowInfo.hWnd)
        {
            ::DestroyWindow(m_windowInfo.hWnd);
            ::UnregisterClassW(L"Raphael Engine", GetModuleHandle(nullptr));
            m_windowInfo.hWnd = nullptr;
        }
    }

    LRESULT Window::handleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (m_messageCallback)
            return m_messageCallback(hWnd, msg, wParam, lParam);

        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}
