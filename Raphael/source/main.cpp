// Dear ImGui: standalone example application for Windows API + DirectX 12

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp


#include <tchar.h>
#include "Application.h"
#include <windowsx.h>

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
