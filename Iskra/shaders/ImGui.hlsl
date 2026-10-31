cbuffer UiConstants : register(b0)
{
  float2 scale;
  float2 translate;
};

Texture2D uiTexture : register(t0);
SamplerState uiSampler : register(s0);

struct VertexInput
{
  float2 position : POSITION;
  float2 uv : TEXCOORD0;
  float4 color : COLOR0;
};

struct VertexOutput
{
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  float4 color : COLOR0;
};

VertexOutput VSMain(VertexInput input)
{
  VertexOutput output;
  output.position = float4(input.position * scale + translate, 0.0f, 1.0f);
  output.uv = input.uv;
  output.color = input.color;
  return output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
  return input.color * uiTexture.Sample(uiSampler, input.uv);
}
