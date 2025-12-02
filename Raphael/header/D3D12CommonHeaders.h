#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// DirectX12 specific headers
#include <d3d12.h>
#include <dxgi1_6.h>            // Microsoft DirectX Graphics Infrastructure
#include <d3dcompiler.h>        // Contains functions to compile HLSL code at runtime
#include <DirectXMath.h>        // SIMD-friendly C++ types and functions
using namespace DirectX;
#include <DirectXColors.h>

#include "d3dx12.h"             // D3D12 extension library

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <chrono>
#include <memory>
#include <queue>
#include <map>
#include <stdexcept>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

static constexpr int NUM_BACK_BUFFERS = 2;
