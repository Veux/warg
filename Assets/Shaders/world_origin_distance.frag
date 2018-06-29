#version 330
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 additional_ambient;
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
struct Light
{
  vec3 position;
  vec3 direction;
  vec3 color;
  vec3 attenuation;
  vec3 ambient;
  float cone_angle;
  uint type;
};
#define MAX_LIGHTS 10
uniform Light lights[MAX_LIGHTS];
uniform uint number_of_lights;

in vec3 frag_world_position;
in mat3 frag_TBN;
in vec2 frag_uv;
layout(location = 0) out vec4 out0;

float linearize_depth(float depth)
{
  float near = 0.1;
  float far = 10000;
  float z = depth * 2.0 - 1.0;
  return (2.0 * near * far) / (far + near - z * (far - near));
}

float epsilon = 0.00001;
void main()
{   
  bool tile_light = (mod(frag_world_position.x+epsilon, 1) < 0.5) ^^ (mod(frag_world_position.y+epsilon, 1) < 0.5)^^ (mod(frag_world_position.z+epsilon, 1) < 0.5);
  vec3 color = vec3(0);
  if (tile_light)
  {
   color = frag_world_position / 12;
  }

  float dist = length(frag_world_position);
  if(dist < 1)
  {
    color += 0.5f*vec3(1.000f+sin(time*10)*dist);
  }

  out0 = vec4(color, 1);
}