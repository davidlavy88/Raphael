#pragma once
#include "Window.h"

namespace raphael
{
	class IDemo
	{
	public:
		virtual ~IDemo() = default;
		virtual bool Initialize(WindowInfo windowInfo) = 0;
		virtual void Shutdown() = 0;
		virtual void Render() = 0;
		virtual void Resize(unsigned int width, unsigned int height) = 0;
	};
}
