#include "TestRenderer.h"
#include "Demos/DemoFactory.h"
#include "imgui/backends/imgui_impl_win32.h"

using namespace raphael;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool resized = false;

LRESULT TestRenderer::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (m_initialized)
            resized = (wParam != SIZE_MINIMIZED);
		break;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    if (resized)
    {
		m_demo->Resize(LOWORD(lParam), HIWORD(lParam));
        resized = false;
    }

    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool TestRenderer::Initialize()
{
    m_demo = CreateDemo(DemoType::GBuffer);
    m_demo->SetDemoType(DemoType::GBuffer);
    m_window.setMessageCallback([this](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return HandleMessage(hwnd, msg, wParam, lParam);
    });

	m_window.createWindow();
    if (!m_demo->Initialize(m_window.getWindowInfo()))
    {
        m_demo.reset();
        return false;
    }

    m_initialized = true;
    return true;
}

void TestRenderer::Run()
{
    if (!m_demo)
        return;

    m_demo->Render();

    if (auto next = m_demo->TakeSwitchRequest())
        SwitchDemo(*next);
}

void TestRenderer::SwitchDemo(DemoType type)
{
    // The current demo owns its DeviceDx12/SwapChain/ImGui. Shutdown() waits for
    // the GPU to go idle before its resources are released, so tearing the demo
    // down and rebuilding on the same window is safe between frames.
    m_demo->Shutdown();
    m_demo.reset();

    m_demo = CreateDemo(type);
    m_demo->SetDemoType(type);
    if (!m_demo->Initialize(m_window.getWindowInfo()))
        m_demo.reset();
}

void TestRenderer::Shutdown()
{
    if (m_demo)
    {
        m_demo->Shutdown();
        m_demo.reset();
    }
}
