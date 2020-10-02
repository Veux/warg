#version 330
uniform sampler2D texture0; //  albedo or specular color
uniform sampler2D texture1; // emissive
uniform sampler2D texture2; // roughness
uniform sampler2D texture3; // normal
uniform sampler2D texture4; // metalness
uniform sampler2D texture5; // ambient occlusion

uniform samplerCube texture6; // environment
uniform samplerCube texture7; // irradiance
uniform sampler2D texture8;   // brdf_ibl_lut
uniform sampler2D texture9;   // refraction
uniform sampler2D texture10;  // depth of opaque scene
// uniform sampler2D texture11;  // displacement
uniform sampler2D texture12; // water velocity
uniform sampler2D texture13; // depth of self

uniform vec4 texture0_mod;
uniform vec4 texture1_mod;
uniform vec4 texture2_mod;
uniform vec4 texture4_mod;
uniform vec4 texture5_mod;
uniform vec4 texture6_mod;
uniform vec4 texture10_mod;

#define xor ^^
#define MAX_LIGHTS 10
uniform sampler2D shadow_maps[MAX_LIGHTS];
uniform float max_variance[MAX_LIGHTS];
uniform bool shadow_map_enabled[MAX_LIGHTS];
uniform mat4 model;
uniform vec3 camera_forward;
uniform vec3 camera_right;
uniform vec3 camera_up;
uniform mat4 projection;
uniform vec3 additional_ambient;
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
uniform bool discard_on_alpha;
uniform float alpha_albedo_override;
uniform vec2 viewport_size;
uniform float aspect_ratio;
uniform float index_of_refraction;
uniform float refraction_offset_factor;
uniform float water_eps;
uniform float water_scale;
uniform float water_speed;
uniform float water_height_scale;
uniform float water_dist_exp;
uniform float water_dist_scale;
uniform float water_dist_min;
uniform float water_scale2;
uniform float height_scale2;
uniform float water_time2;
uniform float surfdotv_exp;
uniform bool water_use_uv;

struct Light
{
  vec3 position;
  vec3 direction;
  vec3 flux;
  vec3 attenuation;
  vec3 ambient;
  float cone_angle;
  int type;
};

uniform Light lights[MAX_LIGHTS];
uniform int number_of_lights;

in vec3 frag_world_position;
in mat3 frag_TBN;
in vec2 frag_uv;
in vec2 frag_normal_uv;
in vec4 frag_in_shadow_space[MAX_LIGHTS];
in float water_depth;
in float height_variance;

in vec4 indebug;

layout(location = 0) out vec4 out0;

const float PI = 3.14159265358979f;
const float TWOPI = 2.0f * PI;
const int max_lights = MAX_LIGHTS;
vec3[max_lights] frag_in_shadow_space_postw;
vec2[max_lights] variance_depth;

float linearize_depth(float z)
{
  float near = 0.001;
  float far = 100;
  float depth = z * 2.0 - 1.0;
  return (2.0 * near * far) / (far + near - depth * (far - near));
}

float saturate(float x)
{
  return clamp(x, 0, 1);
}
struct Material
{
  vec3 albedo;
  vec3 emissive;
  vec3 normal;
  vec3 environment;
  float roughness;
  float metalness;
  float ambient_occlusion;
};

float chebyshevUpperBound(vec2 moments, float distance, float max_variance)
{
  if (distance <= moments.x)
    return 1.0;
  float variance = moments.y - (moments.x * moments.x);
  variance = max(variance, max_variance);
  float d = distance - moments.x;
  float p_max = variance / (variance + d * d);
  return p_max;
}

void gather_shadow_moments()
{
  frag_in_shadow_space_postw[0] = vec3(frag_in_shadow_space[0].xyz / frag_in_shadow_space[0].w);
  variance_depth[0] = texture2D(shadow_maps[0], frag_in_shadow_space_postw[0].xy).rg;
  frag_in_shadow_space_postw[1] = vec3(frag_in_shadow_space[1].xyz / frag_in_shadow_space[1].w);
  variance_depth[1] = texture2D(shadow_maps[1], frag_in_shadow_space_postw[1].xy).rg;
  frag_in_shadow_space_postw[2] = vec3(frag_in_shadow_space[2].xyz / frag_in_shadow_space[2].w);
  variance_depth[2] = texture2D(shadow_maps[2], frag_in_shadow_space_postw[2].xy).rg;
  frag_in_shadow_space_postw[3] = vec3(frag_in_shadow_space[3].xyz / frag_in_shadow_space[3].w);
  variance_depth[3] = texture2D(shadow_maps[3], frag_in_shadow_space_postw[3].xy).rg;
  frag_in_shadow_space_postw[4] = vec3(frag_in_shadow_space[4].xyz / frag_in_shadow_space[4].w);
  variance_depth[4] = texture2D(shadow_maps[4], frag_in_shadow_space_postw[4].xy).rg;
  frag_in_shadow_space_postw[5] = vec3(frag_in_shadow_space[5].xyz / frag_in_shadow_space[5].w);
  variance_depth[5] = texture2D(shadow_maps[5], frag_in_shadow_space_postw[5].xy).rg;
  frag_in_shadow_space_postw[6] = vec3(frag_in_shadow_space[6].xyz / frag_in_shadow_space[6].w);
  variance_depth[6] = texture2D(shadow_maps[6], frag_in_shadow_space_postw[6].xy).rg;
  frag_in_shadow_space_postw[7] = vec3(frag_in_shadow_space[7].xyz / frag_in_shadow_space[7].w);
  variance_depth[7] = texture2D(shadow_maps[7], frag_in_shadow_space_postw[7].xy).rg;
  frag_in_shadow_space_postw[8] = vec3(frag_in_shadow_space[8].xyz / frag_in_shadow_space[8].w);
  variance_depth[8] = texture2D(shadow_maps[8], frag_in_shadow_space_postw[8].xy).rg;
  frag_in_shadow_space_postw[9] = vec3(frag_in_shadow_space[9].xyz / frag_in_shadow_space[9].w);
  variance_depth[9] = texture2D(shadow_maps[9], frag_in_shadow_space_postw[9].xy).rg;
}

// vec3 F_cook-torrance()
//{
//  η=1+F0−−√1−F0−−√
//  c=v⋅h
//  g=η2+c2−1−−−−−−−−−√
//  FCook−Torrance(v,h)=12(g−cg+c)2(1+((g+c)c−1(g−c)c+1)2)
//}

// http://graphicrants.blogspot.co.uk/2013/08/specular-brdf-reference.html
float D_beckmann(float ndoth, float a2)
{
  // DBeckmann(m) =1πα2(n⋅m)4 exp((n⋅m)2−1α2(n⋅m)2)
  float ndoth2 = ndoth * ndoth;
  float left_denom = PI * a2 * ndoth2 * ndoth2;
  return ((1.0f) / left_denom) * exp((ndoth2 - 1.0f) / (a2 * ndoth2));
}

// http://graphicrants.blogspot.co.uk/2013/08/specular-brdf-reference.html
vec3 F_schlick(vec3 F0, float ndotv)
{
  return F0 + (1 - F0) * pow(1 - ndotv, 5);
}

// includes Cook-Torrance denominator
// http://graphicrants.blogspot.co.uk/2013/08/specular-brdf-reference.html
// above uses the surface normal, below uses the half vector??
//  http://www.trentreed.net/blog/physically-based-shading-and-image-based-lighting/
float G_smith_GGX_denom(float a, float NoV, float NoL)
{
  //                           2(ndotv)
  // GGGX(v) = -------------------------------------------
  //            ndotv + sqrt(a^2 + (1−a^2) * (ndotv)^2)

  //                      1
  // CT denominator:  ------------
  //                  4(n⋅l)(n⋅v)

  // GGGX(V)*GGGX(L)*CTd:
  float a2 = a * a;
  float G_V = NoV + sqrt((NoV - NoV * a2) * NoV + a2);
  float G_L = NoL + sqrt((NoL - NoL * a2) * NoL + a2);
  return 1.0f / (G_V * G_L);
}

// http://graphicrants.blogspot.co.uk/2013/08/specular-brdf-reference.html
// above uses the surface normal, below uses the half vector??
// http://www.trentreed.net/blog/physically-based-shading-and-image-based-lighting/
float G_smith_GGX(float a, float NoV, float NoL)
{
  //                           2(ndotv)
  // GGGX(v) = -------------------------------------------
  //            ndotv + sqrt(a^2 + (1−a^2) * (ndotv)^2)

  float a2 = a * a;

  float GV = (2 * NoV) / (NoV + sqrt(a2 + (1 - a2) * NoV * NoV));
  float GL = (2 * NoL) / (NoL + sqrt(a2 + (1 - a2) * NoL * NoL));

  return GV * GL;
}

// https://learnopengl.com/PBR/Theory
float G_smith_schlick_GGX_direct(float roughness, float ndotv, float ndotl)
{
  float r = (roughness + 1.0);
  float k = r * r / 8.0;
  float V = ndotv / ((ndotv * (1.0 - k)) + k);
  float L = ndotl / ((ndotl * (1.0 - k)) + k);
  return V * L;
}
float G_smith_schlick_GGX_ibl(float roughness, float ndotv, float ndotl)
{
  float k = 0.5f * (roughness * roughness);
  float V = ndotv / ((ndotv * (1.0 - k)) + k);
  float L = ndotl / ((ndotl * (1.0 - k)) + k);
  return V * L;
}

// GGX (Trowbridge-Reitz) [4]
// http://graphicrants.blogspot.co.uk/2013/08/specular-brdf-reference.html
float D_GGX(float a, float ndoth)
{
  // DGGX(h)=         a^2
  //          ----------------------
  //          pi((n⋅h)^2(a^2−1)+1)^2

  float a2 = a * a;
  float x = ((ndoth * ndoth) * (a2 - 1.0)) + 1.0f;
  return a2 / (PI * x * x);
}

// Microfacet Models for Refraction through Rough Surfaces
// Walter et al.
// http://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
// aka Towbridge-Reitz
float D_ggx(in float roughness, in float NoH)
{
  float a = roughness;
  float a2 = roughness * roughness;
  float cos2 = NoH * NoH;
  float x = a / (cos2 * (a2 - 1) + 1);
  return (1.0f / PI) * x * x;
  /*
  // version from the paper, eq 33
  float CosSquared = NoH*NoH;
  float TanSquared = (1.0f - CosSquared)/CosSquared;
  return (1.0f/M_PI) * sqr(alpha/(CosSquared * (alpha*alpha + TanSquared)));
  */
}

float rand(vec2 p)
{
  return fract(sin(dot(p.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 spherical_to_euclidean(float theta, float phi)
{
  return vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(theta));
  // return vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
}
vec2 direction_to_polar(vec3 v)
{
  return vec2(0.5f * PI + atan(-v.y, v.x), asin(-v.z));
}

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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

//
// float random(float x) {
//
//    return fract(sin(x) * 10000.);
//
//}
//
// float noise(vec2 p) {
//
//    return random(p.x + p.y * 10000.);
//
//}
//
// vec2 sw(vec2 p) { return vec2(floor(p.x), floor(p.y)); }
// vec2 se(vec2 p) { return vec2(ceil(p.x), floor(p.y)); }
// vec2 nw(vec2 p) { return vec2(floor(p.x), ceil(p.y)); }
// vec2 ne(vec2 p) { return vec2(ceil(p.x), ceil(p.y)); }
//
// float smoothNoise(vec2 p) {
//
//    vec2 interp = smoothstep(0., 1., fract(p));
//    float s = mix(noise(sw(p)), noise(se(p)), interp.x);
//    float n = mix(noise(nw(p)), noise(ne(p)), interp.x);
//    return mix(s, n, interp.y);
//
//}
//
// float fractalNoise(vec2 p) {
//
//    float x = 0.;
//    x += smoothNoise(p      );
//    x += smoothNoise(p * 2. ) / 2.;
//    x += smoothNoise(p * 4. ) / 4.;
//    x += smoothNoise(p * 8. ) / 8.;
//    x += smoothNoise(p * 16.) / 16.;
//    x += smoothNoise(p * 32.) / 32.;
//    x += smoothNoise(p * 64.) / 64.;
//    x += smoothNoise(p * 128.) / 128.;
//    x += smoothNoise(p * 256.) / 256.;
//    x /= 1. + 1./2. + 1./4. + 1./8. + 1./16.+ 1./32.+ 1./64.+ 1./128.+ 1./256.;
//    //x /= 1. + 1./2.;
//    return x;
//
//}
//
// float movingNoise(vec2 p,float time) {
//
//    float x = fractalNoise(p + time);
//    float y = fractalNoise(p - time);
//    return fractalNoise(p + vec2(x, y));
//
//}
//
//// call this for water noise function
// float nestedNoise(vec2 p,float time)
//{
//  float result;
//  float x = movingNoise(p,time);
//
//
//
//    float y = movingNoise(p + 100.,time);
//    result = movingNoise(p + vec2(x, y),time);
//
//  return result;
//}

vec2 hash(vec2 p)
{
  p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));

  return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float noise_2(vec2 p, float level, float time)
{
  vec2 i = floor(p);
  vec2 f = fract(p);

  vec2 u = f * f * (3.0 - 2.0 * f);
  float t = pow(2., level) * .4 * time;
  mat2 R = mat2(cos(t), -sin(t), sin(t), cos(t));
  if (mod(i.x + i.y, 2.) == 0.)
    R = -R;
  return 2. * mix(mix(dot(hash(i + vec2(0, 0)), (f - vec2(0, 0)) * R), dot(hash(i + vec2(1, 0)), -(f - vec2(1, 0)) * R),
                      u.x),
                  mix(dot(hash(i + vec2(0, 1)), -(f - vec2(0, 1)) * R), dot(hash(i + vec2(1, 1)), (f - vec2(1, 1)) * R),
                      u.x),
                  u.y);
}

float random(vec2 st)
{
  return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 st)
{
  vec2 i = floor(st);
  vec2 f = fract(st);
  float a = random(i);
  float b = random(i + vec2(1.0, 0.0));
  float c = random(i + vec2(0.0, 1.0));
  float d = random(i + vec2(1.0, 1.0));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float Mnoise(vec2 uv, float level, float time)
{
  // return noise(4.*uv);
  return noise_2(2.f * uv, level, time);
  // return -1. + 2.* (1.-abs(noise(uv,time)));  // flame like
  return -1. + 2. * (abs(noise_2(uv, level, time))); // cloud like
}

float turb_orig(vec2 uv, float time)
{
  float f = 0.0;

  float level = 1.;
  mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
  f = 0.5000 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.2500 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.1250 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.0625 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  // f += 0.0314 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  return f / .9375;
}

float turb(vec2 uv, float time)
{
  float f = 0.0;

  float level = 1.;
  mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
  f = 0.5000 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.2500 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.1250 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.0625 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.0314 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.0157 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  return 15 * f;
}

float fbm_h_n(vec2 x, float H, int n)
{
  float G = exp2(-H);
  float f = 1.0;
  float a = 1.0;
  float t = 0.0;
  for (int i = 0; i < n; i++)
  {
    t += a * noise(f * x);
    f *= 2.0;
    a *= G;
  }
  return t;
}

float fbm_h_n_dist_fade(vec2 x, float H, int n)
{
  vec3 partial_d = dFdy(frag_world_position);
  float lenpd = 1 - length(partial_d);
  float G = exp2(-H);
  float f = 1.0;
  float a = 1.0;
  float t = 0.0;
  for (int i = 0; i < n; i++)
  {
    lenpd = pow(lenpd, 2.2500f);
    t += lenpd * a * noise(f * x);
    f *= 2.0;
    a *= G;
  }
  return t;
}

float fbm_n(vec2 uv, int n)
{
  float value = 0.0;
  float amplitude = .5;
  float frequency = 0.;
  for (int i = 0; i < n; i++)
  {
    value += amplitude * noise(uv);
    uv *= 2.;
    amplitude *= .5;
  }
  return value;
}

#define NUM_OCTAVES 4
float fbm(vec2 uv)
{
  float v = 0.0;
  float a = 0.5;
  vec2 shift = vec2(100.0);
  // Rotate to reduce axial bias
  mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.50));
  for (int i = 0; i < NUM_OCTAVES; ++i)
  {
    v += a * noise(uv);
    uv = rot * uv * 2.0 + shift;
    a *= 0.5;
  }
  return v;
}

#define NUM_OCTAVES2 3
float fbm2(vec2 uv)
{
  float v = 0.0;
  float a = 0.5;
  vec2 shift = vec2(100.0);
  // Rotate to reduce axial bias
  mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.50));
  for (int i = 0; i < NUM_OCTAVES2; ++i)
  {
    v += a * noise(uv);
    uv = rot * uv * 2.0 + shift;
    a *= 0.5;
  }
  return v;
}

#define NUM_OCTAVES3 5
float fbm3(vec2 uv)
{
  float value = 0.0;
  float amplitude = .5;
  float frequency = 0.;
  for (int i = 0; i < NUM_OCTAVES3; i++)
  {
    value += amplitude * noise(uv);
    uv *= 2.;
    amplitude *= .5;
  }
  return value;
}

float waterheight(vec2 uv, float time)
{
  // much faster, but worse:
  // return .5f * smoothstep(0.0, 2., fbm3(uv + vec2(.13f * time)) + fbm3(.829f * uv.yx - vec2(.13f * time)));
  vec2 q = vec2(0.);
  q.x = fbm3(uv + .00 * time);
  q.y = fbm3(uv + vec2(1.0));

  vec2 r = vec2(0.);
  r.x = fbm(uv + 1.0 * q + vec2(1.7, 9.2) + 0.15 * time);
  r.y = fbm(uv + 1.0 * q + vec2(8.3, 2.8) + 0.126 * time);
  float f = fbm(uv + r);

  vec3 partial_d = dFdy(frag_world_position);
  float lenpd = length(partial_d);

  float divisor = 65.1f * lenpd;

  divisor = max(1, divisor);
  f = f / divisor;
  // f = 0.2*f;

  float scale = 0.051f;
  uv = scale * uv;
  q.x = fbm(uv + .00 * time);
  q.y = fbm(uv + vec2(1.0));
  r.x = fbm(uv + 1.0 * q + vec2(1.7, 9.2) + scale * 1.515 * time);
  r.y = fbm(uv + 1.0 * q + vec2(8.3, 2.8) + scale * 1.5126 * time);
  f = f + 3.f * fbm2(uv + r);

  return f;
}

float ambient_waves(vec2 p, float timescale, int i)
{
  // turb style:
  return turb(p, timescale * 11 * time);

  // fbm style:
  float water_height = 0.f;
  water_height = water_height + fbm_h_n_dist_fade(p + timescale * vec2(.9 * time, -.7 * time), .825960248501f, i);
  water_height = water_height + fbm_h_n_dist_fade(p + timescale * vec2(-.85 * time, .8 * time), .825930248501f, i);
  return water_height;
}

// terribly slow
float waterheightms(vec2 uv, float time)
{
  // vec3 partial_d = dFdx(frag_world_position);
  vec3 partial_d = dFdy(frag_world_position);
  float lenpd = length(partial_d);

  float offset = 0.25f * lenpd;

  float f0 = waterheight(uv, time);
  vec2 o1 = vec2(0, offset);
  vec2 o2 = vec2(offset, 0);
  vec2 o3 = vec2(0, -offset);
  vec2 o4 = vec2(-offset, 0);

  // float f1 = waterheight(uv+o1,time);
  //    float f2 = waterheight(uv-o2,time);
  // float f3 = waterheight(uv+o3,time);
  //    float f4 = waterheight(uv+o4,time);

  // float f = f0+f1+f2+f3+f4;
  // f = f/5;

  return f0;
}

const int ITER_GEOMETRY = 3;
const int ITER_FRAGMENT = 1;
const float SEA_HEIGHT = 1.5116;
const float SEA_CHOPPY = .1000000100;
const float SEA_SPEED = .31128;
const float SEA_FREQ = .501010011015116;
const vec3 SEA_BASE = vec3(0.0, 0.09, 0.18);
const vec3 SEA_WATER_COLOR = vec3(0.8, 0.9, 0.6) * 0.6;
#define SEA_TIME (1.0 + time * SEA_SPEED)
const mat2 octave_m = mat2(1.6, 1.2, -1.2, 1.6);

// sea
float sea_octave(vec2 uv, float choppy)
{
  uv += noise(uv);
  vec2 wv = 1.0 - abs(sin(uv));
  vec2 swv = abs(cos(uv));
  wv = mix(wv, swv, wv);
  return pow(1.0 - pow(wv.x * wv.y, 0.65), choppy);
}

float map(vec3 p)
{

  float freq = SEA_FREQ;
  float amp = SEA_HEIGHT;
  float choppy = SEA_CHOPPY;
  vec2 uv = p.xz;
  uv.x *= 0.75;

  float d, h = 0.0;
  for (int i = 0; i < ITER_GEOMETRY; i++)
  {
    d = sea_octave((uv + SEA_TIME) * freq, choppy);
    d += sea_octave((uv - SEA_TIME) * freq, choppy);
    h += d * amp;
    uv *= octave_m;
    freq *= 1.9;
    amp *= 0.22;
    choppy = mix(choppy, 1.0, 0.2);
  }
  return p.y - h;
}
float map_detailed(vec3 p)
{
  float freq = SEA_FREQ;
  float amp = SEA_HEIGHT;
  float choppy = SEA_CHOPPY;
  vec2 uv = p.xz;
  uv.x *= 0.75;

  float d, h = 0.0;
  for (int i = 0; i < ITER_FRAGMENT; i++)
  {
    d = sea_octave((uv + SEA_TIME) * freq, choppy);
    d += sea_octave((uv - SEA_TIME) * freq, choppy);
    h += d * amp;
    uv *= octave_m;
    freq *= 1.9;
    amp *= 0.22;
    choppy = mix(choppy, 1.0, 0.2);
  }
  return p.y - h;
}

// tracing
vec3 getNormal(vec3 p, float eps)
{
  p = 111.f * p;
  vec3 n;
  n.z = map_detailed(p);
  n.x = map_detailed(vec3(p.x + eps, p.y, p.z)) - n.z;
  n.y = map_detailed(vec3(p.x, p.y, p.z + eps)) - n.z;
  n.z = eps;
  return normalize(n);
}

vec3 getNormal2(vec3 p, float eps)
{
  vec3 n;
  n.z = map(p);
  n.x = map(vec3(p.x + eps, p.y, p.z)) - n.z;
  n.y = map(vec3(p.x, p.y, p.z + eps)) - n.z;
  n.z = eps;
  return normalize(n);
}

vec3 get_water_normal()
{
  // newer not-really-working ambient waves:
  float eps = 0.00004f;
  vec3 partial_d = dFdx(frag_world_position);
  float lenpd = length(partial_d);

  vec2 waterp = 55.f * frag_world_position.xy;
  float h = ambient_waves(waterp, 1.7310f, 4);
  float hdy = ambient_waves(vec2(waterp.x, waterp.y + eps), 1.7310f, 4);
  float hdx = ambient_waves(vec2(waterp.x + eps, waterp.y), 1.7310f, 4);
  float dx = hdx - h;
  float dy = hdy - h;
  vec3 waternormalfine = normalize(vec3(dx, dy, lenpd * 5.f));

  waterp = 3.1f * frag_world_position.xy;
  h = ambient_waves(waterp, .2350f, 5);
  hdy = ambient_waves(vec2(waterp.x, waterp.y + eps), .2350f, 5);
  hdx = ambient_waves(vec2(waterp.x + eps, waterp.y), .2350f, 5);
  dx = hdx - h;
  dy = hdy - h;
  vec3 waternormallarge = normalize(vec3(dx, dy, lenpd * 3.75f));

  vec3 waternormal = normalize(waternormalfine + waternormallarge);

  waterp = 3.1f * frag_world_position.xy;

  h = ambient_waves(waterp, .2350f, 11);
  hdy = ambient_waves(vec2(waterp.x, waterp.y + eps), .2350f, 11);
  hdx = ambient_waves(vec2(waterp.x + eps, waterp.y), .2350f, 11);
  dx = hdx - h;
  dy = hdy - h;
  vec3 waternormaltest = normalize(vec3(dx, dy, eps));
  waternormaltest.z *= max(lenpd * 511121.f, .025251);
  return normalize(waternormaltest);
}

void main()
{

  vec4 albedo_tex = texture2D(texture0, frag_uv).rgba;
  if (discard_on_alpha)
  {
    if (albedo_tex.a < 0.3)
      discard;
  }
  gather_shadow_moments();

  mat3 TBN;
  TBN[0] = normalize(frag_TBN[0]);
  TBN[1] = normalize(frag_TBN[1]);
  TBN[2] = normalize(frag_TBN[2]);

  Material m;

  float premultiply_alpha = albedo_tex.a;
  if (alpha_albedo_override != -1.0f)
  {
    premultiply_alpha = alpha_albedo_override;
  }
  premultiply_alpha = premultiply_alpha * max(texture0_mod.a, 0);
  premultiply_alpha = clamp(premultiply_alpha, 0, 1);
  m.albedo = premultiply_alpha * texture0_mod.rgb * albedo_tex.rgb;

  float dist_to_pixel = length(camera_position - frag_world_position);
  float dist_factor = dist_to_pixel;
  dist_factor = water_dist_scale * dist_factor;
  dist_factor = 1.0f;

  // lets do less dist_factor when the camera is pointing at the surface
  float surfdotv = 1. - dot(normalize(camera_position - frag_world_position), vec3(0, 0, 1));

  surfdotv = clamp(surfdotv, 0, 1);
  surfdotv = pow(surfdotv, surfdotv_exp);

  float height_scale = 1.0 / pow(dist_factor, water_dist_exp);
  height_scale = 1.;

  // SMALL WAVE PARAMETERS:
  float height_scale1f = 0.35f * water_height_scale * height_scale;
  float water_time1f = water_speed * time;

  // LARGE WAVE PARAMETERS:
  float height_scale2f = height_scale2 * height_scale;
  float water_time2f = water_time2 * time;

  vec2 wateruv;
  if (water_use_uv)
  {
    wateruv = frag_uv;
  }
  else
  {
    wateruv = frag_world_position.xy;
  }

  // vec3 waternormal = water(wateruv,  water_scale,  height_scale1f,  water_time1f);
  // vec3 waternormal2 = water(wateruv,  water_scale2,  height_scale2f,  water_time2f);

  vec3 partial_d = dFdx(frag_world_position);
  float lenpd = length(partial_d);
  vec3 waternormal = get_water_normal();
  // waternormal = vec3(0,0,1);
  
  vec4 velocity_sample = texture(texture12, frag_uv);
  vec3 velocity = velocity_sample.r * vec3(-1, 0, 0) + velocity_sample.g * vec3(1, 0, 0) +
                  velocity_sample.b * vec3(0, -1, 0) + velocity_sample.a * vec3(0, 1, 0);

  vec2 fbmv = vec2(pow(length(velocity),1.2) / max(water_depth, 0.01));
  //fbmv = smoothstep(75.1011f, 216.50f, fbmv + velocity.xy);
  //fbmv = smoothstep(75.1011f, 216.50f, fbmv);
  float mistlocation = smoothstep(0.05f,3.f,length(fbmv));
  //mistlocation =saturate(pow(31300.f * height_variance * smoothstep(.10, .15, pow(length(velocity), 0.85f)), 1.15));
  






  float mistf1 =
      fbm_n(150.f * frag_uv - vec2(time, .5f * time), 3) * fbm_n(160.f * frag_uv + vec2(.5f * time, time), 3);
  float mistf2 = fbm_n(40.f * frag_uv - vec2(time, .5f * time), 1) + fbm_n(50.f * frag_uv + vec2(.5f * time, time), 1);

  float mistf = 51.5f * pow(mistf1, 0.785f) * pow(mistf2, 0.685f);

  
  float mist_dist_fade = min(1./(.03*pow(dist_to_pixel,2)),1);
  float mist_dist_fade2 = min(1./(.0004*pow(dist_to_pixel,2)),1);
  mistf1 = fbm_h_n(6150.f * frag_uv - 0.21f * vec2(time, .5f * time), .120f, 3) *
           fbm_h_n(6160.f * frag_uv + 0.21f * vec2(.5f * time, time), .120f, 3);
  mistf2 = fbm_h_n(240.f * frag_uv - 0.21f * vec2(time, .5f * time), .5120f, 5) +
           fbm_h_n(250.f * frag_uv + 0.21f * vec2(.5f * time, time), .5120f, 5);
           
  mistf1 = .51051*mistf1;
  mistf2 = .11*pow(mistf2,3);
  
  // good:
  //mistf = smoothstep(0, 35, mistf1* mistf2);
  mistf = mist_dist_fade*mistf1 * mist_dist_fade2*mistf2;
  mistf = .1*mistf1 * mistf2;
  

  m.albedo = mix(m.albedo, vec3(mistf), mistlocation);
  m.normal = normalize(TBN * waternormal);
  m.roughness = texture2_mod.r * texture2D(texture2, frag_uv).r;
  // m.roughness = mix(m.roughness, .6, mistlocation);
  m.ambient_occlusion = texture5_mod.r * texture2D(texture5, frag_uv).r;
  m.metalness = texture4_mod.r * texture2D(texture4, frag_uv).r;

  vec3 p = frag_world_position;
  vec3 v = normalize(camera_position - p);
  vec3 r = reflect(v, m.normal);
  vec3 F0 = vec3(0.02); // 0.02 F0 for water
  F0 = mix(F0, m.albedo, m.metalness);
  // F0 = vec3(0.9512);
  float ndotv = clamp(dot(m.normal, v), 0, 1); // allowing the backface ndotv to contribute for water

  m.emissive = texture1_mod.rgb * texture2D(texture1, frag_uv).rgb;
  vec3 debug = vec3(-9);
  vec3 direct_ambient = vec3(0);
  vec3 direct_specular = vec3(0);
  vec3 direct_diffuse = vec3(0);
  for (int i = 0; i < number_of_lights; ++i)
  {
    vec3 l = lights[i].position - p;
    float d = length(l);
    l = normalize(l);
    vec3 h = normalize(l + v);
    vec3 att = lights[i].attenuation;
    float at = 1.0 / (att.x + (att.y * d) + (att.z * d * d));

    const float shadow_bias = 0.0000003;

    float visibility = 1.0f; // used for blending cone edges and shadowing

    vec3 shadow_space_postw = frag_in_shadow_space_postw[i] - shadow_bias;
    vec2 shadow_moments = variance_depth[i];
    bool shadow_map_enabled = shadow_map_enabled[i];
    if (lights[i].type == 0)
    { // directional
      l = normalize(-lights[i].direction);
      h = normalize(l + v);
    }
    else if (lights[i].type == 2)
    { // cone
      vec3 dir = normalize(lights[i].position - lights[i].direction);
      float theta = lights[i].cone_angle;
      float phi = 1.0 - dot(l, dir);
      visibility = 0.0f;
      if (phi < theta)
      {
        float edge_softness_distance = 2.3f * theta;
        visibility = saturate((theta - phi) / edge_softness_distance);
      }
      if (shadow_map_enabled)
      {
        float light_visibility = chebyshevUpperBound(shadow_moments, shadow_space_postw.z, max_variance[i]);
        visibility = visibility * light_visibility;
      }
    }
    // direct light
    float ndotl = saturate(dot(m.normal, l));
    // we want to let light through the opposite side
    // for diffuse component, but if we use ndotl, we get a seam
    // at 90 degrees, so we use average of ndotl: 0.5
    // but since the light is going through both sides
    // we halve that again for conservation of energy
    float diffuse_ndotl = 0.25;

    float ndoth = saturate(dot(m.normal, h));
    float vdoth = saturate(dot(v, h));
    // float G = G_smith_GGX_denom(a,ndotv,ndotl);
    // specular brdf

    vec3 F = F_schlick(F0, ndoth); // sponge
    //    F = F_schlick(F0, 0.5f);
    if (dot(m.normal, l) < 0.0)
    {
      // F = 0.1 + (F*0.9);
    }
    // vec3 F = fresnelSchlickRoughness(clamp(ndotv,0,1), F0, m.roughness);
    float G = G_smith_schlick_GGX_direct(m.roughness, ndotv, ndotl);
    // float G = clamp(G_smith_GGX(m.roughness, vdoth, ndotl),0,1);
    float D = D_ggx(m.roughness, ndoth);
    float denominator = max(4.0f * ndotl * ndotv, 0.000001);
    vec3 specular = (F * G * D) / denominator;
    vec3 radiance = lights[i].flux * at;
    // specular result
    vec3 specular_result = radiance * specular;
    // diffuse result
    // F = F0;
    vec3 Kd = 1.0f - F;
    vec3 diffuse = Kd * m.albedo / PI;
    vec3 diffuse_result = radiance * diffuse;

    // this is where we used the special diffuse ndotl
    // specular is occluded on the back, and diffuse is let through

    direct_specular += specular_result * ndotl * visibility;
    direct_diffuse += diffuse_result * ndotl * visibility;

    // result += (specular_result + diffuse_result) * visibility * ndotl;

    direct_ambient += lights[i].ambient * at;
  }

  vec3 Ks = fresnelSchlickRoughness(ndotv, F0, m.roughness);
  const float MAX_REFLECTION_LOD = 6.0;

  // dont reflect downward in the cubemap
  // not the best but looks better because we know generally if we reflect downward
  // we'd hit other water that would reflect back up
  if (dot(r, vec3(0, 0, 1)) > 0)
  {
   r.z = -r.z;


  }

  float diffuse_loss = 1.0;//.315f; // lobe?

  // refraction sampling
  vec3 refracted_view = normalize(refract(v, m.normal, 1.02).xyz);
  vec2 offset = vec2(dot(refracted_view, camera_right), dot(refracted_view, camera_up));
  float inv_aspect = viewport_size.y / viewport_size.x;
  offset.x = offset.x * inv_aspect;
  // offset = refraction_offset_factor * offset; //sponge uniform offset factor
  offset = .03f * offset;
  offset = smoothstep(.01, .1f, water_depth) * offset;
  // offset = vec2(0);
  vec2 this_pixel = gl_FragCoord.xy / viewport_size;
  vec2 ref_sample_loc = this_pixel + offset;
  if (ref_sample_loc.x < 0 || ref_sample_loc.x > 1)
  {
    ref_sample_loc.x = this_pixel.x;
  }
  if (ref_sample_loc.y < 0 || ref_sample_loc.y > 1)
  {
    ref_sample_loc.y = this_pixel.y;
  }

  float len_offset = length(offset);
  if (isinf(len_offset) || isnan(len_offset))
  {
    ref_sample_loc = this_pixel;
    
  }


  // ambient specular
  vec3 prefilteredColor = textureLod(texture6, r, m.roughness * MAX_REFLECTION_LOD).rgb;
  vec2 envBRDF = texture2D(texture8, vec2(ndotv, m.roughness)).xy;
  vec3 ambient_specular = diffuse_loss * mix(vec3(1), F0, m.metalness) * prefilteredColor *
                          (mix(vec3(1), Ks, 1 - m.metalness) * envBRDF.x + envBRDF.y);

  // ambient diffuse
  vec3 Kd = diffuse_loss * vec3(1 - m.metalness) * (1.0 - Ks);
  vec3 irradiance = texture(texture7, -m.normal).rgb; // likely also wrong for flipped y
  vec3 ambient_diffuse = Kd * irradiance * m.albedo;


  vec4 refraction_src = texture2D(texture9, ref_sample_loc);
  // volumetric absorb
  //
  float depth_of_scene = texture2D(texture10, ref_sample_loc).r; // texture2D(texture10, this_pixel).r;
  float depth_of_self = texture2D(texture13, this_pixel).r;      // texture2D(texture10, this_pixel).r;
                                                                 // texture2D(texture9, this_pixel).rgb;

  if (linearize_depth(depth_of_scene) < linearize_depth(gl_FragCoord.z) - 0.011f)
  {
    refraction_src = texture2D(texture9, this_pixel);
    // refraction_src = vec4(1,0,0,1);
    // depth_of_scene = texture2D(texture10, this_pixel).r;
  }
  depth_of_scene = texture2D(texture10, this_pixel).r;
  // roughly world space but not exact, frustrum values differ in the linearize_depth function from the projection
  // matrix
  float depth_of_object = linearize_depth(depth_of_scene) - linearize_depth(gl_FragCoord.z);
  if (linearize_depth(depth_of_scene) > 10)
  {//probably is env map behind water
    //depth_of_object = linearize_depth(depth_of_self) - linearize_depth(gl_FragCoord.z);
    //debug = vec3(pow(linearize_depth(gl_FragCoord.z),1),0,0);

  }
  depth_of_object = 215.1f * depth_of_object;
  float density = pow(premultiply_alpha, 1);
  float A = pow(1 - density, pow(depth_of_object, 0.85f));

     if (isinf(len_offset) || isnan(len_offset))
  {
    //A = 1.0f;
  }


  // A = saturate((1 * density)/depth_of_object);
  float trim_very_thin_water =
      saturate(smoothstep(0.0015, 0.015, water_depth) + smoothstep(0.0015, 0.015, depth_of_object));
  float trim_very_thin_water2 = smoothstep(0.0345, 0.045, water_depth);
  float shore_fbm = saturate(0.45f * fbm_h_n(15.f * frag_world_position.xy + 2.f * vec2(sin(time)), 0.125f, 4));
  float shore_fbm2 =
      pow(saturate(.755f * fbm_h_n(1.5f * frag_world_position.xy + .65f * vec2(sin(time)), 0.8125f, 2)), 2);
  float shore_t = shore_fbm2 * shore_fbm * trim_very_thin_water2 * (1 - smoothstep(.00, 0.19f, water_depth));
  // A = mix(A,0.9f,step(0.9,A));

  // light that reaches v from sea floor
  float rndotv = clamp(-dot(-m.normal, -refracted_view), 0, 1);
  // vec3 rKs = fresnelSchlickRoughness(rndotv, F0, m.roughness);
  // vec3 tD = (1 - rKs); // transmissive minus internal specular
  vec3 tD = vec3(1.f);
  vec3 transmission = tD * A * refraction_src.rgb;

  vec3 total_specular = direct_specular + ambient_specular;
  vec3 total_diffuse = direct_diffuse + ambient_diffuse;

  vec3 mist_result = vec3(mistf * saturate(shore_t + mistlocation));
  vec3 result = transmission + trim_very_thin_water * (1 - A) * max(total_diffuse + total_specular, vec3(0));
  // result += (1 - A) * 2.f * mist_result * total_specular;
  result += trim_very_thin_water * clamp(2 * mist_result, vec3(0), vec3(1)) * length(total_diffuse);
  // result = (1 - A) *mist_result*Kd;
  vec3 ambient = m.ambient_occlusion * (ambient_specular + ambient_diffuse + max(direct_ambient, 0));
  // result = m.emissive + ambient +direct_specular + direct_diffuse;//+ transmission;
  result += m.emissive; //+ transmission;

  // result = vec3(smoothstep(0.000, 0.115, depth_of_object));
  // result = vec3(Ks*smoothstep(0.01,0.1,water_depth));
  // result = vec3(A);
  if (debug != vec3(-9))
  {
    result = debug;
  }
  // float height_t =  1-smoothstep(0,.4141,pow(lenpd,.25f));
  // result = vec3(height_t);


 

  //



  // basic fog:
  const float LOG2 = 1.442695;
  float z = gl_FragCoord.z / gl_FragCoord.w;
  float camera_relative_depth = length(frag_world_position-camera_position);
  float randfog = 1.4 + 00.7 * fbm_h_n(.112f *(frag_world_position.xy + vec2(frag_world_position.z)) + vec2(.3f*time), .820f, 3);
  z *= randfog;
  randfog = 1;
  float fogFactor = exp2(-.000015131f * randfog * z * z * LOG2);
  fogFactor = clamp(fogFactor, 0.0, 1.0);
  vec3 color = textureLod(texture6, v, 1).rgb; // mix(vec3(104,142,173)/vec3(255)
  result = mix(color, result, fogFactor);

  out0 = vec4(result, 1);
}
