#ifndef DDGI_GATHER_HLSLI
#define DDGI_GATHER_HLSLI

// Include after DDGIHelpers and the irradianceTexture/visibilityTexture declarations.
// Both callers read stable atlas history; biasDirection points toward their ray origin.
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

float3 ComputeIndirectLighting(float3 worldPosition, float3 normal, float3 biasDirection)
{
  if (any(probeGridSize < 1.0f) || any(probeGridSpacing <= 0.0f))
    return 0.0f;

  float3 V = biasDirection / max(length(biasDirection), 1e-6f);
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
      weight *= max(visibility * visibility * visibility, 0.05f);
    }

    weight = max(weight, 1e-6f);
    const float crushThreshold = 0.2f;
    float crush = min(weight / crushThreshold, 1.0f);
    weight *= crush * crush;
    weight *= trilinear.x * trilinear.y * trilinear.z;

    float2 irradianceUV = getProbeUV(normal, probeIndex, irradianceDimensions);
    sumIrradiance += weight * SampleProbeAtlas(false, irradianceUV, irradianceDimensions).rgb;
    sumWeight += weight;
  }
  return PI * sumIrradiance / max(sumWeight, 1e-8f);
}

#endif
