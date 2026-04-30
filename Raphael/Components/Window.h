#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <functional>

namespace raphael
{
    struct WindowInfo
    {
        HWND hWnd = nullptr;
        const char* title = "Raphael Engine";
        int width = 1280;
        int height = 720;
    };

    using WndProcCallback = std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>;

    class Window
    {
    public:
        void createWindow();
        void destroyWindow();

		WindowInfo& getWindowInfo() { return m_windowInfo; }
        void setMessageCallback(WndProcCallback callback) { m_messageCallback = std::move(callback); }

    protected:
        virtual LRESULT handleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        static LRESULT WINAPI StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        WindowInfo m_windowInfo;
        WndProcCallback m_messageCallback;
    };
}
