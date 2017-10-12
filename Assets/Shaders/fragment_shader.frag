#version 330
uniform sampler2D albedo;
uniform sampler2D specular;
uniform sampler2D normal;
uniform sampler2D emissive;
uniform sampler2D roughness;
uniform sampler2D test_shadow_map;
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

float chebyshevUpperBound(sampler2D map, vec2 coord, float distance)
{
	// We retrive the two moments previously stored (depth and depth*depth)
	vec2 moments = texture2D(map,coord.xy).rg;
	
	// Surface is fully lit. as the current fragment is before the light occluder
	if (distance <= moments.x)
		return 1.0 ;

	// The fragment is either in shadow or penumbra. We now use chebyshev's upperBound to check
	// How likely this pixel is to be lit (p_max)
	float variance = moments.y - (moments.x*moments.x);
	variance = max(variance,0.00002);

	float d = distance - moments.x;
	float p_max = variance / (variance + d*d);

	return p_max;
}

void main()
{
  vec4 albedo_tex = texture2D(albedo, frag_uv).rgba;

  if(discard_over_blend)
  {
    if(albedo_tex.a < 0.3)
      discard;
  }

  Material m;
  m.specular = to_linear(texture2D(specular, frag_uv).rgb);
  m.albedo = to_linear(albedo_tex.rgb) / PI;
  m.emissive = to_linear(texture2D(emissive, frag_uv).rgb);
  m.shininess = 1.0 + 44 * (1.0 - to_linear(texture2D(roughness, frag_uv).r));
  vec3 n = texture2D(normal, frag_uv).rgb;
  //if (n == vec3(0))
  //{
  //  n = vec3(0, 0, 1);
  //  m.normal = frag_TBN * n;
  //}
  //else
  //{
  //  m.normal = frag_TBN * normalize((n * 2) - 1.0f);
  //}
  //vec3 debug = vec3(-1);
  //vec3 result = vec3(0);
  //for (int i = 0; i < number_of_lights; ++i)
  //{
  //  vec3 l = lights[i].position - frag_world_position;
  //  float d = length(l);
  //  l = normalize(l);
  //  vec3 v = normalize(camera_position - frag_world_position);
  //  vec3 h = normalize(l + v);
  //  vec3 att = lights[i].attenuation;
  //  float at = 1.0 / (att.x + (att.y * d) + (att.z * d * d));
  //  float alpha = 1.0f;

  //  if (lights[i].type == 0)
  //  { // directional
  //    l = -lights[i].direction;
  //    h = normalize(l + v);
  //  }
  //  else if (lights[i].type == 2)
  //  { // cone
  //    vec3 dir = normalize(lights[i].position - lights[i].direction);
  //    float theta = lights[i].cone_angle;
  //    float phi = 1.0 - dot(l, dir);
  //    alpha = 0.0f;
  //    if (phi < theta)
  //    {
  //      float edge_softness_distance = 2.3f*theta;
  //      alpha = clamp((theta - phi) / edge_softness_distance, 0, 1);
  //    }
  //    if(alpha != 0.0f && shadow_map_enabled[i])
  //    {
  //      vec2 shadow_coord = vec2(shadow_map_coords[i].xy / shadow_map_coords[i].w);
  //      float light_visibility = chebyshevUpperBound(shadow_maps[i], shadow_coord, d);
  //      alpha = alpha * light_visibility;
  //      float red_channel = texture2D(shadow_maps[i],vec2(0,0)).r;
  //      debug = vec3(100000*red_channel,0,0);

  //      //debug = vec3(texture2D(shadow_maps[i],shadow_coord).r);
  //      //debug = vec3(10*sin(time)*shadow_coord,0);
  //    }

  //  }
  //  float ldotn = clamp(dot(l, m.normal), 0, 1);
  //  float ec = (8.0f * m.shininess) / (8.0f * PI);
  //  float specular = ec * pow(max(dot(h, m.normal), 0.0), m.shininess);
    
  //  vec3 ambient = vec3(lights[i].ambient * at * m.albedo);
  //  result += ldotn * specular * m.albedo * lights[i].color * at * alpha;
  //  result += ambient;
  //}
  //result += m.emissive;
  //result += additional_ambient * m.albedo;

  //if (debug != vec3(-1))
  //  result = debug;





vec3 result = texture2D(test_shadow_map,frag_uv).rgb;

//vec3 result = texelFetch(shadow_maps[2],ivec2(1024,1024),0).rgb;
 //  ALBEDO = vec4(m.normal,1);


  //prevent optimizations....
  result+= 0.00000001*m.specular;
  result+= 0.00000001*m.albedo;
  result+= 0.00000001*m.emissive;
  result+= 0.00000001*m.shininess;
 ALBEDO = vec4(result,1);//sponge to_srgb albedo_tex.a
}
