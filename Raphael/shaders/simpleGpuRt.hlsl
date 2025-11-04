// Simple Ray Tracer in HLSL
// One sphere, one plane and one point light source
// Shadow Checking

cbuffer SceneCB : register(b0)
{
    float4x4 viewProjInverse;
    float4 cameraPos; // w component unused
    float4 sphereData; // xyz: center, w: radius
    float4 planeData; // xyz: normal, w: distance from origin (equation: dot(normal, P) + d = 0)
    float4 lightPos; // w component unused
    float4 sphereColor; // rgb: color, a: unused
    float4 planeColor; // rgb: color, a: unused
    
};

// I will use these later for adding textures
//Texture2D tex00 : register(t0);
//SamplerState sam00 : register(s0);

// -----------------------------------------------------------
// Vertex shader input/output
// Fullscreen draw -> we provide clip-space directly
// -----------------------------------------------------------
struct VSIn
{
    float3 pos : POSITION; // clip-space vertex (-1..1)
};

struct VSOut
{
    float4 pos : SV_Position; // what rasterizer uses 
    float2 ndcPos : TEXCOORD0; // normalized device coords (0..1)
};

// Vertex Shader
//   Simply passes through clip-space position and computes NDC coords
VSOut VS_Main(VSIn input)
{
    VSOut output;
    output.pos = float4(input.pos, 1.0f);
    
    // Since we are already in clip-space, NDC coords are just xy components
    // Send this to the pixel shader for ray generation
    output.ndcPos = input.pos.xy; // * 0.5f + float2(0.5f, 0.5f); // map from -1..1 to 0..1 
    return output;
};

// Helper function to compute ray-sphere intersection
// Sphere is defined as: |P - C|^2 = r^2, where P is a point on the sphere, C is center, r is radius
// P(t) = O + t * D, where t is the parameter along the ray, O is ray origin, D is ray direction
// Solve quadratic equation for intersection. Only care about nearest positive t.
// Returns distance to intersection
bool IntersectSphere(float3 rayOrigin, float3 rayDir, float maxT, out float t)
{
    float3 oc = rayOrigin - sphereData.xyz; // oc is vector from sphere center to ray origin
    float a = dot(rayDir, rayDir);
    float b = 2.0f * dot(oc, rayDir);
    float c = dot(oc, oc) - sphereData.w * sphereData.w;
    float discriminant = b * b - 4 * a * c;
    
    t = 0.0f;
    
    if (discriminant < 0)
    {
        return false;
    }
    else
    {
        float sqrtDisc = sqrt(discriminant);
        float t0 = (-b - sqrtDisc) / (2.0f * a);
        float t1 = (-b + sqrtDisc) / (2.0f * a);
        
        // We want the nearest positive t
        if (t0 > 0 && t0 < maxT)
        {
            t = t0;
            return true;
        }
        else if (t1 > 0 && t1 < maxT)
        {
            t = t1;
            return true;
        }
        else
        {
            return false;
        }
    }
}

// Helper function to compute ray-plane intersection
// Plane is defined as: dot(N, P) + d = 0, where N is normal, d is distance from origin
// P(t) = O + t * D, where t is the parameter along the ray, O is ray origin, D is ray direction
bool IntersectPlane(float3 rayOrigin, float3 rayDir, float maxT, out float t)
{
    float denom = dot(planeData.xyz, rayDir);
    t = 0.0f;
    
    // If denom is near zero, ray is parallel to plane
    if (abs(denom) > 1e-6)
    {
        float numer = - (dot(planeData.xyz, rayOrigin) + planeData.w);
        float t0 = numer / denom;
        if (t0 > 0 && t0 < maxT)
        {
            t = t0;
            return true;
        }
    }
    return false;
}

// Calculate sphere normal
float3 SphereNormal(float3 p, float4 sphere)
{
    return normalize(p - sphere.xyz);
}

// Calculate plane normal
float3 PlaneNormal(float4 plane)
{
    return normalize(plane.xyz);
}

// Shadow check
// Cast a ray from point to light, see if it hits anything
// If it hits something before reaching light, point is in shadow
// Only check sphere, since plane is infinite will often self-shadow everything, so skip it
bool IsInShadow(float3 p, float3 lightDir, float lightDistance)
{
    // Offset origin to avoid self-intersection acne
    // The offset pushes the ray origin slightly away from the surface ttowards the light
    float3 rayOrigin = p + lightDir * 0.01;
    
    float t;
    if (IntersectSphere(rayOrigin, lightDir, lightDistance, t))
    {
        return true; // In shadow
    }
    return false; // Not in shadow
}

// Shading function
// We use ambient + diffuse shading model (Lambert) + specular + shadowing
float4 Shade(float3 p, float3 normal, float4 baseColor, float3 camPos)
{
    // Light direction
    float3 lightDir = normalize(lightPos.xyz - p);
    
    // View direction
    float3 viewDir = normalize(camPos - p);
    
    float ambientStrength = 0.15f;
    float diffuseStrength = 0.8f;
    float specularStrength = 0.5f;
    float shininess = 32.0f;
    
    // Ambient
    float3 ambient = ambientStrength * baseColor.rgb;
    
    // Diffuse
    float diff = saturate(dot(normal, lightDir));
    float3 diffuse = diffuseStrength * diff * baseColor.rgb;
    
    // Specular
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(saturate(dot(viewDir, reflectDir)), shininess);
    float3 specular = specularStrength * spec * float3(1.0f, 1.0f, 1.0f); // white specular highlight
    
    return float4(ambient + diffuse + specular, 1.0f);
}

// Pixel Shader
// Generates ray from camera through pixel, intersects with scene, shades the hit point
float4 PS_Main(VSOut IN) : SV_TARGET
{
	// Reconstruct world space ray
    // NDC from vertex shader is in -1..1 range
    float2 ndc = IN.ndcPos; // already in -1..1 range
    
    // Homogeneous clip space position
    float4 clipNearest = float4(ndc, 0.0f, 1.0f); // near plane
    float4 clipFar = float4(ndc, 1.0f, 1.0f); // far plane
    
    // Unproject to world space: worldPos = invViewProj * clipPos
    float4 worldNearest = mul(clipNearest, viewProjInverse);
    float4 worldFar = mul(clipFar, viewProjInverse);
    
    // Perspective divide
    worldNearest /= worldNearest.w;
    worldFar /= worldFar.w;
    
    // Ray will start at camera position
    float3 rayOrigin = cameraPos.xyz;
    
    // Ray direction: from camera to worldFar
    float3 rayDir = normalize(worldFar.xyz - rayOrigin);
    
    // Initialize intersection variables
    float tMin = 1e20; // large number
    bool hitSomething = false;
    float3 hitPoint;
    float3 hitNormal;
    float4 hitColor;
    
    // Check intersection with sphere
    {
        float tSphere;
        if (IntersectSphere(rayOrigin, rayDir, tMin, tSphere))
        {
            hitSomething = true;
            tMin = tSphere;
            hitPoint = rayOrigin + rayDir * tSphere;
            hitNormal = SphereNormal(hitPoint, sphereData);
            hitColor = sphereColor;
        }
    }
    
    // Check intersection with plane
    {
        float tPlane;
        if (IntersectPlane(rayOrigin, rayDir, tMin, tPlane))
        {
            hitSomething = true;
            tMin = tPlane;
            hitPoint = rayOrigin + rayDir * tPlane;
            hitNormal = PlaneNormal(planeData);
            hitColor = planeColor;
        }
    }
    
    // If we hit something, shade it
    if (hitSomething)
    {
        // Shadow check
        float3 lightDir = normalize(lightPos.xyz - hitPoint);
        float lightDistance = length(lightPos.xyz - hitPoint);
        bool inShadow = IsInShadow(hitPoint, lightDir, lightDistance);
        
        if (inShadow)
        {
            // Simple shadow: darken the color
            return float4(hitColor.rgb * 0.3f, 1.0f); // darken to 30%
        }
        else
        {
            return Shade(hitPoint, hitNormal, hitColor, cameraPos.xyz);
        }
    }
    
    // Background color (sky-ish)
    return float4(0.4, 0.6, 0.9, 1.0);
}
