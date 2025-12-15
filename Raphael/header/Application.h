#pragma once
#include "D3D12CommonHeaders.h"
#include "BoxRenderer.h"

// Application class that manages the overall application
class Application
{
public:
    bool Initialize();
    void Shutdown();
    void Run();
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool CreateAppWindow();
    void DestroyAppWindow();
    static LRESULT WINAPI StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
    float m_dpiScale = 1.0f;
    D3D12Device m_device;
    SwapChain m_swapChain;
    BoxRenderer m_renderer;
};


