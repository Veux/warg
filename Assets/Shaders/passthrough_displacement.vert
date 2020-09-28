#version 330
uniform sampler2D texture11; // displacement
uniform mat4 transform;
uniform bool ground;
uniform uint displacement_map_size;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

out vec4 frag_position;
out vec2 frag_uv;

vec4 get_height_sample(vec2 uv)
{
  vec2 texture_size = vec2(256);
  ivec2 p = ivec2((uv * texture_size) - vec2(0.25f));
  float o = 1. / 1024;
  vec4 height = texture(texture11, uv, 0);
  vec4 height_left = texture(texture11, uv + vec2(-o, 0), 0);
  vec4 height_right = texture(texture11, uv + vec2(o, 0), 0);
  vec4 height_down = texture(texture11, uv + vec2(0, -o), 0);
  vec4 height_up = texture(texture11, uv + vec2(0, o), 0);
  // return height;
  vec4 height_all = height + height_left + height_right + height_down + height_up;
  return .2f * height_all;
}

void main()
{
  float offset = 0.005505;

    // no smoothing
  vec2 uv_offset = uv*(displacement_map_size-1u);
  uv_offset = uv_offset + vec2(.5f);
  uv_offset = uv_offset / displacement_map_size;
  vec2 sample_uv = uv_offset;
  vec4 height_sample = texture2D(texture11, sample_uv);
  vec4 vheight_offsetx = texture2D(texture11, sample_uv + vec2(offset, 0));
  vec4 vheight_offsety = texture2D(texture11, sample_uv + vec2(0, offset));

  float height = height_sample.r;
  float height_offsetx = vheight_offsetx.r;
  float height_offsety = vheight_offsety.r;
  if (ground)
  {
    height = height_sample.g;
    height_offsetx = vheight_offsetx.g;
    height_offsety = vheight_offsety.g;
  }
  float dhx = height_offsetx - height;
  float dhy = height_offsety - height;

  vec3 displacement_vector = normal;
  vec3 displacement_offset = height * displacement_vector;

  vec4 pos = transform * vec4(position + displacement_offset, 1);

  frag_position = pos;
  frag_uv = uv;
  gl_Position = pos;
}
