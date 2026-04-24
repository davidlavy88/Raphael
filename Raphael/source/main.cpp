// Dear ImGui: standalone example application for Windows API + DirectX 12

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#define TEST_RENDERER_INTERFACE 1

#include <tchar.h>
#if TEST_RENDERER_INTERFACE
#include "BoxDemo.h"
#else
#include "Application.h"
#endif
#include <windowsx.h>

// Entry point
int main(int, char**)
{
#if TEST_RENDERER_INTERFACE
    BoxDemo testApp;

    if (!testApp.Initialize())
        return 1;

    testApp.Run();
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
