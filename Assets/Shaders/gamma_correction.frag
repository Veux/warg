#version 330
uniform sampler2D texture0;

in vec2 frag_uv;

layout(location = 0) out vec4 out0;

const float gamma = 2.2;
vec3 to_srgb(in vec3 linear) { return pow(linear, vec3(1. / gamma)); }
void main()
{
  vec4 sample = texture2D(texture0, frag_uv);
  out0 = vec4(to_srgb(sample.rgb),sample.a);
}