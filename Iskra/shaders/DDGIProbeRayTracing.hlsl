//@stages cs

#include "DDGIHelpers.hlsl"

RWTexture2D<float4> rayProbeRadianceDistance : register(u0);
// Previous atlases are read-only throughout this dispatch.
RWTexture2D<float4> irradianceTexture : register(u1);
RWTexture2D<float4> visibilityTexture : register(u2);

#include "DDGIGather.hlsli"

RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<float3> spherePositions : register(t1);

StructuredBuffer<float3> sceneVertexPositions : register(t2);
StructuredBuffer<uint> sceneIndices : register(t3);
StructuredBuffer<uint> instanceIndexOffsets : register(t4);
StructuredBuffer<float2> sceneVertexUVs : register(t5);
// Indexed by the TLAS instance ID, in the same order as instanceIndexOffsets.
StructuredBuffer<int> instanceMaterialIndices : register(t6);

SamplerState BasicSampler : register(s0);

struct Material
{
  float4 baseColorFactor;
  float metallicFactor;
  float roughnessFactor;
  int baseColorTextureIndex;
  int metallicRoughnessTextureIndex;
  int normalTextureIndex;
  int occlusionTextureIndex;
  int emissiveTextureIndex;
  float padding;
};

cbuffer MaterialBuffer : register(b1)
{
  Material materials[1024];
}

Texture2D<float4> textures[] : register(t0, space2);

float3 ComputeShading(int materialIndex, float2 uv, float3 directIrradiance, float3 indirectIrradiance)
{
  float3 albedo = float3(0.8f, 0.8f, 0.8f);
  if (materialIndex >= 0 && materialIndex < 1024)
  {
    albedo = materials[materialIndex].baseColorFactor.rgb;
    uint albedoTextureIndex = materials[materialIndex].baseColorTextureIndex;
    float3 albedoTextureSample = textures[NonUniformResourceIndex(albedoTextureIndex)].SampleLevel(BasicSampler, uv, 0).rgb;
    albedo *= albedoTextureSample;
  }
  return (albedo / PI) * (directIrradiance + indirectIrradiance);
}

void EvaluateQueryHitPosition(RayQuery<RAY_FLAG_FORCE_OPAQUE> query, out float3 hitPosition, out float3 normal, out float2 hitUV) {
  uint instanceID = query.CommittedInstanceID();
  uint triangleID = query.CommittedPrimitiveIndex();
  float2 bary = query.CommittedTriangleBarycentrics();

  uint firstIndex = instanceIndexOffsets[instanceID] + triangleID * 3;
  float3x4 objectToWorld = query.CommittedObjectToWorld3x4();
  uint3 indices = uint3(sceneIndices[firstIndex], sceneIndices[firstIndex + 1], sceneIndices[firstIndex + 2]);
  float3 v0 = mul(objectToWorld, float4(sceneVertexPositions[indices.x], 1.0f));
  float3 v1 = mul(objectToWorld, float4(sceneVertexPositions[indices.y], 1.0f));
  float3 v2 = mul(objectToWorld, float4(sceneVertexPositions[indices.z], 1.0f));
  hitPosition = v0 * (1.0f - bary.x - bary.y) + v1 * bary.x + v2 * bary.y;
  hitUV = sceneVertexUVs[indices.x] * (1.0f - bary.x - bary.y)
    + sceneVertexUVs[indices.y] * bary.x + sceneVertexUVs[indices.z] * bary.y;
  normal = normalize(cross(v1 - v0, v2 - v0));
}

[numthreads(128, 1, 1)]
void CSMain(
  uint3 groupID : SV_GroupID,
  uint threadIndex : SV_GroupIndex)
{
  float3 rayOrigin = spherePositions[groupID.x];
  float3 rayDirection = FibonacciSphere(threadIndex, 128, randomAngle);
  
  RayDesc ray;
  
  ray.Origin = rayOrigin;
  ray.Direction = rayDirection;
  ray.TMin = 0.001f;
  ray.TMax = 1000.0f;
  
  RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
  query.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
  
  while (query.Proceed())
  {
    
  }
  
  float3 irradiance = float3(0.0f, 0.0f, 0.0f);
  float distance = 1000.0f;
  
  if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
  {
    
    float3 hitPosition, normal;
    float2 hitUV;
    EvaluateQueryHitPosition(query, hitPosition, normal, hitUV);
    distance = query.CommittedRayT();

    if (!query.CommittedTriangleFrontFace())
    {
      rayProbeRadianceDistance[uint2(groupID.x, threadIndex)] = float4(0.0f, 0.0f, 0.0f, -distance);
      return;
    }

    
    float3 lightDir = normalize(toLight);
    float NdotL = saturate(dot(normal, lightDir));
    float3 directIrradiance = 0.0f;
    if (NdotL > 0.0f)
    {
      RayDesc shadowRay;
      shadowRay.Origin = hitPosition + normal * 0.001f;
      shadowRay.Direction = lightDir;
      shadowRay.TMin = 0.001f;
      shadowRay.TMax = 1000.0f;

      RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadowQuery;
      shadowQuery.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, shadowRay);
      while (shadowQuery.Proceed()) {}
      if (shadowQuery.CommittedStatus() == COMMITTED_NOTHING)
        directIrradiance = 3.0f * lightColor.rgb * NdotL;
    }

    // Shadowing gates sunlight only. Even shadowed hits can bounce indirect light.
    float3 indirectIrradiance = ComputeIndirectLighting(hitPosition, normal, -rayDirection);
    int materialIndex = instanceMaterialIndices[query.CommittedInstanceID()];
    irradiance = ComputeShading(materialIndex, hitUV, directIrradiance, indirectIrradiance);
  }
  else
  {
    // Unoccluded environment rays receive sky radiance in linear space.
    irradiance = lightColor.rgb;
  }
  
  uint probeNr = groupID.x;
  uint rayNr = threadIndex;
  rayProbeRadianceDistance[uint2(probeNr, rayNr)] = float4(irradiance, distance);
}
