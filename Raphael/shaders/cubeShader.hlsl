cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPerFrame : register(b1)
{
    float4x4 gViewProj;
    float3   gEyePosW;
    float    pad0;
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

// Convert an sRGB (display-referred) color to linear. Vertex colors are authored by eye in
// sRGB space, so they must be decoded to linear before the (linear -> sRGB) output encode;
// otherwise the _SRGB render target double-encodes them and they look washed out.
float3 SRGBToLinear(float3 c)
{
    float3 lo = c / 12.92f;
    float3 hi = pow((c + 0.055f) / 1.055f, 2.4f);
    return lerp(hi, lo, step(c, 0.04045f));   // step(c,0.04045)=1 -> pick lo (linear segment)
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Transform from local space -> world space -> clip space
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    vout.Color = float4(SRGBToLinear(vin.Color.rgb), vin.Color.a);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}