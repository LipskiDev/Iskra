// Cube.hlsl

struct VertexInput
{
  float3 position : POSITION;
  float3 normal : COLOR;
};

struct VertexOutput
{
  float4 position : SV_Position;
  float3 worldPosition : TEXCOORD0;
  float3 worldNormal : TEXCOORD1;
};

cbuffer CameraConstants : register(b0)
{
  column_major float4x4 model;
  column_major float4x4 view;
  column_major float4x4 projection;
  column_major float4x4 normalMatrix;
  float3 toLight;
};

RaytracingAccelerationStructure scene : register(t0);

VertexOutput VSMain(VertexInput input)
{
  VertexOutput output;
  
  float4 worldPosition = mul(model, float4(input.position, 1.0f));
  
  output.position = mul(projection, mul(view, worldPosition));
  output.worldPosition = worldPosition.xyz;
  output.worldNormal = mul(normalMatrix, float4(input.normal, 0.0f)).xyz;
  
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

float4 PSMain(VertexOutput input) : SV_Target
{
  float3 N = normalize(input.worldNormal);
  
  float3 L = normalize(toLight);
  
  float NdotL = saturate(dot(N, L));
  float visibility = 0.0f;
  
  if (NdotL > 0.0f)
  {
    visibility = TraceVisibility(input.worldPosition, N, L);
  }
  
  float3 albedo = float3(0.8f, 0.8f, 0.8f);
  
  float3 lightIrradiance = float3(3.0f, 3.0f, 3.0f);
  
  float3 directLight = (albedo / 3.141592f) * lightIrradiance * NdotL * visibility;
  
  return float4(directLight, 1.0f);

}