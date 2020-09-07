#version 330
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform sampler2D texture4;
uniform sampler2D texture5;
uniform sampler2D texture6;
uniform sampler2D texture7;
uniform float progress0;
uniform float progress1;
uniform float progress2;
uniform float progress3;
uniform float progress4;
uniform float progress5;
uniform float progress6;
uniform float progress7;
uniform int count;
uniform float time;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;
layout(location = 1) out vec4 out1;
layout(location = 2) out vec4 out2;
layout(location = 3) out vec4 out3;
layout(location = 4) out vec4 out4;
layout(location = 5) out vec4 out5;
layout(location = 6) out vec4 out6;
layout(location = 7) out vec4 out7;

float PI = 3.14159;

vec4 compute(vec4 src, float progress)
{
  vec2 p = vec2(-(1.0 - frag_uv.y - 0.5), frag_uv.x - 0.5);

  float progress_radians = progress * 2.0 * PI;
  float position_radians = atan(p.y, p.x) + PI;

  bool should_filter = position_radians < progress_radians;
  vec4 overlay = vec4(1.0 - float(should_filter) * 0.5);

  return src * overlay;
}

void main()
{
  // out0 = compute(texture2D(texture0, frag_uv), progress0);
  out1 = compute(texture2D(texture1, frag_uv), progress1);
  out2 = compute(texture2D(texture2, frag_uv), progress2);
  out3 = compute(texture2D(texture3, frag_uv), progress3);
  out4 = compute(texture2D(texture4, frag_uv), progress4);
  out5 = compute(texture2D(texture5, frag_uv), progress5);
  out6 = compute(texture2D(texture6, frag_uv), progress6);
  out7 = compute(texture2D(texture7, frag_uv), progress7);

  out0 = vec4(1, 1, 0, 1);
}
