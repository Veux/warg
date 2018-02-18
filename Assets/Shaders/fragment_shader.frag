#version 330
uniform sampler2D texture0; // albedo
uniform sampler2D texture1; // specular;
uniform sampler2D texture2; // normal;
uniform sampler2D texture3; // emissive;
uniform sampler2D texture4; // roughness;
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
uniform bool discard_over_blend;
struct Light
{
  vec3 position;
  vec3 direction;
  vec3 color;
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
in vec4 frag_in_shadow_space[MAX_LIGHTS];

layout(location = 0) out vec4 RESULT;

const float gamma = 2.2;
const float PI = 3.14159265358979f;
const int max_lights = MAX_LIGHTS;
vec3[max_lights] frag_in_shadow_space_postw;
vec2[max_lights] variance_depth;
vec3 to_linear(in vec3 srgb) { return pow(srgb, vec3(gamma)); }
float to_linear(in float srgb) { return pow(srgb, gamma); }
float linearize_depth(float z)
{
  float near = 0.001;
  float far = 100;
  float depth = z * 2.0 - 1.0;
  return (2.0 * near * far) / (far + near - depth * (far - near));
}
struct Material
{
  vec3 albedo;
  vec3 specular;
  vec3 emissive;
  vec3 normal;
  float shininess;
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
  frag_in_shadow_space_postw[0] =
      vec3(frag_in_shadow_space[0].xyz / frag_in_shadow_space[0].w);
  variance_depth[0] =
      texture2D(shadow_maps[0], frag_in_shadow_space_postw[0].xy).rg;

  frag_in_shadow_space_postw[1] =
      vec3(frag_in_shadow_space[1].xyz / frag_in_shadow_space[1].w);
  variance_depth[1] =
      texture2D(shadow_maps[1], frag_in_shadow_space_postw[1].xy).rg;
  frag_in_shadow_space_postw[2] =
      vec3(frag_in_shadow_space[2].xyz / frag_in_shadow_space[2].w);
  variance_depth[2] =
      texture2D(shadow_maps[2], frag_in_shadow_space_postw[2].xy).rg;
  frag_in_shadow_space_postw[3] =
      vec3(frag_in_shadow_space[3].xyz / frag_in_shadow_space[3].w);
  variance_depth[3] =
      texture2D(shadow_maps[3], frag_in_shadow_space_postw[3].xy).rg;
  frag_in_shadow_space_postw[4] =
      vec3(frag_in_shadow_space[4].xyz / frag_in_shadow_space[4].w);
  variance_depth[4] =
      texture2D(shadow_maps[4], frag_in_shadow_space_postw[4].xy).rg;
  frag_in_shadow_space_postw[5] =
      vec3(frag_in_shadow_space[5].xyz / frag_in_shadow_space[5].w);
  variance_depth[5] =
      texture2D(shadow_maps[5], frag_in_shadow_space_postw[5].xy).rg;
  frag_in_shadow_space_postw[6] =
      vec3(frag_in_shadow_space[6].xyz / frag_in_shadow_space[6].w);
  variance_depth[6] =
      texture2D(shadow_maps[6], frag_in_shadow_space_postw[6].xy).rg;
  frag_in_shadow_space_postw[7] =
      vec3(frag_in_shadow_space[7].xyz / frag_in_shadow_space[7].w);
  variance_depth[7] =
      texture2D(shadow_maps[7], frag_in_shadow_space_postw[7].xy).rg;
  frag_in_shadow_space_postw[8] =
      vec3(frag_in_shadow_space[8].xyz / frag_in_shadow_space[8].w);
  variance_depth[8] =
      texture2D(shadow_maps[8], frag_in_shadow_space_postw[8].xy).rg;
  frag_in_shadow_space_postw[9] =
      vec3(frag_in_shadow_space[9].xyz / frag_in_shadow_space[9].w);
  variance_depth[9] =
      texture2D(shadow_maps[9], frag_in_shadow_space_postw[9].xy).rg;
}

void main()
{
  vec4 debug = vec4(-1);
  gather_shadow_moments();

  vec4 albedo_tex = texture2D(texture0, frag_uv).rgba;

  if (discard_over_blend)
  {
    if (albedo_tex.a < 0.3)
      discard;
  }

  Material m;
  m.specular = to_linear(texture2D(texture1, frag_uv).rgb);
  m.albedo = to_linear(albedo_tex.rgb) / PI;
  m.emissive = to_linear(texture2D(texture3, frag_uv).rgb);
  m.shininess = 1.0 + 64 * (1.0 - to_linear(texture2D(texture4, frag_uv).r));
  vec3 n = texture2D(texture2, frag_uv).rgb;

  if (n == vec3(0))
  {
    n = vec3(0, 0, 1);
    m.normal = frag_TBN * n;
  }
  else
  {
    m.normal = frag_TBN * normalize((n * 2) - 1.0f);
  }
  vec3 result = vec3(0);
  for (int i = 0; i < number_of_lights; ++i)
  {
    vec3 l = lights[i].position - frag_world_position;
    float d = length(l);
    l = normalize(l);
    vec3 v = normalize(camera_position - frag_world_position);
    vec3 h = normalize(l + v);
    vec3 att = lights[i].attenuation;
    float at = 1.0 / (att.x + (att.y * d) + (att.z * d * d));
    float alpha = 1.0f;
    float shadow_bias = 0.0000003;
    vec3 frag_in_this_light_shadow_space_postw =
        frag_in_shadow_space_postw[i] - shadow_bias;
    vec2 shadow_moments = variance_depth[i];
    float this_map_variance = max_variance[i];
    bool shadow_map_enabled = shadow_map_enabled[i];
    if (lights[i].type == 0)
    { // directional
      l = -lights[i].direction;
      h = normalize(l + v);
    }
    else if (lights[i].type == 2)
    { // cone
      vec3 dir = normalize(lights[i].position - lights[i].direction);
      float theta = lights[i].cone_angle;
      float phi = 1.0 - dot(l, dir);
      alpha = 0.0f;
      if (phi < theta)
      {
        float edge_softness_distance = 2.3f * theta;
        alpha = clamp((theta - phi) / edge_softness_distance, 0, 1);

        // debug.rgb = vec3(0,1,0);
        // float shadow_map_depth = (shadow_moments.r-0.5)*2.0;
        // float frag_depth_from_light =
        // frag_in_this_light_shadow_space_postw.z;  float light_visibility =
        // float(frag_depth_from_light-0.0000008 < shadow_moments.r);  debug.rgb =
        // vec3(linearize_depth(shadow_map_depth));  debug.rgb =
        // vec3(light_visibility);
      }
      if (true)
      {
        float light_visibility = chebyshevUpperBound(shadow_moments,
            frag_in_this_light_shadow_space_postw.z, this_map_variance);
        // vec2 shadow_uv = frag_in_this_light_shadow_space_postw.xy;
        // debug.rgb = vec3(shadow_uv,0);
        // debug.rgb = vec3(light_visibility);
        // debug.a = 1.;
        //  debug.rgb = vec3(shadow_moments.r);
        alpha = alpha * light_visibility;
      }
    }
    float ldotn = clamp(dot(l, m.normal), 0, 1);
    float ec = (8.0f * m.shininess) / (8.0f * PI);
    float specular = ec * pow(max(dot(h, m.normal), 0.0), m.shininess);
    vec3 ambient = vec3(lights[i].ambient * at * m.albedo);
    result += ldotn * specular * m.albedo * lights[i].color * at * alpha;
    result += ambient;
  }
  result += 2.0f*m.emissive;
  result += additional_ambient * m.albedo;

  //debug.rgb = vec3(texture2D(shadow_maps[1],
  //frag_in_shadow_space_postw[1].xy).rg,0);  
  //debug.rgb = m.normal;  
  //debug.a = 1;
  albedo_tex.a = 1;
  if (debug != vec4(-1))
  {
    result = debug.rgb;
    albedo_tex.a = debug.a;
  }
  
  RESULT = vec4(result, albedo_tex.a);
}
