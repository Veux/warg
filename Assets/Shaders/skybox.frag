#version 330
uniform samplerCube texture6; //environment

uniform float time;
in vec2 frag_uv;
in vec4 frag_world_position;

layout(location = 0) out vec4 out0;
void main()
{
  out0 = vec4(textureLod(texture6, -(frag_world_position.xyz),0).rgb,1);
}