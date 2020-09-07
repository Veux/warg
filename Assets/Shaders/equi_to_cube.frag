#version 330 core
uniform sampler2D texture0;

uniform bool gamma_encoded;

in vec3 direction;

layout(location = 0) out vec4 out0;

const float PI = 3.14159;
const vec2 scale = vec2(1.0f / (2.0f * PI), 1.0f / PI);
vec2 direction_to_polar(vec3 v) { return vec2(0.5f*PI+atan(-v.y, v.x), asin(-v.z)); }

vec2 polar_to_uv(vec2 polar)
{// from x=[-pi,pi], y=[-halfpi,halfpi] range to [0,1]
  return (polar*scale) + 0.5f;
}

void main()
{
  vec2 polar = direction_to_polar(normalize(-direction));
  vec2 uv = polar_to_uv(polar);

  vec4 result = texture2D(texture0, uv);
  if(gamma_encoded)
  {
    result = pow(result,vec4(2.2));
    result = 1.*result;
  }
  out0 = result;
  
}