#pragma once
#include "Demos/IDemo.h"
#include "Demos/DemoFactory.h"
#include "Components/Window.h"
#include <memory>

class TestRenderer
{
public:
	bool Initialize();
	void Run();
	void Shutdown();

private:
	LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void SwitchDemo(raphael::DemoType type);

private:
	std::unique_ptr<raphael::IDemo> m_demo;
	raphael::Window m_window;
	bool m_initialized = false;
};

