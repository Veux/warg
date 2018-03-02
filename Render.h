#pragma once
#include "Globals.h"
#include "Mesh_Loader.h"
#include "Shader.h"
#include "stb_image.h"
#include <SDL2/SDL.h>
#include <array>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct Framebuffer;

using namespace glm;

enum Texture_Location
{
  albedo,
  specular,
  normal,
  emissive,
  roughness,
  s0, // shadow maps
  s1,
  s2,
  s3,
  s4,
  s5,
  s6,
  s7,
  s8,
  s9
};

struct Renderbuffer_Handle
{
  ~Renderbuffer_Handle();
  GLuint rbo = 0;
  glm::ivec2 size = glm::ivec2(0);
  GLenum format = GL_DEPTH_COMPONENT;
};

struct Texture_Handle
{
  ~Texture_Handle();
  GLuint texture = 0;
  GLenum format = GL_RGBA;
  GLenum filtering = GL_LINEAR;
  GLenum wrap = GL_CLAMP_TO_EDGE;
  glm::ivec2 size = glm::ivec2(0, 0);
  time_t file_mod_t = 0;
};

struct Texture
{
  Texture();
  Texture(std::string path, bool premul = false);
  void load();
  void bind(GLuint texture_unit);
  std::shared_ptr<Texture_Handle> texture;
  std::string file_path;
  glm::ivec2 size = glm::ivec2(0);
  GLenum format = GL_RGBA32F;
  GLenum filtering = GL_LINEAR;
  GLenum wrap = GL_CLAMP_TO_EDGE;
  bool process_premultiply = false;
};

struct Mesh_Handle
{
  ~Mesh_Handle();
  GLuint vao = 0;
  GLuint position_buffer = 0;
  GLuint normal_buffer = 0;
  GLuint uv_buffer = 0;
  GLuint tangents_buffer = 0;
  GLuint bitangents_buffer = 0;
  GLuint indices_buffer = 0;
  GLuint indices_buffer_size = 0;
  void enable_assign_attributes();
  Mesh_Data data;
};

struct Mesh
{
  Mesh();
  Mesh(Mesh_Primitive p, std::string mesh_name);
  Mesh(Mesh_Data mesh_data, std::string mesh_name);
  Mesh(const aiMesh *aimesh, std::string unique_identifier);
  GLuint get_vao() { return mesh->vao; }
  GLuint get_indices_buffer() { return mesh->indices_buffer; }
  GLuint get_indices_buffer_size() { return mesh->indices_buffer_size; }
  std::string name = "NULL";
  // private:
  std::string unique_identifier = "NULL";
  std::shared_ptr<Mesh_Handle> upload_data(const Mesh_Data &data);
  std::shared_ptr<Mesh_Handle> mesh;
};

struct Material_Descriptor
{
  // solid colors can be specified with "color(r,g,b,a)" uint8 values
  std::string albedo;
  std::string roughness;
  std::string specular;  // specular color for conductors   - unused for now
  std::string metalness; // boolean conductor or insulator - unused for now
  std::string tangent;   // anisotropic surface roughness    - unused for now
  std::string normal = "color(0.5,.5,1,0)";
  std::string ambient_occlusion; // unused for now
  std::string emissive;
  std::string vertex_shader = "vertex_shader.vert";
  std::string frag_shader = "fragment_shader.frag";
  vec2 uv_scale = vec2(1);
  uint8 albedo_alpha_override = 0;
  bool backface_culling = true;
  bool uses_transparency = false;
  bool casts_shadows = true;
  // when adding new things here, be sure to add them in the
  // material constructor override section
};

struct Material
{
  Material();
  Material(Material_Descriptor &m);
  Material(aiMaterial *ai_material, std::string working_directory,
      Material_Descriptor *material_override);

  Texture albedo;
  Texture normal;
  Texture emissive;
  Texture roughness;
  Shader shader;
  Material_Descriptor m;

  vec4 albedo_mod = vec4(1.f);
  vec3 emissive_mod = vec3(1.f);
  vec3 roughness_mod = vec3(1.f);

private:
  friend struct Render;
  void load(Material_Descriptor &m);
  void bind(Shader *shader);
  void unbind_textures();
};

enum struct Light_Type
{
  parallel,
  omnidirectional,
  spot
};

struct Light
{
  vec3 position = vec3(0, 0, 0);
  vec3 direction = vec3(0, 0, 0);
  vec3 color = vec3(1, 1, 1);
  vec3 attenuation = vec3(1, 0.22, 0.2);
  float ambient = 0.0004f;
  float cone_angle = .15f;
  Light_Type type;
  bool operator==(const Light &rhs) const;

  bool casts_shadows = false;
  // these take a lot of careful tuning
  // start with max_variance at 0
  // near plane as far away, and far plane as close as possible is critical
  // after that, increase the max_variance up from 0, slowly, right until noise
  // disappears
  int shadow_blur_iterations =
      2; // higher is higher quality, but lower performance
  float shadow_blur_radius = .5f; // preference
  float shadow_near_plane =
      0.1f; // this should be as far as possible without clipping into geometry
  float shadow_far_plane =
      100.f; // this should be as near as possible without clipping geometry
  float max_variance = 0.000001f; // this value should be as low as possible
                                  // without causing float precision noise
  float shadow_fov = glm::radians(90.f); // this should be as low as possible
                                         // without causing artifacts around the
                                         // edge of the light field of view
private:
};

struct Light_Array
{
  bool operator==(const Light_Array &rhs);
  std::array<Light, MAX_LIGHTS> lights;
  vec3 additional_ambient = vec3(0);
  uint32 light_count = 0;
};

// A render entity/render instance is a complete prepared representation of an
// object to be rendered by a draw call
// this should eventually contain the necessary skeletal animation data
struct Render_Entity
{
  Render_Entity(Mesh *mesh, Material *material, mat4 world_to_model);
  mat4 transformation;
  Mesh *mesh;
  Material *material;
  std::string name;
  uint32 ID;
  bool casts_shadows = true;
};

// Similar to Render_Entity, but rendered with instancing
struct Render_Instance
{
  Render_Instance() {}
  Mesh *mesh;
  Material *material;
  std::vector<mat4> MVP_Matrices;
  std::vector<mat4> Model_Matrices;
};

struct Framebuffer_Handle
{
  ~Framebuffer_Handle();
  GLuint fbo = 0;
};

struct Framebuffer
{
  Framebuffer();
  void init();
  void bind();
  std::shared_ptr<Framebuffer_Handle> fbo;
  std::vector<Texture> color_attachments;
  std::shared_ptr<Renderbuffer_Handle> depth;
  glm::ivec2 depth_size = glm::ivec2(0);
  GLenum depth_format = GL_DEPTH_COMPONENT;
  bool depth_enabled = false;
};

struct Gaussian_Blur
{
  Gaussian_Blur();
  void init(ivec2 size, GLenum format);
  void draw(
      Render *renderer, Texture *src, float32 radius, uint32 iterations = 1);
  Shader gaussian_blur_shader;
  Framebuffer target;
  Framebuffer intermediate_fbo;
  float32 aspect_ratio_factor = 1.0f;
  bool initialized = false;
};

struct Spotlight_Shadow_Map
{
  Spotlight_Shadow_Map(){};
  void init(ivec2 size);
  Framebuffer pre_blur;
  mat4 projection_camera;
  bool enabled = false;
  Gaussian_Blur blur;
  GLenum format = GL_RG32F;
  bool initialized = false;
};

struct High_Pass_Filter
{
  High_Pass_Filter();
  void init(ivec2 size, GLenum format);
  void draw(Render *renderer, Texture *src);

  Framebuffer target;
  Shader high_pass_shader;
  bool initialized = false;
};

struct Bloom_Shader
{
  Bloom_Shader();
  void init(ivec2 size, GLenum format);

  // adds the blurred src to dst
  void draw(Render *renderer, Texture *src, Framebuffer *dst);
  Framebuffer target;
  High_Pass_Filter high_pass;
  Gaussian_Blur blur;
  float32 radius = 6.0f;
  uint32 iterations = 15;
  bool initialized = false;
};

void run_pixel_shader(Shader *shader, std::vector<Texture *> *src_textures,
    Framebuffer *dst, bool clear_dst = false);

struct Render
{
  Render(SDL_Window *window, ivec2 window_size);
  ~Render();
  void render(float64 state_time);

  bool use_txaa = false;
  void resize_window(ivec2 window_size);
  float32 get_render_scale() const { return render_scale; }
  float32 get_vfov() { return vfov; }
  void set_render_scale(float32 scale);
  void set_camera(vec3 camera_pos, vec3 dir);
  void set_camera_gaze(vec3 camera_pos, vec3 p);
  void set_vfov(float32 vfov); // vertical field of view in degrees
  SDL_Window *window;
  void set_render_entities(std::vector<Render_Entity> *entities);
  void set_lights(Light_Array lights);
  float64 target_frame_time = 1.0 / 60.0;
  uint64 frame_count = 0;
  vec3 clear_color = vec3(1, 0, 0);
  uint32 draw_calls_last_frame = 0;
  static mat4 ortho_projection(ivec2 &dst_size);

  Mesh quad;
  Shader temporalaa;
  Shader passthrough;
  Shader variance_shadow_map;
  Shader gamma_correction;
  Bloom_Shader bloom;

private:
  Light_Array lights;
  std::array<Spotlight_Shadow_Map, MAX_LIGHTS> spotlight_shadow_maps;
  std::vector<Render_Entity> previous_render_entities;
  std::vector<Render_Entity> render_entities;
  std::vector<Render_Instance> render_instances;
  std::vector<Render_Entity> translucent_entities;
  void set_uniform_lights(Shader &shader);
  void set_uniform_shadowmaps(Shader &shader);
  void build_shadow_maps();
  void opaque_pass(float32 time);
  void instance_pass(float32 time);
  void translucent_pass(float32 time);
  float64 time_of_last_scale_change = 0.;
  void init_render_targets();
  void dynamic_framerate_target();
  mat4 get_next_TXAA_sample();
  float32 render_scale = 1.0f;
  ivec2 window_size; // actual window size
  ivec2 size;        // render target size
  float32 vfov = 60;
  ivec2 shadow_map_size = ivec2(2048, 2048) / 2;
  mat4 camera;
  mat4 projection;
  vec3 camera_position = vec3(0);
  vec3 prev_camera_position = vec3(0);
  bool jitter_switch = false;
  mat4 txaa_jitter = mat4(1);
  Framebuffer previous_frame;

  Framebuffer draw_target;
  GLuint *output_texture = nullptr;

  bool previous_color_target_missing = true;
  GLuint instance_mvp_buffer = 0;   // buffer object holding MVP matrices
  GLuint instance_model_buffer = 0; // buffer object holding model matrices
};
