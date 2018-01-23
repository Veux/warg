#version 330
uniform sampler2D albedo;
uniform sampler2D specular;
uniform sampler2D normal;
uniform sampler2D emissive;
uniform sampler2D roughness;
#define MAX_LIGHTS 10
uniform sampler2D shadow_maps[MAX_LIGHTS];
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
in vec4 shadow_map_coords[MAX_LIGHTS];

layout(location = 0) out vec4 ALBEDO;

const float PI = 3.14159265358979f;
const float gamma = 2.2;
const int max_lights = MAX_LIGHTS;
vec3[max_lights] shadow_coord_array;
vec2[max_lights] shadow_moments_array;
vec3 to_linear(in vec3 srgb) { return pow(srgb, vec3(gamma)); }
float to_linear(in float srgb) { return pow(srgb, gamma); }
vec3 to_srgb(in vec3 linear) { return pow(linear, vec3(1. / gamma)); }
float linearize_depth(float z)
{
  float near = 0.1;
  float far = 1000;
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

float chebyshevUpperBound(vec2 moments, float distance)
{
  // Surface is fully lit. as the current fragment is before the light occluder
  if (distance <= moments.x)
    return 1.0;

  // The fragment is either in shadow or penumbra. We now use chebyshev's
  // upperBound to check How likely this pixel is to be lit (p_max)
  float variance = moments.y - (moments.x * moments.x);
  variance = max(variance, 0.00002);

  float d = distance - moments.x;
  float p_max = variance / (variance + d * d);

  return p_max;
}

void autism_init()
{
shadow_coord_array[0] = vec3(shadow_map_coords[0].xyz / shadow_map_coords[0].w);
    shadow_moments_array[0] = texture2D(shadow_maps[0], shadow_coord_array[0].xy).rg;
shadow_coord_array[1] = vec3(shadow_map_coords[1].xyz / shadow_map_coords[1].w);
    shadow_moments_array[1] = texture2D(shadow_maps[1], shadow_coord_array[1].xy).rg;
shadow_coord_array[2] = vec3(shadow_map_coords[2].xyz / shadow_map_coords[2].w);
    shadow_moments_array[2] = texture2D(shadow_maps[2], shadow_coord_array[2].xy).rg;
shadow_coord_array[3] = vec3(shadow_map_coords[3].xyz / shadow_map_coords[3].w);
    shadow_moments_array[3] = texture2D(shadow_maps[3], shadow_coord_array[3].xy).rg;
shadow_coord_array[4] = vec3(shadow_map_coords[4].xyz / shadow_map_coords[4].w);
    shadow_moments_array[4] = texture2D(shadow_maps[4], shadow_coord_array[4].xy).rg;
shadow_coord_array[5] = vec3(shadow_map_coords[5].xyz / shadow_map_coords[5].w);
    shadow_moments_array[5] = texture2D(shadow_maps[5], shadow_coord_array[5].xy).rg;
shadow_coord_array[6] = vec3(shadow_map_coords[6].xyz / shadow_map_coords[6].w);
    shadow_moments_array[6] = texture2D(shadow_maps[6], shadow_coord_array[6].xy).rg;
shadow_coord_array[7] = vec3(shadow_map_coords[7].xyz / shadow_map_coords[7].w);
    shadow_moments_array[7] = texture2D(shadow_maps[7], shadow_coord_array[7].xy).rg;
shadow_coord_array[8] = vec3(shadow_map_coords[8].xyz / shadow_map_coords[8].w);
    shadow_moments_array[8] = texture2D(shadow_maps[8], shadow_coord_array[8].xy).rg;
shadow_coord_array[9] = vec3(shadow_map_coords[9].xyz / shadow_map_coords[9].w);
    shadow_moments_array[9] = texture2D(shadow_maps[9], shadow_coord_array[9].xy).rg;
}

void main()
{
  vec3 debug = vec3(-1);
  autism_init();

  vec4 albedo_tex = texture2D(albedo, frag_uv).rgba;

  if (discard_over_blend)
  {
    if (albedo_tex.a < 0.3)
      discard;
  }

  Material m;
  m.specular = to_linear(texture2D(specular, frag_uv).rgb);
  m.albedo = to_linear(albedo_tex.rgb) / PI;
  m.emissive = to_linear(texture2D(emissive, frag_uv).rgb);
  m.shininess = 1.0 + 44 * (1.0 - to_linear(texture2D(roughness, frag_uv).r));
  vec3 n = texture2D(normal, frag_uv).rgb;
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
    vec3 shadow_coord = shadow_coord_array[i];
    vec2 shadow_moments = shadow_moments_array[i];
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
      }
      float light_visibility =  1.0 - chebyshevUpperBound(shadow_moments, shadow_coord.z);
     
      alpha = alpha * light_visibility;
    }
    float ldotn = clamp(dot(l, m.normal), 0, 1);
    float ec = (8.0f * m.shininess) / (8.0f * PI);
    float specular = ec * pow(max(dot(h, m.normal), 0.0), m.shininess);

    vec3 ambient = vec3(lights[i].ambient * at * m.albedo);
    result += ldotn * specular * m.albedo * lights[i].color * at * alpha;
    result += ambient;
  }
  result += m.emissive;
  result += additional_ambient * m.albedo;

  if (debug != vec3(-1))
    result = debug;

  ALBEDO = vec4(to_srgb(result), 1); // a was albedo_tex.a
}
