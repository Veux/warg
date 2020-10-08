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

uniform sampler2D texture10; // refraction

uniform sampler2D texture11; // displacement

uniform vec4 texture0_mod;
uniform vec4 texture1_mod;
uniform vec4 texture2_mod;
uniform vec4 texture3_mod;
uniform vec4 texture4_mod;
uniform vec4 texture5_mod;
uniform vec4 texture6_mod;
uniform vec4 texture10_mod;
uniform vec4 texture11_mod;

#define xor ^^
#define MAX_LIGHTS 10
uniform sampler2D shadow_maps[MAX_LIGHTS];
uniform float max_variance[MAX_LIGHTS];
uniform bool shadow_map_enabled[MAX_LIGHTS];
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 additional_ambient;
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
uniform bool discard_on_alpha;
uniform float alpha_albedo_override;
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
uniform bool ground;
in float water_depth;
in float ground_height;
flat in float biome;
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

float in_range(float x, float min_edge, float max_edge)
{
  return float((x >= min_edge) && (x < max_edge));
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

/*
useful links:


http://c0de517e.blogspot.com/2014/06/oh-envmap-lighting-how-do-we-get-you.html


notes:
  radiant flux: total amount of energy, watts: Φ
  solid angle: area of the shape of an object projected onto unit sphere: ω
  radiant intensity: flux per solid angle: I

  radiance: radiant intensity scaled by surface angle: L
            the amount of light a point on a surface gets from a certain source
direction

irradiance: sum of all radiance from every direction onto the point

Rendering equation:
  Lo(p,ωo)= ∫Ω fr(p,ωi,ωo)Li(p,ωi)n⋅ωi dωi

  ωo is a light output direction
  p is the point on a surface

  Lo(p,ωo) is light from point p to w, the solid angle of a camera's pixel
  Li(p,ωi) is irradiance from the scene onto point p
  n⋅ωi is surface incident angle correction
  fr(p,ωi,ωo) is the BRDF, how much of the specific input light ray in question
contributes to the output


    we require a shader that is able to compute an irradiance map from a
radiance(environment) map this is very slow required for rough surfaces because
the subpixel roughness at p can reflect varying amounts of light from many
different incoming directions


  smith's method separates the G term into two pieces, G(l)G(v), G being the
same function for both light and view vectors



*/
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

float random(vec2 st)
{
  return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 st)
{
  vec2 i = floor(st);
  vec2 f = fract(st);

  // Four corners in 2D of a tile
  float a = random(i);
  float b = random(i + vec2(1.0, 0.0));
  float c = random(i + vec2(0.0, 1.0));
  float d = random(i + vec2(1.0, 1.0));

  vec2 u = f * f * (3.0 - 2.0 * f);

  return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
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

#define NUM_OCTAVES2 3
float fbm(vec2 uv)
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

float waterheight(vec2 uv, float time)
{
  vec2 q = vec2(0.);
  q.x = fbm(uv + .00 * time);
  q.y = fbm(uv + vec2(1.0));

  vec2 r = vec2(0.);
  r.x = fbm(uv + 1.0 * q + vec2(1.7, 9.2) + 0.15 * time);
  r.y = fbm(uv + 1.0 * q + vec2(8.3, 2.8) + 0.126 * time);
  float f = fbm(uv + r);

  // vec3 partial_d = dFdx(frag_world_position);
  vec3 partial_d = dFdy(frag_world_position);
  float lenpd = length(partial_d);

  float divisor = 65.1f * lenpd;

  divisor = max(1, divisor);
  f = f / divisor;
  // f = 0.2*f;

  float scale = 0.151f;
  uv = scale * uv;
  q.x = fbm(uv + .00 * time);
  q.y = fbm(uv + vec2(1.0));
  r.x = fbm(uv + 1.0 * q + vec2(1.7, 9.2) + scale * .3515 * time);
  r.y = fbm(uv + 1.0 * q + vec2(8.3, 2.8) + scale * .35126 * time);
  f = f + 1.f * fbm(uv + 3.f * r);

  return f;
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

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float num = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = GeometrySchlickGGX(NdotV, roughness);
  float ggx1 = GeometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}

vec3 hsv2rgb(vec3 c)
{
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
  vec3 result = vec3(0);
  vec4 debug = vec4(-1);

  vec4 albedo_tex = texture2D(texture0, frag_uv).rgba;
  if (discard_on_alpha)
  {
    // if (texture0_mod.a*albedo_tex.a < 0.3)
    // discard;
  }
  gather_shadow_moments();

  mat3 TBN;
  TBN[0] = normalize(frag_TBN[0]);
  TBN[1] = normalize(frag_TBN[1]);
  TBN[2] = normalize(frag_TBN[2]);

  Material m;

  /*
    alpha is defined as the percentage of light that is absorbed by the Material and converted to heat
    when passing through it
    technically, some light would bounce around inside and obey fresnels law and may exit elsewhere
    but we will just use a single float to describe the absorption

    the alpha value multiplies the framebuffer color behind it to 'block' that light from view
    alpha value also scales the albedo here before we compute lighting to keep conservation of energy
    at low alpha values, we want low diffuse values because this translucent object is being 'added' to the dst buffer

    a low diffuse value won't affect the specularity at all on nonconductors, and transparent conductors
    don't exist irl

    so the diffuse component can be thought of as 'the amount of the (non-specular) light that is reflected back out to
    see' and alpha is "how much of the (non-specular) radiant light passes through the object rather than absorbed or
    reflected back out
  */
  float premultiply_alpha = albedo_tex.a;
  if (alpha_albedo_override != -1.0f)
  {
    premultiply_alpha = alpha_albedo_override;
  }
  premultiply_alpha *= texture0_mod.a;

  m.albedo = texture0_mod.rgb * albedo_tex.rgb;
  // m.albedo = pow(m.albedo,vec3(1/2.2));
  m.normal = TBN * normalize(texture3_mod.rgb * texture2D(texture3, frag_normal_uv).rgb * 2.0f - 1.0f);
  m.emissive = texture1_mod.rgb * texture2D(texture1, frag_uv).rgb;
  m.roughness = texture2_mod.r * texture2D(texture2, frag_uv).r;
  m.metalness = texture4_mod.r * texture2D(texture4, frag_uv).r;
  // m.metalness = clamp(m.metalness,0.05f,0.45f);
  m.ambient_occlusion = texture5_mod.r * texture2D(texture5, frag_uv).r;

  float rrt = random(vec2(0.5f * time, time));

  float is_char_to_dirt = in_range(biome, 0.f, 1.f);
  float is_soil = in_range(biome, 1.f, 2.f);
  float is_very_wet_soil = in_range(biome, 1.5f, 2.f);
  float is_grass = in_range(biome, 2.f, 4.f);
  float is_heavy_grass = in_range(biome, 2.5f, 4.f);
  float on_fire = in_range(biome, 4.f, 5.f); // timenoise mod red color
  // float fire_is_fading = in_range(biome,5.f,6.f);//fade to char but add blinking dots of embers

  float fire_f = 1.0f;
  vec3 fire = 30.f * fire_f * vec3(4.5, 2.1, .1);
  vec3 grass = 0.255f * fbm_h_n(5.15101f * frag_world_position.xy, .41f, 9) * vec3(0.18, .433, .08);
  grass = grass * fbm_h_n(.1515101f * frag_world_position.xy, .71f, 3);
  vec3 snow = 0.55f * fbm_h_n(.5101f * frag_world_position.xy, .41f, 9) * vec3(1);
  // grass =;

  // i will be assigning the snow color to the grass variable because thats whats actually getting rendered, jank

  float snow_fbm = .4 + .6 * fbm_n(130 * frag_uv, 3);
  float snow_top = smoothstep(11. * snow_fbm, 14. * snow_fbm, 95 * ground_height);
  grass = grass * (1 - snow_top) + snow_top * vec3(1);

  vec3 soil = fbm_h_n(10.f * frag_world_position.xy, .01f, 7) * 0.35845f * vec3(0.3, .13, .036);
  float rock_t = fbm_h_n(20.f * frag_world_position.xy, .01f, 1);
  float rock_t2 = fbm_h_n(50.f * frag_world_position.xy, .501f, 1);
  float rock_t3 = fbm_h_n(660.f * frag_world_position.xy, .501f, 2);
  vec3 rock_color =
      rock_t3 * (mix(vec3(0), vec3(.3, .15, 0), 0.523f * rock_t) + mix(vec3(0), vec3(1), 0.12f * rock_t2));
  soil = soil + (step(.9515, fbm_h_n(550.f * frag_world_position.xy, .01f, 1)) *
                    fbm_h_n(55.f * frag_world_position.xy, .701f, 1) * grass);
  // soil = soil + (smoothstep(4.155,4.156,fbm_h_n(50.f*frag_world_position.xy,.01f,7))*rock_color);
  float rock_location = smoothstep(0.852871115814, .980f, fbm_h_n(15.f * frag_world_position.xy, .01f, 1));
  soil = mix(soil, rock_color, rock_location);
  // soil = vec3(rock_location);
  vec3 wet_soil = 0.32f * soil;

  vec3 c = hsv2rgb(
      vec3(saturate(noise(21.1f * frag_world_position.xy) * fbm_h_n(31.f * frag_world_position.xy, .501f, 4)), 1, 1));

  vec3 flower_color = c;

  float flower_location = (1 - 1.512f * fbm_h_n(2.7595f * frag_world_position.xy, .101f, 2)) +
                          (1 - 1.12f * fbm_h_n(25.5f * frag_world_position.xy, .101f, 3));

  vec3 charred = 0.135f * vec3(length(soil));
  // only one of these will be nonzero
  float char_to_dirt_t = saturate(biome);
  float soil_t = saturate(biome - 1.0f);
  float grass_t = saturate(biome - 2.f);
  float fire_t = saturate(biome - 4.f);

  float fire_visual_intensity = sin(3.14 * pow(fire_t, 2));
  // extra effects:
  float vert_wet_soil_t = 2.f * clamp(biome - 1.5f, 0, 0.5f);
  float heavy_grass_t = 2.f * clamp(biome - 2.65f, 0, 0.5f);
  char_to_dirt_t = pow(char_to_dirt_t, 5.f);
  vec3 charr_contribution = is_char_to_dirt * mix(charred, soil, char_to_dirt_t);
  vec3 soil_contribution = is_soil * mix(soil, wet_soil, soil_t);
  vec3 grass_contribution = is_grass * mix(wet_soil, grass, grass_t);
  vec3 fire_contribution = fire_visual_intensity * on_fire * fire;
  float smoke = pow(waterheight(210.f * frag_world_position.xy, 20.f * time), 1.0);
  fire_contribution = 1.f * mix(vec3(0), fire_contribution, 1.f - smoke);
  vec3 flowers_contribution = max(is_heavy_grass * flower_location * flower_color, 0);

  vec3 water = result;
  float water_depth_t = clamp(pow(12.1f * water_depth, 1.f), 0, 1);

  m.albedo = max(charr_contribution + soil_contribution + grass_contribution, 0);
  m.emissive = max(fire_contribution, vec3(0));
  m.roughness = 1.0f - (is_soil * soil_t);

  float terrain_ao = 0.1f + 0.9 * smoothstep(0, 7, pow(255.f * ground_height, 1.0));
  m.ambient_occlusion = terrain_ao;

  vec3 p = frag_world_position;
  vec3 v = normalize(camera_position - p);
  vec3 r = reflect(v, m.normal);
  vec3 F0 = vec3(0.04); // default dielectrics
  // todo: could put dielectric reflectivity in a uniform
  // this would let us specify more light absorbant materials
  F0 = mix(F0, m.albedo, m.metalness);

  float ndotv = saturate(dot(m.normal, v));

  float roughnessclamp = clamp(m.roughness, 0.001, 1);

  /*
  metal should mul specular by albedo because albedo map means F0
  plastic should not mul specular by albedo
  */

  ///////////////////////////////////////////////////////////////////
  // LIGHTING:
  vec3 direct_ambient = vec3(0);
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
    float ndotl = saturate(dot(m.normal, l));
    float ndoth = saturate(dot(m.normal, h));
    float vdoth = saturate(dot(v, h));
    vec3 radiance = lights[i].flux * at;
    // specular brdf
    // smith ggx denom seems to get rid of the weird banding artifacting at high roughness
    vec3 F = F_schlick(F0, ndoth);
    float G = G_smith_GGX_denom(roughnessclamp, ndotv, ndotl);
    G = G_smith_GGX(roughnessclamp, ndotv, ndotl);
    float D = D_ggx(roughnessclamp, ndoth);
    D = DistributionGGX(m.normal, h, roughnessclamp); // these are different, not sure why
    vec3 specular = (F * G * D);

    float denominator = max(4.0f * ndotl * ndotv, 0.000001);

    // these both have banding artifacts at high roughness
    // G =  GeometrySmith(m.normal, v, l, roughnessclamp);
    // G =  GeometrySchlickGGX(ndotv, roughnessclamp);

    vec3 specular_result = radiance * specular;
    vec3 Kd = (1.0f - F) * (1 - m.metalness); // Ks = F
    vec3 diffuse = Kd * m.albedo / PI;
    vec3 diffuse_result = radiance * diffuse;
    result += (specular_result + diffuse_result) * visibility * ndotl;
    // debug.rgb = vec3((specular_result)  * ndotl);
    // debug.rgb = vec3(G);
    direct_ambient += lights[i].ambient * at * m.albedo;
  }
  vec3 directonly = result;

  // ambient specular
  vec3 Ks = fresnelSchlickRoughness(ndotv, F0, m.roughness);
  const float MAX_REFLECTION_LOD = 6.0;
  vec3 prefilteredColor = textureLod(texture6, r, m.roughness * MAX_REFLECTION_LOD).rgb;
  vec2 envBRDF = texture2D(texture8, vec2(ndotv, m.roughness)).xy;
  vec3 ambient_specular =
      mix(vec3(1), F0, m.metalness) * prefilteredColor * (mix(vec3(1), Ks, 1 - m.metalness) * envBRDF.x + envBRDF.y);

  // ambient diffuse
  vec3 Kd = vec3(1 - m.metalness) * (1.0 - Ks);
  vec3 irradiance = texture(texture7, -m.normal).rgb;
  vec3 ambient_diffuse = Kd * irradiance * m.albedo; /// PI; //appears to be correct without dividing by pi as in direct
                                                     /// lighting, as it seems to be baked in to the irradiance map
  vec3 ambient = (ambient_specular + ambient_diffuse);

  result += m.ambient_occlusion * (ambient + max(direct_ambient, 0));
  result += m.emissive;

  if (debug != vec4(-1))
  {
    result = debug.rgb;
  }

  // basic fog:
  const float LOG2 = 1.442695;
  float z = gl_FragCoord.z / gl_FragCoord.w;
  float camera_relative_depth = length(frag_world_position - camera_position);
  float randfog =
      1.4 + 00.7 * fbm_h_n(.112f * (frag_world_position.xy + vec2(frag_world_position.z)) + vec2(.3f * time), .820f, 3);
  z *= randfog;
  randfog = 1;
  float fogFactor = exp2(-.000015131f * randfog * z * z * LOG2);
  fogFactor = clamp(fogFactor, 0.0, 1.0);
  vec3 color = textureLod(texture6, v, 2).rgb; // mix(vec3(104,142,173)/vec3(255)
  result = mix(color, result, fogFactor);
  // result = vec3(z);
  // result = ambient_diffuse;
  // result = vec3(is_soil*soil_t);
  // result = m.emissive;
  //  vec2 brdftexcoord = vec2(gl_FragCoord.x/1920,gl_FragCoord.y/1080);
  //  vec2 brdfc = texture2D(texture8,brdftexcoord).xy;
  //  result = vec3(brdfc ,0);
  // result = vec3(gl_FragCoord.x/1920,gl_FragCoord.y/1080,0);
  // result = (mix(vec3(1),Ks,1-m.metalness)*envBRDF.x + envBRDF.y);
  // result = vec3(ndotv);
  // result = vec3(m.metalness);
  // result = vec3(m.roughness);
  // result = mix(vec3(1),F0,m.metalness)*prefilteredColor;
  // result = vec3(gl_FragCoord.x/1920,gl_FragCoord.y/1080,0);
  // result = vec3(gl_FragCoord.x/1920);
  // result = 0.05f*vec3(length(frag_world_position));
  // result = m.albedo;
  // result = max(m.normal,vec3(0));

  // float ff = float(indebug.r >= 1.f && indebug.r <= 1.01f);

  // result = vec3(ff);

  // result = pow(result,vec3(2.2));
  // result = clamp(result,vec3(0),vec3(1));
  // result = prefilteredColor;
  // result = vec3(texture2D(texture2, frag_uv).r);
  // result = vec3(texture2_mod.r);
  // result = directonly;
  // result = vec3(Ks);
  // result = textureLod(texture6, r, .8).rgb;
  // result = vec3(texture2D(texture4, frag_uv).r);
  // result = irradiance;
  // result = m.albedo;
  // result = m.emissive;
  // result = texture3_mod.rgb;
  // result = vec3(frag_normal_uv,0);
  // result = texture2D(texture3, frag_normal_uv).rgb;
  // result = vec3(albedo_tex.a);
  // result = clamp(result,vec3(0),vec3(1));
  // result = textureLod(texture6,r,4).rgb;
  // result = pow(result,vec3(2.2));
  // result = 1*result;
  // result = vec3(ground_height);
  // result = vec3(m.ambient_occlusion);
  out0 = vec4(result, 1);
}