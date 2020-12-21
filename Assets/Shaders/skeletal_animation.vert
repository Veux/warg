#version 330
//#extension GL_ARB_separate_shader_objects : enable

#define MAX_LIGHTS 10
#define MAX_BONES 128u

uniform sampler2D texture11; // displacement
uniform vec4 texture11_mod;
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
uniform vec2 normal_uv_scale;
uniform mat4 txaa_jitter;
uniform mat4 VP;
uniform mat4 Model;
uniform mat4 shadow_map_transform[MAX_LIGHTS];
uniform mat4 bones[MAX_BONES];

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;
layout(location = 5) in uvec4 bone_index;
layout(location = 6) in vec4 bone_weights;

out vec3 frag_world_position;
out mat3 frag_TBN;
out vec2 frag_uv;
out vec2 frag_normal_uv;
out vec4 frag_in_shadow_space[MAX_LIGHTS];

// assuming max 4 bones per vertex
void main()
{

  vec3 input_pos = position;

  mat4 vertex_to_pose = (bones[bone_index.x] * bone_weights.x);
  vertex_to_pose += (bones[bone_index.y] * bone_weights.y);
  vertex_to_pose += (bones[bone_index.z] * bone_weights.z);
  vertex_to_pose += (bones[bone_index.w] * bone_weights.w);

  float weight_sum = bone_weights[0] + bone_weights[1] + bone_weights[2] + bone_weights[3];

  bool bad_index = false;

  uint ai = bone_index.x;
  uint bi = bone_index.y;
  uint ci = bone_index.z;
  uint di = bone_index.w;
  //
  //  gl_Position.x = bone_index.x;
  //  gl_Position.y = bone_index.y;
  //  gl_Position.z = bone_index.z;
  // index_w = bone_index.w;

  if (ai >= MAX_BONES)
  {
    bad_index = true;
  }

  if (bi >= MAX_BONES)
  {
    bad_index = true;
  }

  if (ci >= MAX_BONES)
  {
    bad_index = true;
  }

  if (di >= MAX_BONES)
  {
    bad_index = true;
  }

  for (int i = 0; i < 4; ++i)
  {
    // if ((bone_index[i] < 0) || (bone_index[i] > MAX_BONES))
    {
      //  bad_index = true;

      // input_pos.z = bone_index[i];
    }

    if (bone_weights[i] < 0 || bone_weights[i] > 1)
    {
      // bad = true;
    }
  }

  if (weight_sum < 0.95f || weight_sum > 1.05f)
  {
    input_pos.x = 55 * sin(time);
  }

  if (bad_index)
  {
    input_pos.z = 50 * sin(time);
  }

  // vertex_to_pose = mat4(1);

  mat4 vertex_to_pose_to_world = Model * vertex_to_pose;

  vec3 t = normalize(vertex_to_pose_to_world * normalize(vec4(tangent, 0))).xyz;
  vec3 b = normalize(vertex_to_pose_to_world * normalize(vec4(bitangent, 0))).xyz;
  vec3 n = normalize(vertex_to_pose_to_world * normalize(vec4(normal, 0))).xyz;

  frag_world_position = (vertex_to_pose_to_world * vec4(position, 1)).xyz;
  frag_TBN = mat3(t, b, n);

  frag_uv = uv_scale * vec2(uv.x, -uv.y);
  frag_normal_uv = normal_uv_scale * frag_uv;
  for (int i = 0; i < MAX_LIGHTS; ++i)
  {
    frag_in_shadow_space[i] = shadow_map_transform[i] * vertex_to_pose_to_world * vec4(position, 1);
  }
  gl_Position = VP * vertex_to_pose_to_world * vec4(input_pos, 1);
}
