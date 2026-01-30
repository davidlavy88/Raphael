#pragma once

#define USE_RAYTRACING 0

#include "D3D12CommonHeaders.h"
#if USE_RAYTRACING
#include "RayTracerRenderer.h"
#else
#include "BoxRenderer.h"
#endif

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
#if USE_RAYTRACING
    RayTracerRenderer m_renderer;
#else
    BoxRenderer m_renderer;
#endif
};


