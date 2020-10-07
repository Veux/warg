#version 330
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
uniform vec2 normal_uv_scale;
uniform mat4 view;
uniform mat4 project;
uniform bool use_billboarding;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;
layout(location = 5) in mat4 instanced_MVP;
layout(location = 9) in mat4 instanced_model;
layout(location = 13) in vec4 billboard_position;
layout(location = 14) in vec4 attribute0; // generic per-instance attributes
layout(location = 15) in vec4 attribute1;
layout(location = 16) in vec4 attribute2;

out vec3 frag_world_position;
out mat3 frag_TBN;
out vec2 frag_uv;
out vec2 frag_normal_uv;
void main()
{
  vec3 t = normalize(instanced_model * vec4(tangent, 0)).xyz;
  vec3 b = normalize(instanced_model * vec4(bitangent, 0)).xyz;
  vec3 n = normalize(instanced_model * vec4(normal, 0)).xyz;
  frag_TBN = mat3(t, b, n);
  frag_world_position = (instanced_model * vec4(position, 1)).xyz;
  frag_uv = uv_scale * vec2(uv.x, uv.y);
  frag_normal_uv = normal_uv_scale * frag_uv;
  float s = sin(time);

  //  if (use_billboarding)
  //    gl_Position = project * (view * instanced_model * billboard_position + vec4(position.xy, 0, 0));
  //  else
  //    gl_Position = instanced_MVP * vec4(position, 1);

  if (use_billboarding)
    gl_Position = project * (billboard_position + instanced_model * vec4(position.xy, 0, 0));
  else
    gl_Position = instanced_MVP * vec4(position, 1);
}
