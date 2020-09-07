#version 330
uniform sampler2D texture1;
uniform vec4 texture1_mod = vec4(1, 1, 1, 1);

uniform vec3 emitter_location;
uniform float time;

in vec2 frag_uv;
in vec3 frag_world_position;

layout(location = 0) out vec4 out0;

float random(float x)
{

  return fract(sin(x) * 10000.);
}

float noise(vec2 p)
{

  return random(p.x + p.y * 10000.);
}

vec2 sw(vec2 p)
{
  return vec2(floor(p.x), floor(p.y));
}
vec2 se(vec2 p)
{
  return vec2(ceil(p.x), floor(p.y));
}
vec2 nw(vec2 p)
{
  return vec2(floor(p.x), ceil(p.y));
}
vec2 ne(vec2 p)
{
  return vec2(ceil(p.x), ceil(p.y));
}

float smoothNoise(vec2 p)
{

  vec2 interp = smoothstep(0., 1., fract(p));
  float s = mix(noise(sw(p)), noise(se(p)), interp.x);
  float n = mix(noise(nw(p)), noise(ne(p)), interp.x);
  return mix(s, n, interp.y);
}

float fractalNoise(vec2 p)
{

  float x = 0.;
  x += smoothNoise(p);
  x += smoothNoise(p * 2.) / 2.;
  x += smoothNoise(p * 4.) / 4.;
  x += smoothNoise(p * 8.) / 8.;
  x += smoothNoise(p * 16.) / 16.;
  x /= 1. + 1. / 2. + 1. / 4. + 1. / 8. + 1. / 16.;
  return x;
}

float movingNoise(vec2 p, float time)
{

  float x = fractalNoise(p + time);
  float y = fractalNoise(p - time);
  return fractalNoise(p + vec2(x, y));
}

void main()
{
  vec3 result;
  vec3 fire = vec3(15.f, 3.3f, 0.7f);
  vec3 fire_n = normalize(fire);
  vec3 smoke = vec3(0., 0., 0.);
  vec3 d = frag_world_position - emitter_location;

  float speed = 3.f;
  float sintime = 0.5 * (1. + sin(speed * time));
  float costime = 0.5 * (1. + cos(speed * time));

  // variance = (0.2+sintime)*sin((speed*time)+frag_world_position.x*4.) +
  // (0.2+costime)*cos((speed*time)+frag_world_position.y*4.);

  // variance = 1.f-variance;
  // variance = .325f*variance;
  // vec2 randv =
  // vec2(random(frag_world_position.x+frag_world_position.z),random(frag_world_position.y+frag_world_position.z));

  float variance = movingNoise(3.512 * frag_world_position.xy, 1. * speed * time);
  variance += movingNoise(.12512 * frag_world_position.xy, 1 * speed * time);
  variance = 0.5 * variance;
  variance = pow(variance, 6);
  variance = clamp(variance, 0., 1);
  variance = 1 * variance;
  ;
  variance = 360 * variance;
  // variance = 1.;
  float height_scale = pow(1.192510 * d.z, 2.50f);
  // height_scale = 1;
  float blend_to_smoke = height_scale - variance;
  blend_to_smoke = clamp(blend_to_smoke, 0, 1);
  // 0 is fire, 1 is smoke

  // result = vec3(verticle_scale);

  vec2 uv_center = vec2(0.5, 0.5);

  // bottom left pixel: 0,0

  // fraguv: 0,0  minus uvcenter: 0.5 0.5
  // result: -.5,-.5
  // abs it: .5 .5

  // top right: 1,1 minux uvcenter .5 .5
  // result .5 .5

  vec2 diff = frag_uv - uv_center;
  vec2 centered_uv = frag_uv - uv_center;

  float d_to_center = length(abs(frag_uv - uv_center));
  // d_to_center = pow(d_to_center,2);
  // result = vec3(d_to_center);
  // result = vec3(frag_uv,0);
  // result.g = -result.g;
  // if(result.g < 0)
  {
    // result = vec3(0,1,1);
  }
  vec2 worlduv = frag_world_position.xy;
  vec2 factoruv = worlduv + frag_uv;
  // factoruv.x += frag_world_position.z;

  float distance_factor = 1 - (2.5 * d_to_center);
  // float factor = pow(movingNoise(2*frag_uv,5*time),5);
  float factor2 = pow(movingNoise(2. * factoruv, 5 * time), 7);
  float factor3 = 3 * pow(1 - movingNoise(33. * frag_uv, 5 * time), 1);

  // result = vec3(distance_factor*factor);
  // float angle = atan(centered_uv.y,centered_uv.x);
  // factor = movingNoise(vec2(cos(10.1*angle)),time);
  // float fm = distance_factor - .15*(1.0+sin(angle));

  // result = 3.0*result * factor * factor2 * factor3;

  result = mix(fire_n, smoke, blend_to_smoke);
  result = 160. * result * vec3(distance_factor * factor2 * factor3);
  // result = vec3(factor2*factor3);
  // result = smoothstep(fire_n,smoke,0.5f*vec3(blend_to_smoke));
  // result = 5.5*result*vec3(distance_factor*factor2*factor3);

  // result = vec3(atan((centered_uv.y,2),mod(centered_uv.x));
  // result = vec3(centered_uv,0);

  // result = vec3(1-2.54*d_to_center);

  // result = vec3(height_scale);

  if (blend_to_smoke > 0.955)
  {
    // result = vec3(0,1,0);
  }
  if (result.g < 0)
  {
    // result = vec3(0,1,0);
  }

  result = max(result, vec3(0));
  // result = vec3(frag_uv-uv_center,0);
  // result = texture2D(texture1,frag_uv).rgb;
  // result = clamp(result,vec3(0),vec3(1));

  out0 = vec4(result, 1);
}