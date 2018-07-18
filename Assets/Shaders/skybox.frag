#version 330
uniform samplerCube texture6; //environment

uniform float time;
uniform mat4 transform;
in vec2 frag_uv;
in vec4 frag_world_position;

layout(location = 0) out vec4 out0;
void main()
{
  vec3 v = -(frag_world_position.xyz);
  v = v + transform[2].xyz;
  out0 = vec4(textureLod(texture6, v,0).rgb,1);;
}