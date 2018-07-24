#version 330
uniform sampler2D texture1;
uniform vec4 texture1_mod = vec4(1,1,1,1);

in vec2 frag_uv;
in vec3 frag_world_position;
in vec2 frag_normal_uv;

layout(location = 0) out vec4 out0;

void main()
{
  out0 = texture1_mod * texture2D(texture1, frag_uv);
}