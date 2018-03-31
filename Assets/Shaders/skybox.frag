#version 330
uniform samplerCube texture6; //environment

uniform float time;
in vec2 frag_uv;
in vec4 frag_world_position;

layout(location = 0) out vec4 out0;
void main()
{
  out0 = vec4(3.5f * pow(texture(texture6, -(frag_world_position.xyz)).rgb,vec3(1.4)),1);
}