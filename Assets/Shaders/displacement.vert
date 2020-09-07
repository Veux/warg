#version 330
#extension GL_ARB_separate_shader_objects : enable

#define MAX_LIGHTS 10

uniform sampler2D texture11;//displacement
uniform vec4 texture11_mod;
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
uniform vec2 normal_uv_scale;
uniform mat4 txaa_jitter;
uniform mat4 MVP;
uniform mat4 Model;
uniform mat4 shadow_map_transform[MAX_LIGHTS];

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
out float blocking_terrain;
out float ground_height;
out float water_depth;
flat out float biome;
out vec4 indebug;
void main()
{
  float height = texture2D(texture11, uv).r;  
  float offset = 0.0005505;  
  float height_offsetx = texture2D(texture11, uv+vec2(offset,0)).r;
  float height_offsety = texture2D(texture11, uv+vec2(0,offset)).r;
  float dhx = height_offsetx - height;
  float dhy = height_offsety - height;
  vec3 displacement_tangent = normalize(vec3(offset,0,dhx));
  vec3 displacement_bitangent = normalize(vec3(0,offset,dhy));
  vec3 displacement_normal = normalize(cross(displacement_tangent,displacement_bitangent));
  mat3 displacement_TBN = mat3(displacement_tangent, displacement_bitangent, displacement_normal);


  vec3 displacement_vector = normal;


 vec3 t = displacement_TBN*normalize(Model * normalize(vec4(tangent, 0))).xyz;
 vec3 b = displacement_TBN*normalize(Model * normalize(vec4(bitangent, 0))).xyz;
 vec3 n = displacement_TBN*normalize(Model * normalize(vec4(normal,0))).xyz;

  vec3 displacement_offset = (texture11_mod.r*1.f*height*displacement_vector);

  frag_TBN = mat3(t, b, n);
  frag_world_position = (Model * vec4(position+ displacement_offset, 1)).xyz;
  frag_uv = uv_scale *vec2(uv.x, -uv.y);
  frag_normal_uv = normal_uv_scale * frag_uv;
  for(int i = 0; i < MAX_LIGHTS; ++i)
  {
    frag_in_shadow_space[i] = shadow_map_transform[i] * Model * vec4(position, 1);
  }

  ground_height = texture11_mod.r*texture2D(texture11, uv).g;
  biome = texture11_mod.r*texelFetch(texture11, ivec2((uv*256)-vec2(0.25f)),0).b;
  if(biome >= 3.f && biome < 4.1f)
  {
   biome = 3.f;
  }

  blocking_terrain = 0.f;
  
  water_depth = max(height-ground_height,0);
  
  indebug = vec4(texture2D(texture11, uv).a);
  if(ground_height >= height)
  {
    blocking_terrain = height = ground_height;
  }
 
  gl_Position = txaa_jitter* MVP * vec4(position+ displacement_offset, 1);
  
}
