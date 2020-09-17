#version 330
#extension GL_ARB_separate_shader_objects : enable

#define MAX_LIGHTS 10

uniform sampler2D texture11; // displacement
uniform vec4 texture11_mod;
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
uniform vec2 normal_uv_scale;
uniform mat4 txaa_jitter;
uniform mat4 MVP;
uniform mat4 Model;
uniform mat4 shadow_map_transform[MAX_LIGHTS];

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;

out vec3 frag_world_position;
out mat3 frag_TBN;
out vec2 frag_uv;
out vec2 frag_normal_uv;
out vec4 frag_in_shadow_space[MAX_LIGHTS];
void main()
{
  vec3 t = normalize(Model * normalize(vec4(tangent, 0))).xyz;
  vec3 b = normalize(Model * normalize(vec4(bitangent, 0))).xyz;
  vec3 n = normalize(Model * normalize(vec4(normal, 0))).xyz;

  frag_world_position = (Model * vec4(position, 1)).xyz;
  frag_TBN = mat3(t, b, n);

  frag_uv = uv_scale * vec2(uv.x, -uv.y);
  frag_normal_uv = normal_uv_scale * frag_uv;
  for (int i = 0; i < MAX_LIGHTS; ++i)
  {
    frag_in_shadow_space[i] = shadow_map_transform[i] * Model * vec4(position, 1);
  }
  gl_Position = txaa_jitter * MVP * vec4(position, 1);
}
