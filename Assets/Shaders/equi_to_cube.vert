#version 330
layout(location = 0) in vec3 position;

uniform mat4 projection;
uniform mat4 camera;
uniform mat4 rotation;

out vec3 direction;
void main()
{
  direction = position;
  gl_Position = projection * camera * vec4(position, 1.0);
}