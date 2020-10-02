#version 330
uniform sampler2D texture0;
uniform vec4 texture0_mod = vec4(1, 1, 1, 1);
uniform vec2 mouse_pos;
uniform int mode;
uniform int blendmode;
uniform int hdr;
uniform int brush_selection;
uniform float size;
uniform float intensity;
uniform float brush_exponent;
uniform float i;
uniform float j;
uniform float k;
uniform float time;
uniform float tonemap_pow;
uniform float tonemap_x;
uniform vec4 brush_color = vec4(1, 1, 1, 1);
uniform vec4 mask;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;

float brush0(vec2 p, float d)
{
  d = .1 * pow(d, 1);
  float brush_width = 10.f;
  float t = exp(-85 * pow(d, 2.350f));
  float c = mix(0, .15, clamp(t, 0, 1));
  vec4 color = vec4(c);
  return c;
}

float brush1(vec2 p, float d)
{
  float result = 0;
  if (d < 1)
  {
    result = 1;
  }
  return result;
}

float brush2(vec2 p, float d)
{
  float result = float(0);
  d = max(d, 0.00001);
  result = .051 / d;
  return result;
}

float brush3(vec2 p, float d)
{
  float result = float(0);
  float k1 = .0031250f;
  float h = pow(4 * d, 2.5762);
  result = .0321 * exp(1 - k1 * h);
  return result;
}

float brush4(vec2 p, float d)
{
  float result = float(0);
  float k1 = .50f;
  float h = pow(2 * d, 1);
  result = .21 * exp(1 - k1 * h);
  return result;
}

float brush5(vec2 p, float d)
{
  float result = float(0);
  result = clamp(1 - .8 * d, 0, 1);
  return result;
}

float brush6(vec2 p, float d)
{
  float result = float(0);
  float k = 2.;
  result = 1.0 - pow(max(0, d), k);
  return clamp(result, 0, 1);
}

float brush7(vec2 p, float d)
{
  float result = float(0);
  float size = 8;
  float k = 2.;
  result = pow(cos(.5 * 3.14159 * min(.5 * 3.14, d)), 1);
  return clamp(result, 0, 1);
}

float brush8(vec2 p, float d)
{
  float result = float(0);
  float k = 2.;
  float stime = 0.5 + 0.5 * sin(time * cos(time * 3));
  float dd = (1 + (2 * stime)) * 1 * d;
  result = 1.0 - pow(max(0, dd), k);
  return clamp(result, 0, 1);
}

vec4 Tonemap_ACES(vec4 x)
{
  // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
  const vec4 a = vec4(2.51);
  const vec4 b = vec4(0.03);
  const vec4 c = vec4(2.43);
  const vec4 d = vec4(0.59);
  const vec4 e = vec4(0.14);
  return (x * (a * x + b)) / (x * (c * x + d) + e);
}

float random(vec2 st)
{
  return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}
float noise(vec2 st)
{
  vec2 i = floor(st);
  vec2 f = fract(st);

  // Four corners in 2D of a tile
  float a = random(i);
  float b = random(i + vec2(1.0, 0.0));
  float c = random(i + vec2(0.0, 1.0));
  float d = random(i + vec2(1.0, 1.0));

  vec2 u = f * f * (3.0 - 2.0 * f);

  return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm_h_n(vec2 x, float H, int n)
{
  float G = exp2(-H);
  float f = 1.0;
  float a = 1.0;
  float t = 0.0;
  for (int i = 0; i < n; i++)
  {
    t += a * noise(f * x);
    f *= 2.0;
    a *= G;
  }
  return t;
}
vec2 hash(vec2 p)
{
  p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));

  return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}
float noise_2(vec2 p, float level, float time)
{
  vec2 i = floor(p);
  vec2 f = fract(p);

  vec2 u = f * f * (3.0 - 2.0 * f);
  float t = pow(2., level) * .4 * time;
  mat2 R = mat2(cos(t), -sin(t), sin(t), cos(t));
  if (mod(i.x + i.y, 2.) == 0.)
    R = -R;
  return 2. * mix(mix(dot(hash(i + vec2(0, 0)), (f - vec2(0, 0)) * R), dot(hash(i + vec2(1, 0)), -(f - vec2(1, 0)) * R),
                      u.x),
                  mix(dot(hash(i + vec2(0, 1)), -(f - vec2(0, 1)) * R), dot(hash(i + vec2(1, 1)), (f - vec2(1, 1)) * R),
                      u.x),
                  u.y);
}

float Mnoise(vec2 uv, float level, float time)
{
  // return noise(4.*uv);
  return noise_2(2.f * uv, level, time);
  // return -1. + 2.* (1.-abs(noise(uv,time)));  // flame like
  return -1. + 2. * (abs(noise_2(uv, level, time))); // cloud like
}

float turb(vec2 uv, float time)
{
  float f = 0.0;

  float level = 1.;
  mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
  f = 0.5000 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.2500 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.1250 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.0625 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;
  f += 0.0314 * Mnoise(uv, level, time);
  uv = m * uv;
  level++;

  return abs(f) / .75;
  return abs(f) / .9375;
}

vec4 generate_terrain(vec4 source)
{
  float t1 = 2.5 * turb(0.5 * frag_uv, .025f * time);
  float t2 = 1. * turb(4.0415 * frag_uv, .15f * time);
  float terrain_height = t1 + t2;
  // float terrain_height = .13f * fbm_h_n(vec2(time) + 20.f * frag_uv, 1.5f, 15);
  float biome = 1.1f;
  return vec4(source.r, source.g + terrain_height, biome, 0);
}

vec4 generate_water(vec4 source)
{
  float water_height = .1215f * fbm_h_n(220.f * (vec2(time, time * 0.331f) + random(frag_uv.yx)), .58501f, 2);
  float water_height2 = 0.16135f * fbm_h_n(10.1f * (vec2(time, time * 0.331f) + frag_uv.yx), .391f, 6);
  water_height = 0.1f * random(frag_uv.yx) + water_height2;
  return vec4(water_height + water_height2 * (source.r), source.g, source.b, 0);
}

void main()
{
  vec4 source = texture0_mod * texture2D(texture0, frag_uv);
  vec2 p = 2 * (frag_uv - vec2(0.5));

  float d = 1000. * (1. / size) * length(p - mouse_pos);

  if (mode == 3)
  {
    p = size * (frag_uv - vec2(0.5));
  }
  float brush_t = 0;
  if (brush_selection == 0)
  {
    brush_t = brush0(p, d);
  }
  if (brush_selection == 1)
  {
    brush_t = brush1(p, d);
  }
  if (brush_selection == 2)
  {
    brush_t = brush2(p, d);
  }
  if (brush_selection == 3)
  {
    brush_t = brush3(p, d);
  }
  if (brush_selection == 4)
  {
    brush_t = brush4(p, d);
  }
  if (brush_selection == 5)
  {
    brush_t = brush5(p, d);
  }
  if (brush_selection == 6)
  {
    brush_t = brush6(p, d);
  }
  if (brush_selection == 7)
  {
    brush_t = brush7(p, d);
  }
  if (brush_selection == 8)
  {
    brush_t = brush8(p, d);
  }

  float brush_result = intensity * pow(brush_t, brush_exponent);

  vec4 result_mod_color = brush_result * vec4(brush_color.rgb, 1);
  vec4 result = vec4(0);
  if (mode == 0)
  {
    result = vec4(0);
  }
  if (mode == 1)
  {
    if (blendmode == 0) // mix
      result = mix(source, result_mod_color, brush_t);
    if (blendmode == 1) // blend
      result = mix(source, brush_color, brush_t);
    if (blendmode == 2) // add
      result = source + result_mod_color;
  }
  if (mode == 2)
  {
    result = source - result_mod_color;
  }
  if (mode == 3)
  {
    if (blendmode == 0) // mix
      result = mix(source, result_mod_color, brush_t);
    if (blendmode == 1) // blend
      result = mix(source, brush_color, brush_t);
    if (blendmode == 2) // add
      result = source + result_mod_color;
  }

  if (mode == 4)
  {
    result = tonemap_x * source;
  }

  if (mode == 5)
  {
    result = Tonemap_ACES(source);
  }

  if (mode == 6)
  {
    result = brush_color;
  }
  if (mode == 7)
  {
    result = pow(source, vec4(tonemap_pow));
  }
  if (mode == 8)
  {
    result = generate_terrain(source);
  }
  if (mode == 9)
  {
    result = generate_water(source);
  }
  if (mode == 10)
  {
    result = vec4(0, source.g, source.b, 0);
  }

  if (hdr == 0)
  {
    result = clamp(result, 0, 1);
  }

  result = mix(source, result, mask);
  result.a = 1;
  out0 = result;
}