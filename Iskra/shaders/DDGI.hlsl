//@stages vs ps

#define DDGI_CONSTANTS_REGISTER b1
#include "DDGIHelpers.hlsl"

struct VertexInput
{
  float3 position : POSITION;
  float3 normal : COLOR;
  float2 uv : TEXCOORD0;
  float3 tangent : TANGENT;
  float3 bitangent : BITANGENT;
};

struct VertexOutput
{
  float4 position : SV_Position;
  float3 worldPosition : TEXCOORD0;
  float3 worldNormal : TEXCOORD1;
  float2 uv : TEXCOORD2;
  float3 tangent : TANGENT;
  float3 bitangent : BITANGENT;
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
  output.tangent = mul(normalMatrix, float4(input.tangent, 0.0f)).xyz;
  output.bitangent = mul(normalMatrix, float4(input.bitangent, 0.0f)).xyz;

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

#include "DDGIGather.hlsli"

float4 PSMain(VertexOutput input) : SV_Target
{
  Material mat = materials[materialIndex];
  float3 geometricNormal = normalize(input.worldNormal);
  float3 T = normalize(input.tangent - geometricNormal * dot(input.tangent, geometricNormal));
  float handedness = dot(cross(geometricNormal, T), input.bitangent) < 0.0f ? -1.0f : 1.0f;
  float3 B = normalize(cross(geometricNormal, T)) * handedness;

  float3 tangentNormal = textures[mat.normalTextureIndex].Sample(BasicSampler, input.uv).rgb;
  tangentNormal = tangentNormal * 2.0f - 1.0f;

  float3 N = normalize(
    tangentNormal.x * T +
    tangentNormal.y * B +
    tangentNormal.z * geometricNormal);
    
  float3 L = normalize(surfaceToLight);

  float NdotL = saturate(dot(N, L));
  float visibility = 0.0f;

  if (NdotL > 0.0f)
  {
    visibility = TraceVisibility(input.worldPosition, N, L);
  }
  

  float4 base = textures[mat.baseColorTextureIndex].Sample(BasicSampler, input.uv);
  float alpha = base.a;
  float3 albedo = base.rgb;
  if (alpha < 0.5f)
  {
    discard;
  }

  float3 lightIrradiance = 3.0f * lightColor.rgb;

  float3 directLight = (albedo / 3.141592f) * lightIrradiance * NdotL * visibility;
  float3 indirectLight = (albedo / PI) * ComputeIndirectLighting(input.worldPosition, N, cameraPosition - input.worldPosition);
  
  if (debugMode & 1)
  {
    return float4(LinearToSRGB(directLight), 1.0f);
  }
  else if (debugMode & 2)
  {
    return float4(LinearToSRGB(indirectLight), 1.0f);
  }

  return float4(LinearToSRGB(directLight + indirectLight), 1.0f);
}
