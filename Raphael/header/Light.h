#pragma once
#include <DirectXMath.h>
#include <string>
using namespace DirectX;

enum class LightType
{
	Directional = 0,
	Point = 1,
	Spot = 2
};

class Light
{
public:
	Light(std::string name) 
		: Name(name)
	{ }
 
	LightType Type;
	std::string Name;

	
	XMFLOAT3 Color;	
	float FalloffStart;
	XMFLOAT3 Direction;
	float FalloffEnd;
	XMFLOAT3 Position;
	float SpotLightIntensity;
};

