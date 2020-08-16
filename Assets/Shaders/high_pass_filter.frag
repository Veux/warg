#version 330
uniform sampler2D texture0;

in vec4 frag_position;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

void main()
{
  vec4 color = texture2D(texture0,frag_uv);
  
  //out0 = clamp(color - vec4(1.00),0,15);

  vec4 norm = normalize(color-vec4(1));
  float len = length(color);

  vec4 clamped = clamp(len,0,15.f) * norm;
  
  out0 = max(clamped,0); 
  
}