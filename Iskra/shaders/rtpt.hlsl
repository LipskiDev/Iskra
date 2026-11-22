// @stages lib

#include "Random.hlsl"

RaytracingAccelerationStructure g_accel : register(t0);
RWTexture2D<float4> g_output : register(u0);
RWTexture2D<float4> g_accumulationTexture : register(u1);

struct RayPayload
{
    float3 throughput;
    float3 radiance;
    
    float3 nextOrigin;
    float3 nextDirection;
    
    uint bounceCount;
    uint randomState;
    bool terminated;
};

struct Sphere
{
    float3 center;
    float radius;
    uint materialIndex;
    uint3 padding;
};

struct RTVertex
{
    float3 position;
    float3 normal;
};

struct TriangleInstanceData
{
    uint vertexOffset;
    uint indexOffset;
    uint materialIndex;
    uint padding;
};

static const uint MATERIAL_LAMBERTIAN = 0;
static const uint MATERIAL_METAL = 1;
static const uint MATERIAL_DIELECTRIC = 2;
static const uint MATERIAL_EMISSIVE = 3;

struct Material
{
    float3 baseColor;
    float metallic;

    float3 emission;
    float roughness;

    float transmission;
    float ior;
    float2 padding;
};

struct SphereAttributes
{
    float3 normal;
};

struct UniformBuffer
{
    float3 cameraPosition;
    float tanHalfFovY;
    float3 cameraDirection;
    int frame;
    int seed;
    int3 padding;
};

StructuredBuffer<Sphere> spheres : register(t1);
StructuredBuffer<RTVertex> vertices : register(t2);
StructuredBuffer<uint> indices : register(t3);
StructuredBuffer<Material> materials : register(t4);
StructuredBuffer<TriangleInstanceData> triangleInstances : register(t5);
ConstantBuffer<UniformBuffer> uniformBuffer : register(b0);

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

[shader("raygeneration")]
void RayGen()
{
    float2 launchIndex = DispatchRaysIndex().xy;
    float2 launchDims = DispatchRaysDimensions().xy;
  
    uint seed = xxhash32(uint3(uint2(launchIndex.xy), uniformBuffer.frame));
  
    int sampleCount = 1;
    int bounceCount = 8;
  
    float4 accumulatedColor = float4(0.0, 0.0, 0.0, 0.0);
  
    for (int i = 0; i < sampleCount; ++i)
    {
        RayDesc ray;
        ray.Origin = uniformBuffer.cameraPosition;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;
        
        float r0 = Random(seed + i) - 0.5f;
        float r1 = Random(seed + i * 2) - 0.5f;
        
        float2 subpixelJitter = uniformBuffer.frame == 0 ? float2(0.0, 0.0) : float2(r0, r1);
        
        float2 pixel = float2(launchIndex) + subpixelJitter + 0.5;
        float2 ndc = pixel / float2(launchDims) * 2.0 - 1.0;
        ndc.y *= -1.0;
        float aspect = float(launchDims.x) / float(launchDims.y);
        float3 forward = normalize(uniformBuffer.cameraDirection);
        float3 worldUp = float3(0.0, 1.0, 0.0);
        float3 right = normalize(cross(forward, worldUp));
        float3 up = cross(right, forward);
        
        ray.Origin = uniformBuffer.cameraPosition;
        ray.Direction = normalize(
            forward
            + right * (ndc.x * aspect * uniformBuffer.tanHalfFovY)
            + up * (ndc.y * uniformBuffer.tanHalfFovY)
        );
        
        RayPayload payload;
        payload.throughput = float3(1.0, 1.0, 1.0);
        payload.radiance = float3(0.0, 0.0, 0.0);
        payload.nextOrigin = ray.Origin;
        payload.nextDirection = ray.Direction;
        payload.bounceCount = 0;
        payload.randomState = seed;
        payload.terminated = false;
    
        for (int j = 0; j < bounceCount; ++j)
        {
            TraceRay(g_accel, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
            
            if (payload.terminated)
            {
                break;
            }
            
            ray.Origin = payload.nextOrigin;
            ray.Direction = payload.nextDirection;
        }
        accumulatedColor += float4(payload.radiance, 1.0);
    }
  
    float4 previousColor = g_accumulationTexture[launchIndex];
    float4 currentColor = accumulatedColor / float(sampleCount);
    int accumulatedSamples = uniformBuffer.frame + 1;
    float4 newColor = previousColor + (currentColor - previousColor) / float(accumulatedSamples);
    g_accumulationTexture[launchIndex] = newColor;
  
    float4 finalColor = newColor;
    float3 mappedColor = finalColor.rgb / (1.0f + finalColor.rgb);
    g_output[launchIndex] = float4(LinearToSRGB(mappedColor), finalColor.a);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    float3 dir = normalize(WorldRayDirection());
  
    float t = saturate(dir.y * 0.5f + 0.5f);
    float3 horizonColor = float3(0.5, 0.7, 1.0);
    float3 zenithColor = float3(0.3, 0.3, 0.5);
  
    payload.radiance += payload.throughput * float3(lerp(horizonColor, zenithColor, t));
    payload.terminated = true;
    payload.bounceCount = 10;
}

float Schlick(float cosTheta, float ior)
{
    float r0 = (1.0 - ior) / (1.0 + ior);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}

void ShadeSurface(inout RayPayload payload, Material material, float3 position,
                  float3 outwardNormal, uint randomKey)
{
    float3 normal = normalize(outwardNormal);
    bool frontFace = dot(WorldRayDirection(), normal) < 0.0;
    normal = frontFace ? normal : -normal;
    payload.bounceCount += 1;
    float emissionStrength = max(material.emission.r, max(material.emission.g, material.emission.b));

    if (emissionStrength > 0.0f)
    {
        payload.radiance += payload.throughput * material.emission;
        payload.terminated = true;
        return;
    }

    float transmissionWeight = saturate(material.transmission);
    float metallicWeight = saturate(material.metallic) * (1.0 - transmissionWeight);

    float diffuseWeight = (1.0 - saturate(material.metallic)) * (1.0 - transmissionWeight);

    payload.throughput *= material.baseColor;

    float totalWeight = diffuseWeight + metallicWeight + transmissionWeight;

    if (totalWeight <= 0.0f)
    {
        payload.terminated = true;
        return;
    }

    diffuseWeight /= totalWeight;
    metallicWeight /= totalWeight;
    transmissionWeight /= totalWeight;

    float choice = Random(payload.randomState);

    if (choice < diffuseWeight)
    {
        payload.nextDirection = LambertianHemisphereDirection(normal, payload.randomState);

    }
    else if (choice < diffuseWeight + metallicWeight)
    {
        float3 reflected =
        reflect(normalize(WorldRayDirection()), normal);

        float3 diffuseDirection =
        LambertianHemisphereDirection(normal, payload.randomState);

        payload.nextDirection = normalize(
        lerp(reflected, diffuseDirection, material.roughness));
    }
    else
    {
        float3 direction = normalize(WorldRayDirection());
        float eta = frontFace ? rcp(material.ior) : material.ior;

        float cosTheta = min(dot(-direction, normal), 1.0f);
        float sinTheta =
        sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));

        bool cannotRefract = eta * sinTheta > 1.0f;
        float reflectProbability = Schlick(cosTheta, material.ior);

        if (cannotRefract ||
        Random(payload.randomState) < reflectProbability)
        {
            payload.nextDirection = reflect(direction, normal);
        }
        else
        {
            payload.nextDirection =
            refract(direction, normal, eta);
        }
    }
    
    float offsetSide = dot(payload.nextDirection, normal) >= 0.0f ? 1.0f : -1.0f;

    payload.nextOrigin = position + normal * (offsetSide * 0.001f);
    payload.randomState = xxhash32(uint3(payload.randomState, payload.bounceCount, randomKey));
}

[shader("closesthit")]
void SphereClosestHit(inout RayPayload payload, in SphereAttributes attributes)
{
    Sphere sphere = spheres[PrimitiveIndex()];
    float3 position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 normal = normalize(mul(attributes.normal, (float3x3) ObjectToWorld3x4()));
    ShadeSurface(payload, materials[sphere.materialIndex], position, normal,
                 PrimitiveIndex());
}

[shader("closesthit")]
void TriangleClosestHit(inout RayPayload payload,
                        in BuiltInTriangleIntersectionAttributes attributes)
{
    TriangleInstanceData instance = triangleInstances[InstanceID()];
    uint triangleIndex = instance.indexOffset + PrimitiveIndex() * 3;
    uint i0 = instance.vertexOffset + indices[triangleIndex];
    uint i1 = instance.vertexOffset + indices[triangleIndex + 1];
    uint i2 = instance.vertexOffset + indices[triangleIndex + 2];

    float3 barycentrics = float3(1.0f - attributes.barycentrics.x - attributes.barycentrics.y,
                                 attributes.barycentrics.x, attributes.barycentrics.y);
    float3 objectNormal = vertices[i0].normal * barycentrics.x
                        + vertices[i1].normal * barycentrics.y
                        + vertices[i2].normal * barycentrics.z;
    float3 worldNormal = normalize(mul(objectNormal, (float3x3) ObjectToWorld3x4()));
    float3 position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    ShadeSurface(payload, materials[instance.materialIndex], position, worldNormal,
                 PrimitiveIndex() ^ (InstanceID() * 0x9e3779b9u));
}

[shader("intersection")]
void SphereIntersection()
{
    Sphere sphere = spheres[PrimitiveIndex()];
  
  
    float3 origin = ObjectRayOrigin() - sphere.center;
    float3 direction = ObjectRayDirection();
  
    float a = dot(direction, direction);
    float halfB = dot(origin, direction);
    float c = dot(origin, origin) - sphere.radius * sphere.radius;
    float discriminant = halfB * halfB - a * c;

    if (discriminant < 0.0)
    {
        return;
    }
    float root = sqrt(discriminant);
    float nearT = (-halfB - root) / a;
    float farT = (-halfB + root) / a;
  
    SphereAttributes attributes;
  
    if (nearT >= RayTMin() && nearT <= RayTCurrent())
    {
        attributes.normal = normalize((origin + nearT * direction) / sphere.radius);
    
        if (ReportHit(nearT, 0u, attributes))
        {
            return;
        }
    }
  
    if (farT >= RayTMin() && farT <= RayTCurrent())
    {
        attributes.normal = normalize((origin + farT * direction) / sphere.radius);
        ReportHit(farT, 0u, attributes);
    
    }

}
