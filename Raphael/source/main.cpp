// Dear ImGui: standalone example application for Windows API + DirectX 12

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_5.h>
#include <tchar.h>
#include <cassert>
#include <memory>
//#include "Renderer.h"
#include "RayTracerRenderer.h"

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif
#include <windowsx.h>
#include <imgui_internal.h>


// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Application class that manages the overall application
class Application
{
public:
    bool Initialize()
    {
        // Make process DPI aware and obtain main monitor scale
        ImGui_ImplWin32_EnableDpiAwareness();
        m_dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

        // Create application window
        if (!CreateAppWindow())
            return false;

        // Initialize D3D12 components
        if (!m_device.Initialize())
        {
            DestroyAppWindow();
            return false;
        }

        if (!m_swapChain.Initialize(m_hwnd, m_device))
        {
            m_device.Shutdown();
            DestroyAppWindow();
            return false;
        }

        if (!m_renderer.Initialize(m_device, m_swapChain, m_hwnd))
        {
            m_swapChain.Shutdown();
            m_device.Shutdown();
            DestroyAppWindow();
            return false;
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup ImGui scaling
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(m_dpiScale);
        style.FontScaleDpi = m_dpiScale;

        // Show window
        ::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(m_hwnd);

        return true;
    }

    void Shutdown()
    {
        m_renderer.Shutdown();
        m_swapChain.Shutdown();
        m_device.Shutdown();
        DestroyAppWindow();
    }

    void Run()
    {
        // Application state
        bool showDemoWindow = true;
        bool showAnotherWindow = false;
        ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        MSG msg = { 0 };

        // Main loop
        while (msg.message != WM_QUIT)
        {
            // Process messages
            if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
            else
            {
                if (m_device.m_appPaused)
                {
                    ::Sleep(100);
				}
                else 
                {
                    // Handle window occlusion
                    if ((m_swapChain.IsOccluded() && m_swapChain.Present(false) == DXGI_STATUS_OCCLUDED) || ::IsIconic(m_hwnd))
                    {
                        ::Sleep(10);
                        continue;
                    }

                    // Wait for swap chain if needed
                    if (HANDLE waitableObject = m_swapChain.GetWaitableObject())
                        ::WaitForSingleObject(waitableObject, INFINITE);

                    // Start ImGui frame
                    m_renderer.NewFrame();

                    // Handle mouse input for 3D scene
                    ImGuiIO& io = ImGui::GetIO();
                    static bool leftMousePressed = false;
                    static bool rightMousePressed = false;
                    static ImVec2 lastMousePos = { 0, 0 };

                    // Check for mouse button press (start of drag)
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
                    {
                        leftMousePressed = true;
                        lastMousePos = io.MousePos;
                        m_renderer.ImGuiOnMouseDown(ImGuiMouseButton_Left, io.MousePos.x, io.MousePos.y);
                    }
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !io.WantCaptureMouse)
                    {
                        rightMousePressed = true;
                        lastMousePos = io.MousePos;
                        m_renderer.ImGuiOnMouseDown(ImGuiMouseButton_Right, io.MousePos.x, io.MousePos.y);
                    }

                    // Check for mouse button release (end of drag)
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        leftMousePressed = false;
                    }
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
                    {
                        rightMousePressed = false;
                    }

                    // Handle mouse movement during drag
                    if (leftMousePressed && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
                    {
                        if (io.MousePos.x != lastMousePos.x || io.MousePos.y != lastMousePos.y)
                        {
                            m_renderer.ImGuiOnMouseMove(ImGuiMouseButton_Left, io.MousePos.x, io.MousePos.y);
                            lastMousePos = io.MousePos;
                        }
                    }
                    if (rightMousePressed && ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantCaptureMouse)
                    {
                        if (io.MousePos.x != lastMousePos.x || io.MousePos.y != lastMousePos.y)
                        {
                            m_renderer.ImGuiOnMouseMove(ImGuiMouseButton_Right, io.MousePos.x, io.MousePos.y);
                            lastMousePos = io.MousePos;
                        }
                    }

                    // Check for keyboard input for camera movement
                    if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))
                        m_renderer.CameraForward();
                    if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))
                        m_renderer.CameraBackward();
                    if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))
                        m_renderer.CameraLeft();
                    if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow))
                        m_renderer.CameraRight();

                    // Main window
                    {
                        static float cameraSpeed = 0.05f;

                        ImGui::Begin("Hello, world!");
                        ImGui::ColorEdit3("clear color", (float*)&clearColor);

                        ImGui::SliderFloat("Camera Speed", &cameraSpeed, 0.0f, 0.4f);
                        m_renderer.SetCameraSpeed(cameraSpeed);

                        ImGui::Text("Pitch(degrees): %.2f, Yaw(degrees): %.2f", m_renderer.GetPitch() * 180.0f / XM_PI , m_renderer.GetYaw() * 180.0f / XM_PI);

                        ImGuiIO& io = ImGui::GetIO();
                        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
                        ImGui::End();
                    }

                    // Render frame
                    float deltaTime = 0.01;
                    m_renderer.Update(deltaTime);
                    m_renderer.Render(clearColor);

                    // Present
                    m_swapChain.Present(true);
                    m_device.IncrementFrameIndex();
                }                
            }            
        }
    }

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                m_device.m_appPaused = true;
            }
            else
            {
                m_device.m_appPaused = false;
			}
			return 0;

        case WM_SIZE:
            if (m_device.GetDevice() != nullptr && wParam != SIZE_MINIMIZED)
            {
                m_swapChain.Resize(LOWORD(lParam), HIWORD(lParam), m_device);
            }
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }

        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

private:
    bool CreateAppWindow()
    {
        WNDCLASSEXW wc = { 
            sizeof(wc), CS_CLASSDC, StaticWndProc, 0L, 0L, 
            GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, 
            L"ImGui Example", nullptr 
        };
        
        ::RegisterClassExW(&wc);
        
        m_hwnd = ::CreateWindowW(
            wc.lpszClassName, L"Dear ImGui DirectX12 Example", WS_OVERLAPPEDWINDOW,
            100, 100, static_cast<int>(1280 * m_dpiScale), static_cast<int>(800 * m_dpiScale),
            nullptr, nullptr, wc.hInstance, this
        );

        return m_hwnd != nullptr;
    }

    void DestroyAppWindow()
    {
        if (m_hwnd)
        {
            ::DestroyWindow(m_hwnd);
            ::UnregisterClassW(L"ImGui Example", GetModuleHandle(nullptr));
            m_hwnd = nullptr;
        }
    }

    static LRESULT WINAPI StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Application* app = nullptr;
        
        if (msg == WM_NCCREATE)
        {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            app = static_cast<Application*>(cs->lpCreateParams);
            ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        else
        {
            app = reinterpret_cast<Application*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
        }

        if (app)
            return app->HandleMessage(hWnd, msg, wParam, lParam);

        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }

private:
    HWND m_hwnd = nullptr;
    float m_dpiScale = 1.0f;
    D3D12Device m_device;
    SwapChain m_swapChain;
    RayTracerRenderer m_renderer;
};

// Entry point
int main(int, char**)
{
    Application app;
    
    if (!app.Initialize())
        return 1;

    app.Run();
    app.Shutdown();

    return 0;
}
