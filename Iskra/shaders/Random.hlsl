uint Hash(uint x)
{
  x ^= x >> 16;
  x *= 0x7feb352d;
  x ^= x >> 15;
  x *= 0x846ca68b;
  x ^= x >> 16;
  return x;
}

// Generate a random float in the range [0, 1) based on a seed
float Random(uint seed)
{
  return float(Hash(seed)) / float(0xFFFFFFFF);
}

uint xxhash32(uint3 input)
{
  uint h32 = 0x811c9dc5 ^ (input.x * 0x01000193);
  h32 = (h32 ^ (input.y * 0x01000193)) * 0x01000193;
  h32 = (h32 ^ (input.z * 0x01000193)) * 0x01000193;
  return h32;
}

float3 RandomDirection(uint seed)
{
  float u = Random(seed);
  float v = Random(seed + 1);
  
  float theta = 2.0 * 3.14159265359 * u; // azimuthal angle
  float phi = acos(2.0 * v - 1.0); // polar angle
  
  float x = sin(phi) * cos(theta);
  float y = sin(phi) * sin(theta);
  float z = cos(phi);
  
  return normalize(float3(x, y, z));
}

float3 RandomHemisphereDirection(float3 normal, uint seed)
{
  float3 randomDir = RandomDirection(seed);
  if (dot(randomDir, normal) < 0.0)
  {
    randomDir = -randomDir; // Flip to the other hemisphere
  }
  return randomDir;
}

float3 LambertianHemisphereDirection(float3 normal, uint seed)
{
  float u = Random(seed);
  float v = Random(seed + 1);
  
  float theta = 2.0 * 3.14159265359 * u; // azimuthal angle
  float phi = acos(sqrt(v)); // polar angle for Lambertian distribution
  
  float x = sin(phi) * cos(theta);
  float y = sin(phi) * sin(theta);
  float z = cos(phi);
  
  float3 helper = abs(normal.y) < 0.999f
  ? float3(0.0f, 1.0f, 0.0f)
  : float3(1.0f, 0.0f, 0.0f);
  
  float3 tangent = normalize(cross(helper, normal));
  float3 bitangent = cross(normal, tangent);
  
  return normalize(x * tangent + y * bitangent + z * normal);
}