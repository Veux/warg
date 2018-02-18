#version 330
uniform sampler2D texture0;

in vec4 frag_position;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

void main()
{
  vec4 color = texture2D(texture0,frag_uv);
  out0 = clamp(color - vec4(0.85),0,10000); 
}