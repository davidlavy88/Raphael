#pragma once
#include "Demos/IDemo.h"
#include "Demos/DemoFactory.h"
#include "Components/Window.h"
#include <memory>

using namespace raphael;

class TestRenderer
{
public:
	bool Initialize();
	void Run();
	void Shutdown();

private:
	LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void SwitchDemo(DemoType type);

private:
	std::unique_ptr<IDemo> m_demo;
	Window m_window;
	bool m_initialized = false;
};

