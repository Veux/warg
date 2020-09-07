#version 330
uniform sampler2D texture0;
uniform vec4 texture0_mod = vec4(1, 1, 1, 1);
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

void main()
{
  out0 = texture0_mod * texture2D(texture0, frag_uv);
}