#ifndef DDGI_CONSTANTS_REGISTER
#define DDGI_CONSTANTS_REGISTER b0
#endif

cbuffer DDGIConstants : register(DDGI_CONSTANTS_REGISTER)
{
  float3 toLight;
  float randomAngle;
  
  uint irradiance; // 0 = irradiance, 1 = visibility
  uint irradianceTextureSize;
  uint irradianceQuadSize;
  float probeHysteresis;
  float probeMaxRayDistance;
  float3 probeGridOrigin;
  float3 probeGridSpacing;
  float3 probeGridSize;
  float4 lightColor;
}


float2 OctahedralMapEncode(float3 direction)
{
  float3 p = direction /
        (abs(direction.x) + abs(direction.y) + abs(direction.z));

  float2 uv = p.xy;

  if (p.z < 0.0)
  {
    float2 signs = float2(
            uv.x >= 0.0 ? 1.0 : -1.0,
            uv.y >= 0.0 ? 1.0 : -1.0);

    uv = (1.0 - abs(uv.yx)) * signs;
  }

  return uv * 0.5 + 0.5;
}

float3 OctahedralMapDecode(float2 uv)
{
  float2 p = uv * 2.0 - 1.0;
  float3 direction = float3(p, 1.0 - abs(p.x) - abs(p.y));
  if (direction.z < 0.0)
  {
    float2 signs = float2(p.x >= 0.0 ? 1.0 : -1.0,
                          p.y >= 0.0 ? 1.0 : -1.0);
    direction.xy = (1.0 - abs(p.yx)) * signs;
  }
  return normalize(direction);
}

uint2 OctahedralTexel(float3 direction, uint width, uint height)
{
  float2 uv = OctahedralMapEncode(direction);
  uint2 size = uint2(width, height);
  return min(uint2(uv * float2(size)), size - 1);
}


float2 normalizedOctahedralCoordinates(int2 fragCoord, uint probeSideLength)
{
  int probeWithBorderSide = probeSideLength + 2;
  float2 octahedralTexelCoordinates = int2((fragCoord.x - 1) % probeWithBorderSide, (fragCoord.y - 1) % probeWithBorderSide);
  
  octahedralTexelCoordinates += float2(0.5f, 0.5f);
  octahedralTexelCoordinates *= (2.0f / float(probeSideLength));
  octahedralTexelCoordinates -= float2(1.0f, 1.0f);
  
  return octahedralTexelCoordinates;
}

static const float PI = 3.14159265359f;
// The swapchain is UNORM, so screen outputs must explicitly encode linear light.
// Never apply this to radiance or irradiance stored in the probe atlases.
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

static const float GOLDEN_ANGLE = 2.39996322972865332f; // 2 * PI * (1 - 1 / PHI)

float3 FibonacciSphere(
    uint index,
    uint sampleCount,
    float randomAngle)
{
  float z = 1.0 - 2.0 * (index + 0.5) / sampleCount;

  float phi = index * GOLDEN_ANGLE + randomAngle;

  float r = sqrt(max(0.0, 1.0 - z * z));

  return float3(
        r * cos(phi),
        r * sin(phi),
        z
    );
}

float3 worldToGridIndices(float3 worldPosition, float3 probeGridOrigin, float3 probeGridSpacing)
{
  return clamp(int3((worldPosition - probeGridOrigin) / probeGridSpacing), 0, int3(probeGridSpacing) - 1);
}

float3 gridIndicesToWorldNoOffsets(int3 gridIndices, float3 probeGridOrigin, float3 probeGridSpacing)
{
  return probeGridOrigin + gridIndices * probeGridSpacing;
}

int probeIndicesToIndex(int3 probeCoordinates, float3 probeGridSize)
{
  return probeCoordinates.x + probeCoordinates.y * int(probeGridSize.x) + probeCoordinates.z * int(probeGridSize.x) * int(probeGridSize.y);
}

float3 gridIndicesToWorld(int3 gridIndices, int probeIndex, float3 probeGridOrigin, float3 probeGridSpacing)
{
  // TODO: move by computed probe offset
  return gridIndicesToWorldNoOffsets(gridIndices, probeGridOrigin, probeGridSpacing);
}
