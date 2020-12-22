#version 330
uniform sampler2D texture0;
uniform float time;
uniform vec2 mouse_pos;
uniform float zoom;
uniform int tonemap_aces;

uniform float size;
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


float cursor_overlay(float d)
{
  float result = 0;
  if(d < 1 && d > .75)
  {
    result = 1;
  }
  if(size*zoom < 10)
  {
    if(d < 1)
    {
      result = 1;
    }
  }
  return result;
}

vec4 invert(vec4 c)
{
    return vec4(1)-clamp(c,vec4(0),vec4(1)).gbra;
}

void main()
{
  vec4 source = texture2D(texture0, frag_uv);
  vec2 p =  2*(frag_uv-vec2(0.5));  //ndc
  float d = 1000*(1./size)*length(p-mouse_pos);
  vec4 result = source;



  if(tonemap_aces == 1)
  {
    result = Tonemap_ACES(result);
  }

  if(cursor_overlay(d) == 1)
  {
    result = invert(result);
  }
  

  out0 = result;
}