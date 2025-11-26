#include "../s2h/include/s2h.hlsl"
#include "../s2h/include/s2h_3d.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
	
	// Transform to homogeneous clip space.
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    ContextGather ui;
    s2h_init(ui, pin.PosH.xy);
    s2h_setCursor(ui, float2(600, 400));
    s2h_printTxt(ui, _P, _o, _s, _COLON);

    s2h_setScale(ui, 3.0f);
    s2h_printTxt(ui, _H, _e, _l, _l, _o);
    s2h_printLF(ui);
    s2h_printTxt(ui, _W, _o, _r, _l, _d);
    
    return float4(pin.Color.rgb * (1.0f - ui.dstColor.a) + ui.dstColor.rgb, 1);
}
