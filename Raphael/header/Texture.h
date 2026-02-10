#pragma once

#include "D3D12CommonHeaders.h"

class Texture
{
public:
	Texture(std::string name, std::wstring filename)
		: Name(name)
		, Filename(filename)
	{
	}

	std::string Name;
	std::wstring Filename;

	ComPtr<ID3D12Resource> Resource = nullptr;
	ComPtr<ID3D12Resource> UploadHeap = nullptr;
	
	D3D12_CPU_DESCRIPTOR_HANDLE SrvCpuHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE SrvGpuHandle = {};
};

