#version 330
uniform samplerCube texture6; // environment
in vec3 direction;
uniform float roughness;

layout(location = 0) out vec4 out0;

const float PI = 3.14159265358979f;
float radicalInverse_VdC(uint bits)
{
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
vec2 hammersley2d(uint i, uint N)
{
  return vec2(float(i) / float(N), radicalInverse_VdC(i));
}
vec3 ImportanceSampleGGX(vec2 Xi, vec3 n, float roughness)
{
  float a = roughness * roughness;

  float phi = 2.0 * PI * Xi.x;
  float c = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
  float s = sqrt(1.0 - c * c);

  vec3 h;
  h.x = cos(phi) * s;
  h.y = sin(phi) * s;
  h.z = c;

  vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
  vec3 tangent = normalize(cross(up, n));
  vec3 bitangent = cross(n, tangent);
  return normalize(tangent * h.x + bitangent * h.y + n * h.z);
}

float D_ggx(in float roughness, in float NoH)
{
  float a = roughness;
  float a2 = roughness * roughness;
  float cos2 = NoH * NoH;
  float x = a / (cos2 * (a2 - 1) + 1);
  return (1.0f / PI) * x * x;
}

void main()
{
  vec3 n = normalize(direction);
  vec3 r = n;
  vec3 v = r;

  const uint SAMPLE_COUNT = 3000u;
  float weight = 0.0;
  vec3 result = vec3(0.0);
  for (uint i = 0u; i < SAMPLE_COUNT; ++i)
  {
    vec2 Xi = hammersley2d(i, SAMPLE_COUNT);
    vec3 h = ImportanceSampleGGX(Xi, n, roughness);
    vec3 l = normalize(2.0 * dot(v, h) * h - v);

    // to sample mip lod based on roughness:
    float ndoth = dot(n, h);
    float D = D_ggx(roughness, ndoth);
    float hdotv = dot(h, v);
    float pdf = (D * ndoth / (4.0 * hdotv)) + 0.0001;
    float resolution = 2048.0; // resolution of source cubemap
    float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
    float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

    float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

    float ndotl = max(dot(n, l), 0.0);
    if (ndotl > 0.0)
    {
      result += textureLod(texture6, l, mipLevel).rgb * ndotl;
      weight += ndotl;
    }
  }
  result = result / weight;
  out0 = vec4(result, 1.0);
}