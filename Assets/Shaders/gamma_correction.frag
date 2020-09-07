#version 330
uniform sampler2D texture0;

in vec2 frag_uv;

layout(location = 0) out vec4 out0;

const float inverse_gamma = 1.0f / 2.22f;
vec3 to_srgb(in vec3 linear)
{
  return pow(linear, vec3(inverse_gamma));
}
void main()
{
  vec4 p = texture2D(texture0, frag_uv);
  out0 = vec4(to_srgb(p.rgb), p.a);
}