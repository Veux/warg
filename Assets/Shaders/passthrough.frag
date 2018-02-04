#version 330
uniform sampler2D texture0;

in vec2 frag_uv;

layout(location = 0) out vec4 out0;

void main()
{
  out0 = texture2D(texture0, frag_uv);
}