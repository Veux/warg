#version 330
uniform sampler2D texture0;
uniform vec4 texture0_mod = vec4(1,1,1,1);
uniform vec2 mouse_pos;
uniform int mode;
uniform int blendmode;
uniform int hdr;
uniform int brush_selection;
uniform float size;
uniform float exponent;
uniform float i;
uniform float j;
uniform float k;
uniform float time;
uniform float tonemap_pow;;
uniform float tonemap_x;
uniform vec4 brush_color = vec4(1,1,1,1);
in vec2 frag_uv;

layout(location = 0) out vec4 out0;


float brush0(vec2 p,float d)
{
 // float d = (1-size)*2.5*length(p-mouse_pos);

  d = .1*pow(d,1);

  float brush_width = 10.f;
  float t =  exp(-85*pow(d,2.350f));
  float c = mix(0,.15,clamp(t,0,1)); 
  vec4 color = vec4(c);

  return c;
}

float brush1(vec2 p,float d)
{
  float result = 0;
 // float d = (1-size)*length(p-mouse_pos);

  if(d < 1)
  {
    result = 1;
  }
  return result;
}

float brush2(vec2 p,float d)
{
  float result = float(0);
 // float d = (1-size)*length(p-mouse_pos);

  d = max(d,0.00001);


  result = .051/d;
  
  return result;
}

float brush3(vec2 p,float d)
{
  float result = float(0);
  //float d = (1-size)*100*length(p-mouse_pos);

  float k1 = .0031250f;
  float h = pow(4*d,2.5762);
  result = .0321*exp(1-k1*h);
  
  return result;
}

float brush4(vec2 p,float d)
{
  float result = float(0);
 // float d = (1-size)*100*length(p-mouse_pos);

  float k1 = .50f;
  float h = pow(2*d,1);
  result = .21*exp(1-k1*h);
  
  return result;
}

float brush5(vec2 p,float d)
{
  float result = float(0);
 // float d = (1-size)*10*length(p-mouse_pos);

  
  result = clamp(1-.8*d,0,1);
  
  return result;
}

float brush6(vec2 p,float d)
{
  float result = float(0);
  float k = 2.;
  
 // float d = (1-size)*8*length(p-mouse_pos);

  result = 1.0-pow(max(0,d),k);
  
  return clamp(result,0,1);
}

float brush7(vec2 p,float d)
{
  float result = float(0);
  float size = 8;
  float k = 2.;
  
  //float d = (1-size)*8*length(p-mouse_pos);

  result = pow(cos(.5*3.14159 * min(.5*3.14,d)),1);
  
  return clamp(result,0,1);
}

float brush8(vec2 p,float d)
{
  float result = float(0);
  float k = 2.;
  
  float stime = 0.5+0.5*sin(time*cos(time*3));
  float dd = (1+(2*stime))*1*d;

  result = 1.0-pow(max(0,dd),k);
  
  return clamp(result,0,1);
}

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
  vec4 source = texture0_mod * texture2D(texture0, frag_uv);
  
  vec2 fc = gl_FragCoord.xy/1024;
  //vec2 p = 2*(fc-vec2(0.5));
  vec2 p =  2*(frag_uv-vec2(0.5));

  float d = 1000.*(1./size)*length(p-mouse_pos);

  if(mode == 3)
  {
    p = size*(frag_uv-vec2(0.5));
  }
  float color = 0;
  if(brush_selection == 0)
  {
    color = brush0(p,d);
  }
  if(brush_selection == 1)
  {
    color = brush1(p,d);
  }
  if(brush_selection == 2)
  {
    color = brush2(p,d);
  }
  if(brush_selection == 3)
  {
    color = brush3(p,d);
  }
  if(brush_selection == 4)
  {
    color = brush4(p,d);
  }
  if(brush_selection == 5)
  {
    color = brush5(p,d);
  }
  if(brush_selection == 6)
  {
    color = brush6(p,d);
  }
  if(brush_selection == 7)
  {
    color = brush7(p,d);
  } 
  if(brush_selection == 8)
  {
    color = brush8(p,d);
  }

  vec4 result = color*brush_color;
  
  if(mode == 0)
  {
    result = vec4(0);
  }
  if(mode == 1)
  {
  if(blendmode == 0)
    result = mix(source,result,color);
    if(blendmode == 1)
    result = mix(source,brush_color,color);
    if(blendmode == 2)
    result = source + result;
  }
  if(mode == 2)
  {
   result = source - result;
  }

  
  if(mode == 4)
  {
   result = tonemap_x * source;
  }

    if(mode == 5)
  {
   result = Tonemap_ACES(source);
  }

    if(mode == 6)
  {
    result = brush_color;
  }

  
    if(mode == 7)
  {
    result = pow(source,vec4(tonemap_pow));
  }

  if(hdr == 0)
  {
    result = clamp(result,0,1);
  }
  out0 = result;
  
}