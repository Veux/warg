#version 330
layout(location = 0) in vec3 position;
layout(location = 2) in vec2 uv;

uniform mat4 transform;

out vec2 frag_uv;
out vec4 frag_position;
void main()
{
  frag_uv = uv;
  vec4 pos = transform * vec4(position, 1);
  gl_Position = pos;
  frag_position = pos;
}