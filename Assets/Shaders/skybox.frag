#version 330
uniform samplerCube texture6; //environment

uniform vec3 camera_position;
uniform mat4 Model;
//uniform float time;
in vec4 frag_world_position;

layout(location = 0) out vec4 out0;
void main()
{
  vec3 v = -(frag_world_position.xyz-(Model[3].xyz));
  out0 = textureLod(texture6,v,0);
}