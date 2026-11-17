//@stages cs

#include "DDGIHelpers.hlsl"

RWTexture2D<float4> irradianceVisibilityImagePrevious : register(u0);
RWTexture2D<float4> radianceDistanceImageCurrent : register(u1);
RWTexture2D<float4> irradianceVisibilityImageNext : register(u2);

uint getProbeIndexFromPixels(uint2 coords, uint probeWithBorderSide, uint fullTextureWidth)
{
  uint probesPerSide = fullTextureWidth / probeWithBorderSide;
  return coords.x / probeWithBorderSide + (coords.y / probeWithBorderSide) * probesPerSide;
}

#define EPSILON 0.0001f

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
  const uint probeSideLength = irradianceQuadSize;
  uint textureWidth, textureHeight;
  irradianceVisibilityImageNext.GetDimensions(textureWidth, textureHeight);
  if (id.x >= textureWidth || id.y >= textureHeight || probeSideLength == 0)
    return;

  const uint probeWithBorderSide = probeSideLength + 2;
  const uint probeLastPixel = probeSideLength + 1;
  uint2 localPixel = id.xy % probeWithBorderSide;
  uint2 sourceLocalPixel = localPixel;

  // Evaluate border texels using their mirrored interior texel. This produces
  // current-frame borders without reading another thread's UAV writes.
  bool borderX = localPixel.x == 0 || localPixel.x == probeLastPixel;
  bool borderY = localPixel.y == 0 || localPixel.y == probeLastPixel;
  if (borderX && borderY)
  {
    sourceLocalPixel.x = localPixel.x == 0 ? probeSideLength : 1;
    sourceLocalPixel.y = localPixel.y == 0 ? probeSideLength : 1;
  }
  else if (borderY)
  {
    sourceLocalPixel.x = probeLastPixel - localPixel.x;
    sourceLocalPixel.y = localPixel.y == 0 ? 1 : probeSideLength;
  }
  else if (borderX)
  {
    sourceLocalPixel.x = localPixel.x == 0 ? 1 : probeSideLength;
    sourceLocalPixel.y = probeLastPixel - localPixel.y;
  }

  uint2 sourceCoords = id.xy - localPixel + sourceLocalPixel;
  uint probeIndex = getProbeIndexFromPixels(sourceCoords, probeWithBorderSide, textureWidth);
  float2 octahedralCoordinates = normalizedOctahedralCoordinates(int2(sourceCoords), probeSideLength);
  float3 texelDirection = OctahedralMapDecode(octahedralCoordinates * 0.5f + 0.5f);
  float4 previousValue = irradianceVisibilityImagePrevious.Load(int2(sourceCoords));
  float4 result = 0.0f;

  const float energyConservationFactor = 0.95f;
  const uint rayCount = 128;
  const uint maxBackfaces = uint(rayCount * 0.1f);
  uint backfaces = 0;

  for (uint rayIndex = 0; rayIndex < rayCount; ++rayIndex)
  {
    float3 rayDirection = FibonacciSphere(rayIndex, rayCount, randomAngle);
    float weight = max(0.0f, dot(texelDirection, rayDirection));
    float4 rayData = radianceDistanceImageCurrent.Load(int2(probeIndex, rayIndex));

    if (rayData.w < 0.0f)
    {
      if (++backfaces > maxBackfaces)
      {
        irradianceVisibilityImageNext[id.xy] = previousValue;
        return;
      }
      continue;
    }

    if (irradiance == 0)
    {
      if (weight >= EPSILON)
        result += float4(rayData.xyz * energyConservationFactor * weight, weight);
    }
    else
    {
      weight = pow(weight, 2.5f);
      if (weight >= EPSILON)
      {
        float distance = min(abs(rayData.w), probeMaxRayDistance);
        result += float4(float3(distance, distance * distance, 0.0f) * weight, weight);
      }
    }
  }

  if (result.w <= EPSILON)
  {
    irradianceVisibilityImageNext[id.xy] = previousValue;
    return;
  }

  result = float4(result.xyz / result.w, 1.0f);
  // Alpha marks valid history. Blending against cleared distance moments
  // otherwise produces strong, unstable occlusion while probes warm up.
  float historyWeight = previousValue.w > 0.0f ? saturate(probeHysteresis) : 0.0f;
  irradianceVisibilityImageNext[id.xy] = float4(lerp(result.xyz, previousValue.xyz, historyWeight), 1.0f);
}

