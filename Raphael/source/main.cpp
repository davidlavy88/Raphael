// Dear ImGui: standalone example application for Windows API + DirectX 12

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#define TEST_RENDERER_INTERFACE 1

#include <tchar.h>
#if TEST_RENDERER_INTERFACE
#include "TestRenderer.h"
#else
#include "Application.h"
#endif
#include <windowsx.h>

// Entry point
int main(int, char**)
{
#if TEST_RENDERER_INTERFACE
    TestRenderer testApp;

    if (testApp.Initialize())
    {
        MSG msg{};
        bool is_running{ true };
        while (is_running)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                is_running &= (msg.message != WM_QUIT);
            }

            testApp.Run();
        }
    }
    testApp.Shutdown();

    return 0;
#else
    Application app;
    
    if (!app.Initialize())
        return 1;

    app.Run();
    app.Shutdown();

    return 0;
#endif
}
