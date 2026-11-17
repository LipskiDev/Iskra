// @stages ms ps

#define DDGI_CONSTANTS_REGISTER b1
#include "DDGIHelpers.hlsl"

struct VertexOutput
{
  float4 position : SV_Position;
  float3 normal : COLOR;
  float2 uv : TEXCOORD;
  nointerpolation uint probeIndex : PROBE_INDEX;
};

RWTexture2D<float4> probeIrradiance : register(u0);
StructuredBuffer<float3> spherePositions : register(t0);

cbuffer CameraConstants : register(b0)
{
  column_major float4x4 view;
  column_major float4x4 projection;
};

// Sphere vertices and indices
static const uint slices = 8;
static const uint stacks = 8;

static const uint vertexCount = (slices + 1) * (stacks + 1);
static const uint triangleCount = 2 * slices * (stacks - 1);

static const float pi = 3.14159265359;

[outputtopology("triangle")]
[numthreads(32, 1, 1)]
void MSMain(
  uint3 groupID : SV_GroupID,
  uint threadIndex : SV_GroupIndex,
  out vertices VertexOutput vertices[154],
  out indices uint3 triangles[224]) {
  
  SetMeshOutputCounts(vertexCount, triangleCount);
  
  float3 center = spherePositions[groupID.x];
 
  for (uint i = threadIndex; i < vertexCount; i += 32)
  {
    uint longitude = i % (slices + 1);
    uint latitude = i / (slices + 1);
    
    float theta = (float) longitude / slices * 2.0 * pi;
    float phi = (float) latitude / stacks * pi;
    
    float3 normal = float3(sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta));
    
    float3 position = center + normal * 0.2; // Unit sphere
    
    vertices[i].position = mul(projection, mul(view, float4(position, 1.0)));
    vertices[i].normal = normal;
    vertices[i].uv = float2((float) longitude / slices, (float) latitude / stacks);
    vertices[i].probeIndex = groupID.x;
  }
  
  static const uint trianglePerSlice = 2 * (stacks - 1);
  
  for (uint i = threadIndex; i < triangleCount; i += 32)
  {
    uint slice = i / trianglePerSlice;
    uint localTriangle = i % trianglePerSlice;

    uint3 indices;

    if (localTriangle == 0)
    {
            // North cap.
      uint ring = (slices + 1) + slice;
      indices = uint3(slice, ring + 1, ring);
    }
    else if (localTriangle == trianglePerSlice - 1)
    {
            // South cap.
      uint ring = (stacks - 1) * (slices + 1) + slice;
      uint pole = stacks * (slices + 1) + slice;
      indices = uint3(ring, ring + 1, pole);
    }
    else
    {
            // Two triangles per quad between interior latitude rings.
      uint middle = localTriangle - 1;
      uint row = 1 + middle / 2;

      uint a = row * (slices + 1) + slice;
      uint b = a + 1;
      uint c = a + (slices + 1);
      uint d = c + 1;

      indices = (middle % 2 == 0)
                ? uint3(a, b, c)
                : uint3(b, d, c);
    }

    triangles[i] = indices;
  }

}

void PSMain(VertexOutput input, out float4 color : SV_Target)
{
  uint width, height;
  probeIrradiance.GetDimensions(width, height);
  uint tileSide = irradianceQuadSize + 2;
  uint probesPerRow = width / tileSide;
  uint2 tile = uint2(input.probeIndex % probesPerRow, input.probeIndex / probesPerRow);
  uint2 texel = OctahedralTexel(normalize(input.normal), irradianceQuadSize, irradianceQuadSize);
  color = float4(probeIrradiance[tile * tileSide + 1 + texel].rgb, 1.0f);
}
