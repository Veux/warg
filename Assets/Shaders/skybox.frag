#version 330
uniform samplerCube texture6; //environment

uniform float time;
in vec2 frag_uv;
in vec4 frag_world_position;

layout(location = 0) out vec4 out0;
const float gamma = 2.2;
vec3 to_linear(in vec3 srgb) { return pow(srgb, vec3(gamma)); }
void main()
{
  out0 = vec4(3.5f * pow(to_linear(texture(texture6, -(frag_world_position.xyz)).rgb),vec3(1.4)),1);
}