#version 330
uniform sampler2D texture0;
uniform uint mip_count;

in vec2 frag_uv;

layout(location = 0) out vec4 out0;

void main()
{
  vec3 result = vec3(0);
  for(uint i = 1u; i < mip_count; ++i)
  {
    result += textureLod(texture0, frag_uv,i).rgb;
  }
out0 = vec4(result/mip_count,1);

}