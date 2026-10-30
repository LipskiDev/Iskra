// Cube.hlsl

struct VertexInput
{
  float3 position : POSITION;
  float3 color : COLOR;
  uint vertexID : SV_VertexID;
};

struct VertexOutput
{
  float4 position : SV_Position;
  float3 color : COLOR;
};

cbuffer CameraConstants : register(b0)
{
  column_major float4x4 model;
  column_major float4x4 view;
  column_major float4x4 projection;
};

VertexOutput VSMain(VertexInput input)
{
  
  VertexOutput output;
  output.position = mul(projection, mul(view, mul(model, float4(input.position, 1.0f))));
  output.color = input.color;
  return output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
  return float4(input.color, 1.0f);
}