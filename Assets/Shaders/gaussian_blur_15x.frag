#version 330
uniform sampler2D texture0;
uniform vec2 gauss_axis_scale;
uniform uint lod;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

const int kernel_width = 15;
vec2 gauss[kernel_width];

void main()
{
  gauss[0]  = vec2(-7.0, 0.009033);
  gauss[1]  = vec2(-6.0, 0.018476);
  gauss[2]  = vec2(-5.0, 0.033851);
  gauss[3]  = vec2(-4.0, 0.055555);
  gauss[4]  = vec2(-3.0, 0.08167);
  gauss[5]  = vec2(-2.0, 0.107545);
  gauss[6]  = vec2(-1.0, 0.126854);
  gauss[7]  = vec2(0.0, 0.134032);
  gauss[8]  = vec2(1.0, 0.126854);
  gauss[9]  = vec2(2.0, 0.107545);
  gauss[10] = vec2(3.0, 0.08167);
  gauss[11] = vec2(4.0, 0.055555);
  gauss[12] = vec2(5.0, 0.033851);
  gauss[13] = vec2(6.0, 0.018476);
  gauss[14] = vec2(7.0, 0.009033);

  vec4 color = vec4(0.0);
  for (int i = 0; i < kernel_width; i++)
  {
    vec2 offset =
        vec2(gauss[i].x * gauss_axis_scale.x, gauss[i].x * gauss_axis_scale.y);
    vec2 sample_uv = frag_uv + offset;
    color += textureLod(texture0, sample_uv,lod) * gauss[i].y;
  }
  out0 = color;
  //out0 = vec4(0,1,1,1);
}