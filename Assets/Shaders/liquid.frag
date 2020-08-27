#version 330
uniform sampler2D texture0;
uniform float time;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

void main()
{
  vec4 source = texture2D(texture0, frag_uv);
  float h = 0.04f;
  vec4 result = source - h*source;

  

  out0 = max(result ,vec4(0));
}