#version 400
#extension GL_ARB_separate_shader_objects : enable

#define MAX_LIGHTS 10

uniform sampler2D texture11; // displacement
uniform vec4 texture11_mod;
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
uniform vec2 normal_uv_scale;
uniform mat4 txaa_jitter;
uniform mat4 MVP;
uniform mat4 Model;
uniform mat4 shadow_map_transform[MAX_LIGHTS];
uniform bool ground;
uniform uint displacement_map_size;
uniform float derivative_offset;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;

out vec3 frag_world_position;
out mat3 frag_TBN;
out vec2 frag_uv;
out vec2 frag_normal_uv;
out vec4 frag_in_shadow_space[MAX_LIGHTS];
out vec3 model_space_position;
out float ground_height;
out float water_depth;
flat out float biome;
out float height_variance;
out vec4 indebug;

vec4 get_height_sample(vec2 uv)
{
  ivec2 p = ivec2((uv * displacement_map_size) - vec2(0.25f));

  float o = 1. / float(displacement_map_size);
  vec4 height = texture(texture11, uv, 0);
  vec4 height_left = texture(texture11, uv + vec2(-o, 0), 0);
  vec4 height_right = texture(texture11, uv + vec2(o, 0), 0);
  vec4 height_down = texture(texture11, uv + vec2(0, -o), 0);
  vec4 height_up = texture(texture11, uv + vec2(0, o), 0);

  vec4 height_all = height + height_left + height_right + height_down + height_up;
  return height;
  return .2f * height_all;
}

vec4 get_height_sample_variance(vec2 uv)
{
  return vec4(0);
  // unusable until water height doesnt = ground height when 'under'

  ivec2 p = ivec2((uv * displacement_map_size) - vec2(0.25f));

  float o = 1. / float(displacement_map_size);
  vec4 height = texture(texture11, uv, 0);
  vec4 height_left = texture(texture11, uv + vec2(-o, 0), 0);
  vec4 height_right = texture(texture11, uv + vec2(o, 0), 0);
  vec4 height_down = texture(texture11, uv + vec2(0, -o), 0);
  vec4 height_up = texture(texture11, uv + vec2(0, o), 0);

  vec4 height_all = height + height_left + height_right + height_down + height_up;
  vec4 height_all2 = height_left + height_right + height_down + height_up;
  float minimum = min(height.r, min(height_left.r, min(height_right.r, min(height_down.r, height_up.r))));
  float maximum = max(height.r, max(height_left.r, max(height_right.r, max(height_down.r, height_up.r))));

  o = 2. / float(displacement_map_size);
  height = texture(texture11, uv, 0);
  height_left = texture(texture11, uv + vec2(-o, 0), 0);
  height_right = texture(texture11, uv + vec2(o, 0), 0);
  height_down = texture(texture11, uv + vec2(0, -o), 0);
  height_up = texture(texture11, uv + vec2(0, o), 0);

  height_all2 = height_all2 + height_left + height_right + height_down + height_up;

  minimum = min(minimum, min(height.r, min(height_left.r, min(height_right.r, min(height_down.r, height_up.r)))));
  maximum = max(maximum, max(height.r, max(height_left.r, max(height_right.r, max(height_down.r, height_up.r)))));

  o = 3. / float(displacement_map_size);
  height = texture(texture11, uv, 0);
  height_left = texture(texture11, uv + vec2(-o, 0), 0);
  height_right = texture(texture11, uv + vec2(o, 0), 0);
  height_down = texture(texture11, uv + vec2(0, -o), 0);
  height_up = texture(texture11, uv + vec2(0, o), 0);

  height_all2 = height_all2 + height_left + height_right + height_down + height_up;
  minimum = min(minimum, min(height.r, min(height_left.r, min(height_right.r, min(height_down.r, height_up.r)))));
  maximum = max(maximum, max(height.r, max(height_left.r, max(height_right.r, max(height_down.r, height_up.r)))));

  float avg = 1.f / 12.f * height_all2.r;
  float avg_f = abs(height.r - avg);

  float variance = avg_f * (maximum - minimum);

  return vec4(.2f * height_all.xyz, variance);
}

void main()
{

  // float offset = 1.50105f / displacement_map_size;
  float offset = 0.01f * derivative_offset;
  // vec4 height_sample = get_height_sample_variance(uv);
  // vec4 vheight_offsetx = get_height_sample(uv + vec2(offset, 0));
  // vec4 vheight_offsety = get_height_sample(uv + vec2(0, offset));

  // simple
  //   vec4 height_sample = texture(texture11, uv);
  //   vec4 vheight_offsetx = texture(texture11, uv + vec2(offset, 0));
  //   vec4 vheight_offsety = texture(texture11, uv + vec2(0, offset));
  //   vec4 vheight_offsetnx = texture(texture11, uv - vec2(offset, 0));
  //   vec4 vheight_offsetny = texture(texture11, uv - vec2(0, offset));

  // vertex centered on pixel
  // assumes texture is larger by 1 in x and y than the vertex grid
  vec2 uv_offset = uv * (displacement_map_size - 1u);
  uv_offset = uv_offset + vec2(.5f);
  uv_offset = uv_offset / displacement_map_size;
  // vec4 height_sample = texture(texture11, uv_offset);
  vec4 vheight_offsetx = texture(texture11, uv_offset + vec2(offset, 0));
  vec4 vheight_offsety = texture(texture11, uv_offset + vec2(0, offset));
  vec4 vheight_offsetnx = texture(texture11, uv_offset - vec2(offset, 0));
  vec4 vheight_offsetny = texture(texture11, uv_offset - vec2(0, offset));

  // very no smoothing
  vec4 height_sample = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u))), 0);
  // vec4 vheight_offsetx = texelFetch(texture11, ivec2((uv * (displacement_map_size-1u)) + vec2(offset, 0)), 0);
  // vec4 vheight_offsety = texelFetch(texture11, ivec2((uv * (displacement_map_size-1u)) + vec2(0, offset)), 0);
  // vec4 vheight_offsetnx = texelFetch(texture11, ivec2((uv * (displacement_map_size-1u)) - vec2(offset, 0)), 0);
  // vec4 vheight_offsetny = texelFetch(texture11, ivec2((uv * (displacement_map_size-1u)) - vec2(0, offset)), 0);

  vec4 right = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u)) + vec2(1, 0)), 0);
  vec4 up = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u)) + vec2(0, 1)), 0);
  vec4 left = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u)) + vec2(-1, 0)), 0);
  vec4 down = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u)) + vec2(0, -1)), 0);

  vec4 rightup = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u)) + vec2(1, 1)), 0);
  vec4 rightdown = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u)) + vec2(1, -1)), 0);
  vec4 leftup = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u)) + vec2(-1, 1)), 0);
  vec4 leftdown = texelFetch(texture11, ivec2((uv * (displacement_map_size - 1u)) + vec2(-1, -1)), 0);
  indebug = vec4(0);
  float height = height_sample.r;
  if (!ground)
  {
    if (height_sample.r <= height_sample.g)
    {
      vec4 adjacent_waters = vec4(right.r, up.r, left.r, down.r);
      vec4 adjacent_grounds = vec4(right.g, up.g, left.g, down.g);

      vec4 diag_waters = vec4(rightup.r, rightdown.r, leftup.r, leftdown.r);
      vec4 diag_grounds = vec4(rightup.g, rightdown.g, leftup.g, leftdown.g);

      float sum = 0;
      float count = 0;
      for (int i = 0; i < 4; ++i)
      {
        if (adjacent_waters[i] > adjacent_grounds[i])
        {
          sum += adjacent_waters[i];
          count += 1;
        }
        if (diag_waters[i] > diag_grounds[i])
        {
          sum += diag_waters[i];
          count += 1;
        }
      }
      if (count == 0)
      {
      }
      else
      {
        height = sum / count;
      }
    }
  }
  height_variance = get_height_sample_variance(uv).a;
  float height_offsetx = vheight_offsetx.r;
  float height_offsety = vheight_offsety.r;
  float height_offsetnx = vheight_offsetnx.r;
  float height_offsetny = vheight_offsetny.r;
  if (ground)
  {
    // height = texture2D(texture11, uv).g;
    height = height_sample.g;
    height_offsetx = vheight_offsetx.g;
    height_offsety = vheight_offsety.g;
    height_offsetnx = vheight_offsetnx.g;
    height_offsetny = vheight_offsetny.g;
  }

  // right
  float dhx = height_offsetx - height;
  vec3 displacement_tangent_right = normalize(vec3(offset, 0, dhx));
  vec3 displacement_bitangent_right = normalize(vec3(0, 1, 0));
  vec3 normal_right_side = normalize(cross(displacement_tangent_right, displacement_bitangent_right));
  // left
  float dhnx = height_offsetnx - height;
  vec3 displacement_tangent_left = normalize(vec3(offset, 0, -dhnx));
  vec3 displacement_bitangent_left = normalize(vec3(0, 1, 0));
  vec3 normal_left_side = normalize(cross(displacement_tangent_left, displacement_bitangent_left));

  // up
  float dhy = height_offsety - height;
  vec3 displacement_tangent_up = normalize(vec3(1, 0, 0));
  vec3 displacement_bitangent_up = normalize(vec3(0, offset, dhy));
  vec3 normal_up_side = normalize(cross(displacement_tangent_up, displacement_bitangent_up));

  // down
  float dhny = height_offsetny - height;
  vec3 displacement_tangent_down = normalize(vec3(1, 0, 0));
  vec3 displacement_bitangent_down = normalize(vec3(0, offset, -dhny));
  vec3 normal_down_side = normalize(cross(displacement_tangent_down, displacement_bitangent_down));

  vec3 tan_self = vec3(1, 0, 0);
  vec3 bitan_self = vec3(0, 1, 0);
  vec3 n_self = vec3(0, 0, 1);

  vec3 displacement_tangent_sum = normalize(tan_self + displacement_tangent_right + displacement_tangent_left +
                                            displacement_tangent_up + displacement_tangent_down);
  vec3 displacement_bitangent_sum = normalize(bitan_self + displacement_bitangent_right + displacement_bitangent_left +
                                              displacement_bitangent_up + displacement_bitangent_down);
  vec3 displacement_normal_sum =
      normalize(n_self + normal_right_side + normal_left_side + normal_up_side + normal_down_side);

  vec3 displacement_normal = displacement_normal_sum;

  mat3 displacement_TBN = mat3(displacement_tangent_sum, displacement_bitangent_sum, displacement_normal_sum);

  vec3 t = displacement_TBN * normalize(Model * normalize(vec4(tangent, 0))).xyz;
  vec3 b = displacement_TBN * normalize(Model * normalize(vec4(bitangent, 0))).xyz;
  vec3 n = displacement_TBN * normalize(Model * normalize(vec4(normal, 0))).xyz;

  vec3 displacement_vector = normal;
  vec3 displacement_offset = (texture11_mod.r * 1.f * height * displacement_vector);

  frag_TBN = mat3(t, b, n);
  frag_world_position = (Model * vec4(position + displacement_offset, 1)).xyz;
  frag_uv = uv_scale * vec2(uv.x, uv.y);
  frag_normal_uv = normal_uv_scale * frag_uv;
  for (int i = 0; i < MAX_LIGHTS; ++i)
  {
    frag_in_shadow_space[i] = shadow_map_transform[i] * Model * vec4(position + displacement_offset, 1);
  }

  ground_height = texture11_mod.r * height;
  biome = texture11_mod.r * texelFetch(texture11, ivec2((uv * displacement_map_size) - vec2(0.25f)), 0).b;
  if (biome >= 3.f && biome < 4.1f)
  {
    biome = 3.f;
  }

  water_depth = max(height - height_sample.g, 0);

  // indebug = vec4(texture2D(texture11, uv).a);
  model_space_position = position + displacement_offset;
  gl_Position = txaa_jitter * MVP * vec4(position + displacement_offset, 1);
}
