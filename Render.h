#pragma once
#include "Globals.h"
#include "Mesh_Loader.h"
#include "Shader.h"
#include "Third_party/stb/stb_image.h"
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
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;
using namespace glm;

struct Conductor_Reflectivity
{
  static const vec4 iron;
  static const vec4 copper;
  static const vec4 gold;
  static const vec4 aluminum;
  static const vec4 silver;
};

enum Texture_Location
{
  albedo,
  emissive,
  roughness,
  normal,
  metalness,
  ambient_occlusion,
  environment,
  irradiance,

  brdf_ibl_lut,
  t9,
  t10,
  uv_grid,

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
  time_t file_mod_t = 0;
  // this resource's current set settings
  // should only be set by Texture class inside .bind()

  const std::string &peek_filename() { return filename; }

  GLenum get_format() { return format; }
  float imgui_mipmap_setting = 0.f;
  float imgui_size_scale = 1.0f;

  // private:
  //  friend struct Texture;
  //  friend struct Cubemap;
  //  friend struct Environment_Map;
  //  friend struct Renderer;
  //  friend struct SDL_Imgui_State;

  // last bound dynamic state:
  GLenum magnification_filter = GLenum(0);
  GLenum minification_filter = GLenum(0);
  uint32 anisotropic_filtering = 0;
  GLenum wrap_s = GLenum(0);
  GLenum wrap_t = GLenum(0);
  glm::vec4 border_color = glm::vec4(0);

  // should be constant for this handle after initialization:
  std::string filename = "TEXTURE_HANDLE_FILENAME_NOT_SET";
  glm::ivec2 size = glm::ivec2(0, 0);
  GLenum format = GLenum(0);

  // specific for specular environment maps
  bool ibl_mipmaps_generated = false;
};

struct Imgui_Texture_Descriptor
{
  std::shared_ptr<Texture_Handle> ptr = nullptr;
  float mip_lod_to_draw = 0.f;
  bool gamma_encode = false;
  bool is_cubemap = false;
  bool is_mipmap_list_command = false;
};

struct Texture_Descriptor
{
  Texture_Descriptor() {}
  Texture_Descriptor(const char *filename) { this->name = filename; }
  Texture_Descriptor(std::string filename) { this->name = filename; }

  // solid colors can be specified with "color(r,g,b,a)" float values
  std::string name = "";

  vec4 mod = vec4(1);
  glm::ivec2 size = glm::ivec2(0);
  GLenum format = GL_RGBA;
  GLenum magnification_filter = GL_LINEAR;
  GLenum minification_filter = GL_LINEAR_MIPMAP_LINEAR;
  uint32 anisotropic_filtering = MAX_ANISOTROPY;
  GLenum wrap_s = GL_REPEAT;
  GLenum wrap_t = GL_REPEAT;
  glm::vec4 border_color = glm::vec4(0);
  bool process_premultiply = false;
  bool cache_as_unique = true;
};

struct Texture
{
  Texture() {}
  Texture(Texture_Descriptor &td);

  Texture(std::string name, glm::ivec2 size, GLenum format, GLenum minification_filter,
      GLenum magnification_filter = GL_LINEAR, GLenum wraps = GL_CLAMP_TO_EDGE, GLenum wrapt = GL_CLAMP_TO_EDGE,
      glm::vec4 border_color = glm::vec4(0));

  Texture(std::string file_path, bool premul = false);

  Texture_Descriptor t;
  void load();

  void bind(GLuint texture_unit);

  void check_set_parameters();

  // not guaranteed to be constant for every call -
  // handle lifetime only guaranteed for as long as the Texture object
  // is held, and if dynamic texture reloading has not been triggered
  // by a file modification - if it is, it will gracefully display
  // black, or a random other texture
  GLuint get_handle();

  bool is_initialized() { return initialized; }

  // do not modify the handle
  std::shared_ptr<Texture_Handle> texture;

private:
  bool initialized = false;
};

struct Cubemap
{
  Cubemap();
  Cubemap(std::string equirectangular_filename);
  Cubemap(std::array<std::string, 6> filenames);
  void bind(GLuint texture_unit);
  std::array<std::string, 6> filenames;
  std::shared_ptr<Texture_Handle> handle;
  glm::ivec2 size = ivec2(0, 0);
};

struct Environment_Map_Descriptor
{
  Environment_Map_Descriptor() {}
  Environment_Map_Descriptor(std::string environment, std::string irradiance, bool equirectangular = true);
  std::string radiance = "NULL";
  std::string irradiance = "NULL";
  std::array<std::string, 6> environment_faces = {};
  std::array<std::string, 6> irradiance_faces = {};
  bool source_is_equirectangular = true;
};

struct Environment_Map
{
  Environment_Map() {}
  Environment_Map(std::string environment, std::string irradiance, bool equirectangular = true);
  Environment_Map(Environment_Map_Descriptor d);
  Cubemap radiance;
  Cubemap irradiance;

  void load();
  void bind(GLuint base_texture_unit, GLuint irradiance_texture_unit);

  Environment_Map_Descriptor m;

  // todo: irradiance map generation
  void irradiance_convolution();

  void generate_ibl_mipmaps();

  // todo: rendered environment map
  void probe_world(glm::vec3 p, glm::vec2 resolution);
  void blend(Environment_Map &a, Environment_Map &b); //?
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
  Mesh_Descriptor descriptor;
};

struct Mesh
{
  // todo: asynchronous gpu uploading for meshes
  Mesh();
  Mesh(const Mesh_Descriptor &d); // mesh_data must be filled out, except for primitives
  Mesh(const Mesh_Descriptor *d); // mesh_data must be filled out, except for primitives
  GLuint get_vao() { return mesh->vao; }
  GLuint get_indices_buffer() { return mesh->indices_buffer; }
  GLuint get_indices_buffer_size() { return mesh->indices_buffer_size; }
  void draw();
  std::string name = "NULL";
  Mesh_Descriptor get_descriptor()
  {
    if (mesh)
      return mesh->descriptor;
    return Mesh_Descriptor();
  }
  // private:
  std::shared_ptr<Mesh_Handle> upload_data(const Mesh_Data &data);
  std::shared_ptr<Mesh_Handle> mesh;
};

struct Material_Descriptor
{
  void mod_by(const Material_Descriptor *override);
  Texture_Descriptor albedo = Texture_Descriptor("color(1,1,1,1)");
  Texture_Descriptor normal = Texture_Descriptor("color(0.5,.5,1,0)");
  Texture_Descriptor roughness = Texture_Descriptor("color(0.3,0.3,0.3,0.3)");
  Texture_Descriptor metalness = Texture_Descriptor("color(0,0,0,0)");
  Texture_Descriptor emissive = Texture_Descriptor("color(0,0,0,0)");
  Texture_Descriptor ambient_occlusion = Texture_Descriptor("color(1,1,1,1)");

  Texture_Descriptor tangent; // anisotropic surface roughness    - unused for now

  std::string vertex_shader = "vertex_shader.vert";
  std::string frag_shader = "fragment_shader.frag";
  vec2 uv_scale = vec2(1);
  vec2 normal_uv_scale = vec2(1);
  float albedo_alpha_override = -1.f;
  bool backface_culling = true;
  bool uses_transparency = false;
  bool discard_on_alpha = false;
  bool casts_shadows = true;

  /*
  how to import anything from blender to warg with pbr materials:

  1:  open blender
  2:  import your models
  3:  set your textures to the following mappings:

  warg mapping:      -> blender settings
  albedo map         -> diffuse - color
  emissive map       -> diffuse - intensity
  normal map         -> geometry - normal
  roughness map      -> specular - hardness
  metalness map      -> specular - intensity
  ambient_occ map    -> shading - mirror

  note: if a map is missing, make one
  or, leave it blank and it will have a generic default

  note: these will look completely wrong for blender - but we dont care about
  blender we're just hijacking the slots for our own purpose

  4:  save a blender file so you can reload from here
  5:  file -> export -> .fbx
  6:  enable "Scale all..." to the right of the Scale slider (lighter color)
  7:  change Up: to: Z up
  8:  change Forward: to: Y forward
  9:  geometry tab -> check Tangent space
  10: export
  */
};

struct Material
{
  Material();
  Material(Material_Descriptor &m);
  Material_Descriptor descriptor;

private:
  friend struct Renderer;
  Texture albedo;
  Texture normal;
  Texture emissive;
  Texture roughness;
  Texture metalness;
  Texture ambient_occlusion;
  Shader shader;
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
  float32 brightness = 1.0f;
  vec3 color = vec3(1, 1, 1);
  vec3 attenuation = vec3(1, 0.22, 0.0);
  float32 ambient = 0.0004f;
  float radius = 0.1f;
  float32 cone_angle = .15f;
  Light_Type type = Light_Type::omnidirectional;
  bool casts_shadows = false;
  // these take a lot of careful tuning
  // start with max_variance at 0
  // near plane as far away, and far plane as close as possible is critical
  // after that, increase the max_variance up from 0, slowly, right until noise
  // disappears
  int32 shadow_blur_iterations = 2;        // higher is higher quality, but lower performance
  float32 shadow_blur_radius = .5f;        // preference
  float32 shadow_near_plane = 0.1f;        // this should be as far as possible without clipping into geometry
  float32 shadow_far_plane = 100.f;        // this should be as near as possible without clipping geometry
  float32 max_variance = 0.000001f;        // this value should be as low as possible
                                           // without causing float precision noise
  float32 shadow_fov = glm::radians(90.f); // this should be as low as possible
                                           // without causing artifacts around the
                                           // edge of the light field of view
  glm::ivec2 shadow_map_resolution = ivec2(1024);

private:
};

struct Light_Array
{
  Light_Array() {}
  void bind(Shader &shader);
  std::array<Light, MAX_LIGHTS> lights;
  Environment_Map environment =
      Environment_Map_Descriptor(".//Assets/Textures/Environment_Maps/Arches_E_PineTree/radiance.hdr",
          ".//Assets/Textures/Environment_Maps/Arches_E_PineTree/irradiance.hdr");

  uint32 light_count = 0;
};

// A render entity/render instance is a complete prepared representation of an
// object to be rendered by a draw call
// this should eventually contain the necessary skeletal animation data
struct Render_Entity
{
  Render_Entity(Array_String name, Mesh *mesh, Material *material, mat4 world_to_model);
  mat4 transformation;
  Mesh *mesh;
  Material *material;
  Array_String name;
  uint32 ID;
  bool casts_shadows = true;
};

struct World_Object
{
  mat4 transformation;
  Mesh_Descriptor *mesh_descriptor;
  Material_Descriptor *material_descriptor;
  Array_String name;
  uint32 ID;
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
  std::vector<Texture> color_attachments = {Texture()};
  std::shared_ptr<Renderbuffer_Handle> depth;
  glm::ivec2 depth_size = glm::ivec2(0);
  GLenum depth_format = GL_DEPTH_COMPONENT;
  bool depth_enabled = false;
  bool encode_srgb_on_draw = false; // only works for srgb framebuffer
};

struct Gaussian_Blur
{
  Gaussian_Blur();
  void init(Texture_Descriptor &td);
  void draw(Renderer *renderer, Texture *src, float32 radius, uint32 iterations = 1);
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

void run_pixel_shader(Shader *shader, std::vector<Texture *> *src_textures, Framebuffer *dst, bool clear_dst = false);

struct Renderer
{
  // todo: irradiance map generation
  // todo: water shader
  // todo: skeletal animation
  // todo: particle system/instancing
  // todo: deferred rendering
  // todo: refraction
  // todo: screen space reflections
  // todo: parallax mapping
  // todo: negative glow
  Renderer(SDL_Window *window, ivec2 window_size, std::string name);
  ~Renderer();
  void render(float64 state_time);
  std::string name = "Unnamed Renderer";
  bool use_txaa = true;
  bool use_fxaa = true;
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
  static mat4 ortho_projection(ivec2 dst_size);

  Mesh quad;
  Shader temporalaa = Shader("passthrough.vert", "TemporalAA.frag");
  Shader passthrough = Shader("passthrough.vert", "passthrough.frag");
  Shader variance_shadow_map = Shader("passthrough.vert", "variance_shadow_map.frag");
  Shader gamma_correction = Shader("passthrough.vert", "gamma_correction.frag");
  Shader fxaa = Shader("passthrough.vert", "fxaa.frag");
  Shader equi_to_cube = Shader("equi_to_cube.vert", "equi_to_cube.frag");
  Shader gaussian_blur_7x = Shader("passthrough.vert", "gaussian_blur_7x.frag");
  Shader gaussian_blur_15x = Shader("passthrough.vert", "gaussian_blur_15x.frag");
  Shader bloom_mix = Shader("passthrough.vert", "bloom_mix.frag");
  Shader high_pass_shader = Shader("passthrough.vert", "high_pass_filter.frag");
  Shader simple = Shader("vertex_shader.vert", "passthrough.frag");

  Texture uv_map_grid;
  Texture brdf_integration_lut;
  bool previous_color_target_missing = true;

private:
  Light_Array lights;
  std::array<Spotlight_Shadow_Map, MAX_LIGHTS> spotlight_shadow_maps;
  std::vector<Render_Entity> previous_render_entities;
  std::vector<Render_Entity> render_entities;
  std::vector<Render_Instance> render_instances;
  std::vector<Render_Entity> translucent_entities;
  void set_uniform_shadowmaps(Shader &shader);
  void build_shadow_maps();
  void opaque_pass(float32 time);
  void instance_pass(float32 time);
  void translucent_pass(float32 time);
  void postprocess_pass(float32 time);
  Texture translucent_blur_target;
  Texture bloom_target;
  Texture bloom_result;
  Texture bloom_intermediate;
  Framebuffer bloom_fbo;

  float64 time_of_last_scale_change = 0.;
  void init_render_targets();
  void dynamic_framerate_target();
  mat4 get_next_TXAA_sample();
  float32 render_scale = CONFIG.render_scale;
  ivec2 window_size; // actual window size
  ivec2 size;        // render target size
  float32 vfov = CONFIG.fov;
  float shadow_map_scale = CONFIG.shadow_map_scale;
  mat4 camera;
  mat4 previous_camera;
  mat4 projection;
  vec3 camera_position = vec3(0);
  vec3 prev_camera_position = vec3(0);
  bool jitter_switch = false;
  mat4 txaa_jitter = mat4(1);

  Framebuffer previous_draw_target; // full render scaled, float linear

  // in order:
  Framebuffer draw_target;       // full render scaled, float linear
  Framebuffer draw_target_srgb8; // full render scaled, srgb
  Framebuffer draw_target_fxaa;  // full render scaled. srgb

  GLuint instance_mvp_buffer = 0;   // buffer object holding MVP matrices
  GLuint instance_model_buffer = 0; // buffer object holding model matrices
};
