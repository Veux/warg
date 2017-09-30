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

void INIT_RENDERER();
void CLEANUP_RENDERER();

using namespace glm;
struct Texture_Handle
{
  ~Texture_Handle();
  GLuint texture = 0;
  time_t file_mod_t = 0;
};
struct FBO_Handle
{
  ~FBO_Handle();
  GLuint fbo = 0;
};
struct Spotlight_Shadow_Map
{
  Spotlight_Shadow_Map() {};
  Spotlight_Shadow_Map(ivec2 size);
  Texture_Handle color;
  Texture_Handle depth;
  FBO_Handle target;
  mat4 projection_camera;
  bool enabled = false;
};




struct Texture
{
  Texture();
  Texture(std::string path, bool premul = false);

private:
  friend struct Render;
  friend struct Material;
  void load();
  void bind(const char *name, GLuint location, Shader &shader);
  std::shared_ptr<Texture_Handle> texture;
  std::string file_path;

  GLenum storage_type = GL_RGBA;
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
  Mesh_Data data;
};

struct Mesh
{
  Mesh();
  Mesh(Mesh_Primitive p, std::string mesh_name);
  Mesh(Mesh_Data mesh_data, std::string mesh_name);
  Mesh(const aiMesh *aimesh, std::string unique_identifier);
  void bind_to_shader(Shader &shader);
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
  std::string albedo;
  std::string roughness;
  std::string specular;  // specular color for conductors   - unused for now
  std::string metalness; // boolean conductor or insulator - unused for now
  std::string tangent;   // anisotropic surface roughness    - unused for now
  std::string normal;
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
  Material(Material_Descriptor m);
  Material(aiMaterial *ai_material, std::string working_directory,
           Material_Descriptor *material_override);

private:
  friend struct Render;
  void load(Material_Descriptor m);
  void bind();
  void unbind_textures();
  Texture albedo;
  // Texture specular_color;
  Texture normal;
  Texture emissive;
  Texture roughness;
  Shader shader;
  Material_Descriptor m;
};

enum Light_Type
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
  float ambient = 0.0f;
  float cone_angle = 1.0f;
  Light_Type type;
  bool operator==(const Light &rhs) const;
  bool casts_shadows = false;
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
  Render_Entity(Mesh *mesh, Material *material,
                mat4 world_to_model);
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
struct Render
{
  Render(SDL_Window *window, ivec2 window_size);
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

private:
  Light_Array lights;
  std::array<Spotlight_Shadow_Map, MAX_LIGHTS> spotlight_shadow_maps;
  std::vector<Render_Entity> previous_render_entities;
  std::vector<Render_Entity> render_entities;
  std::vector<Render_Instance> render_instances;
  std::vector<Render_Entity> translucent_entities;
  void set_uniform_lights(Shader &shader);
  void set_uniform_shadowmaps(Shader& shader);
  void build_shadow_maps();
  void opaque_pass(float32 time);
  void instance_pass(float32 time);
  void translucent_pass(float32 time);
  float64 time_of_last_scale_change = 0.;
  void init_render_targets();
  void dynamic_framerate_target();
  mat4 get_next_TXAA_sample();
  float32 render_scale = 1.0f; 
  ivec2 window_size;            // actual window size
  ivec2 size;                   // render target size
  float32 vfov = 60;
  mat4 camera;
  mat4 projection;
  vec3 camera_position = vec3(0);
  vec3 prev_camera_position = vec3(0);
  bool jitter_switch = false;
  mat4 txaa_jitter = mat4(1);
};
