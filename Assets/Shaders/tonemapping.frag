#version 330
uniform sampler2D texture0;
uniform vec4 texture0_mod = vec4(1,1,1,1);
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

vec4 Tonemap_ACES(vec4 x) {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const vec4 a = vec4(2.51);
    const vec4 b = vec4(0.03);
    const vec4 c = vec4(2.43);
    const vec4 d = vec4(0.59);
    const vec4 e = vec4(0.14);
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

void main()
{
  vec4 result = texture0_mod * texture2D(texture0, frag_uv);
  out0 = result;
  //out0 = Tonemap_ACES(result);
}