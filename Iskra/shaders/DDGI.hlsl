//@stages vs ps

#define DDGI_CONSTANTS_REGISTER b1
#include "DDGIHelpers.hlsl"

struct VertexInput
{
  float3 position : POSITION;
  float3 normal : COLOR;
  float2 uv : TEXCOORD0;
};

struct VertexOutput
{
  float4 position : SV_Position;
  float3 worldPosition : TEXCOORD0;
  float3 worldNormal : TEXCOORD1;
  float2 uv : TEXCOORD2;
};

SamplerState BasicSampler : register(s0);

cbuffer DrawConstants : register(b0)
{
  column_major float4x4 model;
  column_major float4x4 view;
  column_major float4x4 projection;
  column_major float4x4 normalMatrix;
  float3 surfaceToLight;
  float3 cameraPosition;
  uint materialIndex;
  
};

cbuffer DebugConstants : register(b2)
{
  int debugMode;
};  

struct Material
{
  float4 baseColorFactor;
  float metallicFactor;
  float roughnessFactor;
  uint baseColorTextureIndex;
  uint metallicRoughnessTextureIndex;
  uint normalTextureIndex;
  uint occlusionTextureIndex;
  uint emissiveTextureIndex;
};

cbuffer MaterialBuffer : register(b3)
{
  Material materials[1024];
}

RaytracingAccelerationStructure scene : register(t0);

RWTexture2D<float4> irradianceTexture : register(u0);
RWTexture2D<float4> visibilityTexture : register(u1);

Texture2D<float4> textures[] : register(t0, space2);

VertexOutput VSMain(VertexInput input)
{
  VertexOutput output;

  float4 worldPosition = mul(model, float4(input.position, 1.0f));

  output.position = mul(projection, mul(view, worldPosition));
  output.worldPosition = worldPosition.xyz;
  output.worldNormal = mul(normalMatrix, float4(input.normal, 0.0f)).xyz;
  output.uv = input.uv;

  return output;
}

float TraceVisibility(float3 position, float3 normal, float3 toLight)
{
  RayDesc ray;

  ray.Origin = position + normal * 0.001f; // Offset to avoid self-intersection
  ray.Direction = toLight;
  ray.TMin = 0.001f;
  ray.TMax = 1000.0f;

  RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH > query;
  query.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);

  while (query.Proceed())
  {

  }

  return query.CommittedStatus() == COMMITTED_NOTHING ? 1.0f : 0.0f;
}

// The CPU uploads cell-centered probes with Z varying fastest.
float3 ProbeWorldPosition(int3 coordinates)
{
  return probeGridOrigin + (float3(coordinates) + 0.5f) * probeGridSpacing;
}

uint ProbeIndex(int3 coordinates)
{
  uint3 size = uint3(probeGridSize);
  return (uint(coordinates.x) * size.y + uint(coordinates.y)) * size.z + uint(coordinates.z);
}

float2 getProbeUV(float3 direction, uint probeIndex, uint2 dimensions)
{
  uint probesPerRow = uint(probeGridSize.x * probeGridSize.y);
  uint tileSide = dimensions.x / probesPerRow;
  uint2 tile = uint2(probeIndex % probesPerRow, probeIndex / probesPerRow);
  // The one-texel border allows interpolation across the octahedral seam.
  return (float2(tile * tileSide) + 1.0f
    + OctahedralMapEncode(direction) * float(tileSide - 2)) / float2(dimensions);
}

// UAVs cannot use a filtering sampler, so interpolate the four texels explicitly.
float4 SampleProbeAtlas(bool distanceAtlas, float2 uv, uint2 dimensions)
{
  float2 pixel = uv * float2(dimensions) - 0.5f;
  int2 p = int2(floor(pixel));
  float2 alpha = frac(pixel);
  int2 lastPixel = int2(dimensions) - 1;
  int2 p00 = clamp(p, int2(0, 0), lastPixel);
  int2 p10 = clamp(p + int2(1, 0), int2(0, 0), lastPixel);
  int2 p01 = clamp(p + int2(0, 1), int2(0, 0), lastPixel);
  int2 p11 = clamp(p + int2(1, 1), int2(0, 0), lastPixel);
  float4 a, b, c, d;
  if (distanceAtlas)
  {
    a = visibilityTexture[p00]; b = visibilityTexture[p10];
    c = visibilityTexture[p01]; d = visibilityTexture[p11];
  }
  else
  {
    a = irradianceTexture[p00]; b = irradianceTexture[p10];
    c = irradianceTexture[p01]; d = irradianceTexture[p11];
  }
  return lerp(lerp(a, b, alpha.x), lerp(c, d, alpha.x), alpha.y);
}

float3 ComputeIndirectLighting(float3 worldPosition, float3 normal)
{
  if (any(probeGridSize < 1.0f) || any(probeGridSpacing <= 0.0f))
    return 0.0f;

  float3 toCamera = cameraPosition - worldPosition;
  float3 V = toCamera / max(length(toCamera), 1e-6f);
  float minimumSpacing = min(probeGridSpacing.x, min(probeGridSpacing.y, probeGridSpacing.z));
  float3 biasedPosition = worldPosition + (normal * 0.2f + V * 0.8f) * (0.00075f * minimumSpacing);
  float3 gridPosition = (biasedPosition - probeGridOrigin) / probeGridSpacing - 0.5f;
  gridPosition = clamp(gridPosition, 0.0f, probeGridSize - 1.0f);
  int3 baseGridIndices = int3(floor(gridPosition));
  float3 alpha = frac(gridPosition);

  uint2 irradianceDimensions, visibilityDimensions;
  irradianceTexture.GetDimensions(irradianceDimensions.x, irradianceDimensions.y);
  visibilityTexture.GetDimensions(visibilityDimensions.x, visibilityDimensions.y);
  float3 sumIrradiance = 0.0f;
  float sumWeight = 0.0f;
  for (int i = 0; i < 8; ++i)
  {
    int3 offset = int3(i, i >> 1, i >> 2) & 1;
    int3 coordinates = min(baseGridIndices + offset, int3(probeGridSize) - 1);
    uint probeIndex = ProbeIndex(coordinates);
    float3 probePosition = ProbeWorldPosition(coordinates);
    float3 trilinear = lerp(1.0f - alpha, alpha, float3(offset));
    float3 toProbe = probePosition - worldPosition;
    float3 probeDirection = toProbe / max(length(toProbe), 1e-6f);
    float wrap = saturate((dot(probeDirection, normal) + 1.0f) * 0.5f);
    float weight = wrap * wrap + 0.2f;

    float3 probeToPoint = biasedPosition - probePosition;
    float distanceToPoint = length(probeToPoint);
    float3 direction = distanceToPoint > 1e-6f ? probeToPoint / distanceToPoint : normal;
    float2 visibilityUV = getProbeUV(direction, probeIndex, visibilityDimensions);
    float2 moments = SampleProbeAtlas(true, visibilityUV, visibilityDimensions).xy;
    if (distanceToPoint > moments.x)
    {
      float variance = max(moments.y - moments.x * moments.x, 0.0f);
      float difference = distanceToPoint - moments.x;
      float visibility = variance / max(variance + difference * difference, 1e-8f);
      // Keep noisy distance estimates from abruptly eliminating a probe.
      // A small visibility floor trades a little occlusion for stable blending.
      weight *= max(visibility * visibility * visibility, 0.05f);
    }

    // Smoothly suppress weak probes before applying spatial interpolation.
    weight = max(weight, 1e-6f);
    const float crushThreshold = 0.2f;
    float crush = min(weight / crushThreshold, 1.0f);
    weight *= crush * crush;
    weight *= trilinear.x * trilinear.y * trilinear.z;

    // Irradiance uses the surface normal; visibility uses probe-to-surface direction.
    float2 irradianceUV = getProbeUV(normal, probeIndex, irradianceDimensions);
    sumIrradiance += weight * SampleProbeAtlas(false, irradianceUV, irradianceDimensions).rgb;
    sumWeight += weight;
  }
  // The update pass stores cosine-weighted mean radiance.
  return PI * sumIrradiance / max(sumWeight, 1e-8f);
}

float4 PSMain(VertexOutput input) : SV_Target
{
  float3 N = normalize(input.worldNormal);

  float3 L = normalize(surfaceToLight);

  float NdotL = saturate(dot(N, L));
  float visibility = 0.0f;

  if (NdotL > 0.0f)
  {
    visibility = TraceVisibility(input.worldPosition, N, L);
  }
  
  Material mat = materials[materialIndex];

  float4 base = textures[mat.baseColorTextureIndex].Sample(BasicSampler, input.uv);
  float alpha = base.a;
  float3 albedo = base.rgb;
  if (alpha < 0.5f)
  {
    discard;
  }

  float3 lightIrradiance = float3(3.0f, 3.0f, 3.0f);

  float3 directLight = (albedo / 3.141592f) * lightIrradiance * NdotL * visibility;
  float3 indirectLight = (albedo / PI) * ComputeIndirectLighting(input.worldPosition, N);
  
  if (debugMode & 1)
  {
    return float4(directLight, 1.0f);
  }
  else if (debugMode & 2)
  {
    return float4(indirectLight, 1.0f);
  }

  //return float4(indirectLight, 1.0f);
  return float4(directLight + indirectLight, 1.0f);

}
