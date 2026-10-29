// Triangle.hlsl

struct VertexOutput
{
  float4 position : SV_Position;
  float3 color : COLOR0;
};

VertexOutput VSMain(uint vertexID : SV_VertexID)
{
  const float2 positions[3] =
  {
    float2(-0.5f, -0.5f),
    float2(0.5f, -0.5f),
    float2(0.0f, 0.5f)
  };
  
  const float3 colors[3] =
  {
    float3(1.0f, 0.0f, 0.0f),
    float3(0.0f, 1.0f, 0.0f),
    float3(0.0f, 0.0f, 1.0f)
  };
  
  VertexOutput output;
  output.position = float4(positions[vertexID], 0.0f, 1.0f);
  output.color = colors[vertexID];
  return output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
  return float4(input.color, 1.0f);
}