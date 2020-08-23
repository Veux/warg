#version 330
uniform sampler2D texture0;
uniform float time;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

void main()
{

  float h = sin(frag_uv.x + 0.1f*time);

  out0 = h + texture2D(texture0, frag_uv);
}