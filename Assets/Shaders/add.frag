#version 330
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

void main()
{
  out0 = texture2D(texture0, frag_uv) + texture2D(texture1, frag_uv) + texture2D(texture2, frag_uv);
}