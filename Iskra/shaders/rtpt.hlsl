// @stages lib

#include "Random.hlsl"

RaytracingAccelerationStructure g_accel : register(t0);
RWTexture2D<float4> g_output : register(u0);
RWTexture2D<float4> g_accumulationTexture : register(u1);

struct RayPayload
{
  float3 color;
  float3 position;
  float3 normal;
  int bounceCount;
};

struct Sphere
{
  float3 center;
  float radius;
};

struct Material
{
  float3 albedo;
};

struct SphereAttributes
{
  float3 normal;
};

struct UniformBuffer
{
  int frame;
  int seed;
};

StructuredBuffer<Sphere> spheres : register(t1);
StructuredBuffer<Material> materials : register(t2);
ConstantBuffer<UniformBuffer> uniformBuffer : register(b0);

float3 LinearToSRGB(float3 linearColor)
{
  linearColor = max(linearColor, 0.0f);
  float3 low = 12.92f * linearColor;
  float3 high = 1.055f * pow(linearColor, 1.0f / 2.4f) - 0.055f;
  return float3(
    linearColor.x <= 0.0031308f ? low.x : high.x,
    linearColor.y <= 0.0031308f ? low.y : high.y,
    linearColor.z <= 0.0031308f ? low.z : high.z);
}

[shader("raygeneration")]
void RayGen()
{
  float2 launchIndex = DispatchRaysIndex().xy;
  float2 launchDims = DispatchRaysDimensions().xy;
  
  uint seed = xxhash32(uint3(uint2(launchIndex.xy), uniformBuffer.frame));
  
  int sampleCount = 1;
  int bounceCount = 4;
  
  float4 accumulatedColor = float4(0.0, 0.0, 0.0, 0.0);
  
  for (int i = 0; i < sampleCount; ++i)
  {
    RayDesc ray;
    ray.Origin = float3(0.0, 0.0, 3.0);
    ray.TMin = 0.001;
    ray.TMax = 1000.0;
      
    RayPayload payload;
    payload.color = float3(1.0, 1.0, 1.0);
    payload.position = ray.Origin;
    payload.bounceCount = 0;
    
    float r0 = Random(seed + i) - 0.5f;
    float r1 = Random(seed + i * 2) - 0.5f;
  
    float2 subpixelJitter = uniformBuffer.frame == 0 ? float2(0.0, 0.0) : float2(r0, r1);
  
    float2 pixelPos = launchIndex + subpixelJitter;
    float2 clipCoords = (pixelPos + 0.5) / launchDims * 2.0 - 1.0;
    float aspectRatio = float(launchDims.x) / float(launchDims.y);
    clipCoords.x *= aspectRatio; // Adjust for aspect ratio
    clipCoords.y *= -1.0;
  
    float2 uv = (float2(launchIndex) + subpixelJitter + 0.5) / float2(launchDims);
    
    ray.Direction = float3(clipCoords, -1.0);
    payload.bounceCount = 0;
    TraceRay(g_accel, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    
    for (int j = 1; j < bounceCount; ++j)
    {
      if (payload.bounceCount >= bounceCount)
        break;
      
      float3 newOrigin = payload.position + 0.001 * payload.normal;
      float3 newDirection = LambertianHemisphereDirection(payload.normal, seed + i + 10 * j);
      
      ray.Origin = newOrigin;
      ray.Direction = newDirection;
      
      payload.bounceCount++;
      TraceRay(g_accel, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    }
    accumulatedColor += float4(payload.color, 1.0);
  }
  
  float4 previousColor = g_accumulationTexture[launchIndex];
  float4 currentColor = accumulatedColor / float(sampleCount);
  int accumulatedSamples = uniformBuffer.frame + 1;
  float4 newColor = previousColor + (currentColor - previousColor) / float(accumulatedSamples);
  g_accumulationTexture[launchIndex] = newColor;
  
  float4 finalColor = newColor;
  g_output[launchIndex] = float4(LinearToSRGB(finalColor.rgb), finalColor.a);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
  float3 dir = normalize(WorldRayDirection());
  
  float t = saturate(dir.y * 0.5f + 0.5f);
  float3 horizonColor = float3(0.5, 0.7, 1.0);
  float3 zenithColor = float3(0.3, 0.3, 0.5);
  
  payload.color *= float3(lerp(horizonColor, zenithColor, t));
  payload.bounceCount = 10;
}

[shader("closesthit")]
void SphereClosestHit(inout RayPayload payload, in SphereAttributes attributes)
{
  Material mat = materials[PrimitiveIndex()];
  float3 worldNormal = normalize(mul(attributes.normal, (float3x3) WorldToObject3x4()));
  payload.color *= float3(mat.albedo);
  payload.position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
  payload.normal = worldNormal;
  
  if (payload.bounceCount >= 10)
  {
    payload.color = float3(0.0, 0.0, 0.0);
  }
}

[shader("intersection")]
void SphereIntersection()
{
  Sphere sphere = spheres[PrimitiveIndex()];
  
  
  float3 origin = ObjectRayOrigin() - sphere.center;
  float3 direction = ObjectRayDirection();
  
  float a = dot(direction, direction);
  float halfB = dot(origin, direction);
  float c = dot(origin, origin) - sphere.radius * sphere.radius;
  float discriminant = halfB * halfB - a * c;

  if (discriminant < 0.0)
  {
    return;
  } 
  float root = sqrt(discriminant);
  float nearT = (-halfB - root) / a;
  float farT = (-halfB + root) / a;
  
  SphereAttributes attributes;
  
  if (nearT >= RayTMin() && nearT <= RayTCurrent())
  {
    attributes.normal = normalize((origin + nearT * direction) / sphere.radius);
    
    if (ReportHit(nearT, 0u, attributes))
    {
      return;
    }
  }
  
  if (farT >= RayTMin() && farT <= RayTCurrent())
  {
    attributes.normal = normalize((origin + farT * direction) / sphere.radius);
    ReportHit(farT, 0u, attributes);
    
  }

}
