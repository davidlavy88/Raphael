#pragma once
#include "IDemo.h"
#include "Window.h"

using namespace raphael;

class TestRenderer
{
public:
	bool Initialize();
	void Run();
	void Shutdown();

private:
	LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	// TODO: Change this to a smart pointer and have a factory method to create different demos
	IDemo* m_demo = nullptr;
	Window m_window;
	bool m_initialized = false;
};

