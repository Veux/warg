#version 330
uniform sampler2D texture0; // current;
uniform sampler2D texture1; // previous;

in vec2 frag_uv;
layout(location = 0) out vec4 out0;
void main()
{
  vec4 current = texture2D(texture0, frag_uv);
  vec4 previous = texture2D(texture1, frag_uv);
  vec4 result = mix(previous, current, 0.5f);
  out0 = result;
}