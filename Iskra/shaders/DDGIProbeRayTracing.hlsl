//@stages cs

#include "DDGIHelpers.hlsl"

RWTexture2D<float4> rayProbeRadianceDistance : register(u0);

RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<float3> spherePositions : register(t1);
StructuredBuffer<float3> sceneVertexPositions : register(t2);
StructuredBuffer<uint> sceneIndices : register(t3);
StructuredBuffer<uint> instanceIndexOffsets : register(t4);



[numthreads(128, 1, 1)]
void CSMain(
  uint3 groupID : SV_GroupID,
  uint threadIndex : SV_GroupIndex)
{
  float3 rayOrigin = spherePositions[groupID.x];
  float3 rayDirection = FibonacciSphere(threadIndex, 128, randomAngle);
  
  float2 uv = OctahedralMapEncode(rayDirection);
  
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
    uint instanceID = query.CommittedInstanceID();
    uint triangleID = query.CommittedPrimitiveIndex();
    float2 bary = query.CommittedTriangleBarycentrics();
    
    uint firstIndex = instanceIndexOffsets[instanceID] + triangleID * 3;
    float3x4 objectToWorld = query.CommittedObjectToWorld3x4();
    float3 v0 = mul(objectToWorld, float4(sceneVertexPositions[sceneIndices[firstIndex]], 1.0f));
    float3 v1 = mul(objectToWorld, float4(sceneVertexPositions[sceneIndices[firstIndex + 1]], 1.0f));
    float3 v2 = mul(objectToWorld, float4(sceneVertexPositions[sceneIndices[firstIndex + 2]], 1.0f));
    distance = query.CommittedRayT();
    if (!query.CommittedTriangleFrontFace())
    {
      rayProbeRadianceDistance[uint2(groupID.x, threadIndex)] = float4(0.0f, 0.0f, 0.0f, -distance);
      return;
    }

    float3 hitPosition = v0 * (1.0f - bary.x - bary.y) + v1 * bary.x + v2 * bary.y;
    
    float3 normal = normalize(cross(v1 - v0, v2 - v0));
    
    float3 lightDir = normalize(toLight);
    
    float3 rayOrigin2 = hitPosition + normal * 0.001f; // Offset to avoid self-intersection
    
    RayDesc isShadedRay;
    isShadedRay.Origin = rayOrigin2;
    isShadedRay.Direction = lightDir;
    isShadedRay.TMin = 0.001f;
    isShadedRay.TMax = 1000.0f;
    
    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> isShadedQuery;
    
    isShadedQuery.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, isShadedRay);
    
    while (isShadedQuery.Proceed())
    {
    
    }
    
    if (isShadedQuery.CommittedStatus() == COMMITTED_NOTHING)
    {
      float NdotL = saturate(dot(normal, lightDir));
      float3 albedo = float3(0.8f, 0.8f, 0.8f);
      float3 lightIrradiance = float3(3.0f, 3.0f, 3.0f);
      irradiance = (albedo / 3.141592f) * lightIrradiance * NdotL;

    }
  }
  
  uint probeNr = groupID.x;
  uint rayNr = threadIndex;
  rayProbeRadianceDistance[uint2(probeNr, rayNr)] = float4(irradiance, distance);
}
