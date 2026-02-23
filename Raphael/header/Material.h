#pragma once

#include <string>
#include <DirectXMath.h>

using namespace DirectX;

class Material
{
public:
	Material(std::string name)
		: Name(name)
	{ }

	std::string Name;

	std::string DiffuseTextureName;
	XMFLOAT4 DiffuseAlbedo;
	XMFLOAT3 FresnelR0;
	float Roughness;
};
