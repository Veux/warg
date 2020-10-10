#pragma once
#include <SDL2/SDL.h>
#include "Forward_Declarations.h"
#include "General_Purpose.h"
#include "Mesh_Loader.h"
#include "Shader.h"
#include "Third_party/stb/stb_image.h"
#include "Physics.h"
#include "Globals.h"
#include "Ui.h"

using json = nlohmann::json;
using namespace glm;

void blit_attachment(Framebuffer &src, Framebuffer &dst);
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
  refraction,
  depth_of_scene,
  displacement,
  water_velocity,
  depth_of_self,
  t13,
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
  time_t file_mod_t = 0;
  // this resource's current set settings
  // should only be set by Texture class inside .bind()

  const std::string &peek_filename()
  {
    return filename;
  }

  GLenum get_format()
  {
    return internalformat;
  }
  float imgui_mipmap_setting = 0.f;
  float imgui_size_scale = 1.0f;
  bool imgui_use_alpha = true;
  GLuint texture = 0;

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
  uint8 levels = 0;
  GLenum internalformat = GLenum(0);

  // specific for specular environment maps
  bool ibl_mipmaps_generated = false;
  GLuint ibl_source = 0;
  GLuint ibl_texture_target = 0;
  GLsync ibl_sync = 0;
  GLuint ibl_fbo = 0;
  GLuint ibl_rbo = 0;
  uint32 ibl_tile_max = 10;
  int32 tilex = 0;
  int32 tiley = 0;
  float32 time = 0;
  bool is_cubemap = false;
  void generate_ibl_mipmaps(float32 time);

  // stream state:
  GLsync upload_sync = 0;
  GLsync transfer_sync = 0;
  GLuint uploading_pbo = 0;
  GLenum datatype = 0;

private:
};

// struct Generated_Texture_Descriptor
//{
//  Generated_Texture_Descriptor(const char *name, GLenum format)
//  {
//    this->name = name;
//    this->format = format;
//  }
//  Generated_Texture_Descriptor(std::string name, GLenum format)
//  {
//    this->name = name;
//    this->format = format;
//  }
//
//  std::string name = "";
//  GLenum format = 0;
//  vec4 mod;
//  GLenum magnification_filter = GL_LINEAR;
//  GLenum minification_filter = GL_LINEAR_MIPMAP_LINEAR;
//  GLenum wrap_s = GL_REPEAT;
//  GLenum wrap_t = GL_REPEAT;
//  glm::vec4 border_color = glm::vec4(0);
//};
struct Texture_Descriptor
{
  Texture_Descriptor() {}
  Texture_Descriptor(const char *filename)
  {
    name = filename;
    source = filename;
    format = GL_SRGB8_ALPHA8;
  }
  Texture_Descriptor(std::string filename)
  {
    name = filename;
    source = filename;
    format = GL_SRGB8_ALPHA8;
  }

  std::string name = "default";
  std::string source = "default";
  std::string key;
  uint8 levels = 1;
  GLenum format = 0;
  glm::ivec2 size = ivec2(0);
  glm::vec4 mod = vec4(1);
  GLenum magnification_filter = GL_LINEAR;
  GLenum minification_filter = GL_LINEAR_MIPMAP_LINEAR;
  GLenum wrap_s = GL_REPEAT;
  GLenum wrap_t = GL_REPEAT;
  glm::vec4 border_color = glm::vec4(0);
};

struct Texture
{
  Texture() {}
  Texture(std::shared_ptr<Texture_Handle> texture);
  Texture(Texture_Descriptor &td);

  Texture(std::string name, glm::ivec2 size, uint8 levels, GLenum format, GLenum minification_filter,
      GLenum magnification_filter = GL_LINEAR, GLenum wraps = GL_CLAMP_TO_EDGE, GLenum wrapt = GL_CLAMP_TO_EDGE,
      glm::vec4 border_color = glm::vec4(0));

  Texture_Descriptor t;
  // a call to load guarantees we will have a Texture_Handle after it returns
  // but the texture will start loading asynchronously and may not yet be ready
  void load();
  bool bind_for_sampling_at(GLuint texture_unit);
  bool is_ready() {}
  void check_set_parameters();
  GLuint get_handle();
  std::shared_ptr<Texture_Handle> texture;

private:
};
struct Image2
{
  Image2() {}
  Image2(std::string filename);
  void rotate90();
  std::string filename = "NULL";
  std::vector<float> data;
  int32 width = 0, height = 0, n = 0;
};
struct Cubemap
{
  Cubemap();
  Cubemap(std::string equirectangular_filename, bool gamma_encoded = false);
  Cubemap(std::array<std::string, 6> filenames);
  void bind(GLuint texture_unit);
  std::shared_ptr<Texture_Handle> handle = nullptr;
  glm::ivec2 size = ivec2(0, 0);

  Texture source;

private:
  void produce_cubemap_from_equirectangular_source();
  void produce_cubemap_from_texture_array();
  std::array<Image2, 6> sources;
  bool is_equirectangular = true;
  bool is_gamma_encoded = false;
};

struct Environment_Map_Descriptor
{
  Environment_Map_Descriptor() {}
  Environment_Map_Descriptor(std::string environment, std::string irradiance, bool equirectangular = true);
  std::string radiance = "NULL";
  std::string irradiance = "NULL";
  std::array<std::string, 6> environment_faces = {""};
  std::array<std::string, 6> irradiance_faces = {""};
  bool source_is_equirectangular = true;
};

// we should have n environment map probes distributed within the world
// these should probably be held within the 'scene graph'
// when an object is gathered for rendering from the graph, we can point to the probes it is affected by
// the probes themselves should be

struct Environment_Probe
{
  Environment_Probe(Environment_Map_Descriptor d) {}
};

// a basic single environment map that is a fallback for all objects
// only one - held by the scene graph
// constructed from equirectangular images
// when the scene graph is initialized, the skybox will be missing/black
// when do we begin to load the data?
// we should have a way to signal to begin loading
// what do we do if rendering is called before we are finished loading?
// we could render them as tiles as before, but we see it stream in very ugly
// we should just use the source mip only, and swap to the correct cubemap only after it is all finished
// with convolution
struct Skybox
{
};

struct Environment_Map
{
  Environment_Map() {}
  Environment_Map(std::string environment, std::string irradiance, bool equirectangular = true);
  Environment_Map(Environment_Map_Descriptor d);
  Cubemap radiance;
  Cubemap irradiance;
  bool radiance_is_gamma_encoded = true;

  void load();
  void bind(GLuint base_texture_unit, GLuint irradiance_texture_unit, float32 time, vec2 size);

  Environment_Map_Descriptor m;

  // todo: rendered environment map
  void probe_world(glm::vec3 p, glm::vec2 resolution);
};

struct Mesh_Handle
{
  ~Mesh_Handle();
  void upload_data();
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
  Mesh();
  Mesh(const Mesh_Descriptor &d);
  Mesh(const Mesh_Descriptor *d);

  void load();

  GLuint get_vao()
  {
    return mesh->vao;
  }
  GLuint get_indices_buffer()
  {
    return mesh->indices_buffer;
  }
  GLuint get_indices_buffer_size()
  {
    return mesh->indices_buffer_size;
  }
  void draw();
  std::string name = "NULL";
  std::shared_ptr<Mesh_Handle> mesh;
};

struct Uniform_Set_Descriptor
{
  std::unordered_map<std::string, float32> float32_uniforms;
  std::unordered_map<std::string, int32> int32_uniforms;
  std::unordered_map<std::string, uint32> uint32_uniforms;
  std::unordered_map<std::string, bool> bool_uniforms;
  std::unordered_map<std::string, glm::vec2> vec2_uniforms;
  std::unordered_map<std::string, glm::vec3> vec3_uniforms;
  std::unordered_map<std::string, glm::vec4> vec4_uniforms;
  std::unordered_map<std::string, glm::mat4> mat4_uniforms;
  std::unordered_map<GLint, Texture> texture_uniforms;
  void clear()
  {
    float32_uniforms.clear();
    int32_uniforms.clear();
    uint32_uniforms.clear();
    bool_uniforms.clear();
    vec2_uniforms.clear();
    vec3_uniforms.clear();
    vec4_uniforms.clear();
    mat4_uniforms.clear();
    texture_uniforms.clear();
  }
};

struct Material_Descriptor
{
  // Material_Descriptor() {}
  void mod_by(const Material_Descriptor *override);
  Texture_Descriptor albedo;
  Texture_Descriptor normal;
  Texture_Descriptor roughness;
  Texture_Descriptor metalness;
  Texture_Descriptor emissive;
  Texture_Descriptor ambient_occlusion;
  Texture_Descriptor displacement;
  Texture_Descriptor tangent; // anisotropic surface roughness    - unused for now
  Uniform_Set_Descriptor uniform_set;
  std::string vertex_shader;
  std::string frag_shader;
  vec2 uv_scale = vec2(1);
  vec2 normal_uv_scale = vec2(1);
  float32 albedo_alpha_override = -1.f;
  float32 derivative_offset = 0.008;
  bool backface_culling = true;
  bool translucent_pass = false;
  bool discard_on_alpha = true;
  bool casts_shadows = true;
  bool wireframe = false;
  bool fixed_function_blending = false;

  /*

  //forget this... its not stable
  //back to the old plan
  //name the objects in blender
  //if they have the same name prefix such as Cube and Cube.001
  //use only the prefix Cube, and append _albedo.png
  //objects with the same named prefix will get the same textures
  //and we never have to deal with material import problems
  //
  //so, todo: dont bother reading materials from assimp at all
  //just manually set the texture filenames based on the object name
  //perhaps we can add our warg specific attributes in to blender
  //so we can have them set properly on load
  //alternatively, a text file in the asset directory
  //also, need to have every mesh also supply a collision mesh
  //we determine what the collision mesh is by the collision_<name> prefix
  //the names will be used to match the collision mesh with the render mesh

  //also implement a basic spatial partition - a 3d grid of chunks, put the triangles that intersect the chunks into it
  //particles can just check the single chunk theyre in for collision
  //make a wireframe renderer pass, can do it after bloom and with depth checking disabled













  how to import anything from blender to warg with pbr materials:

  1:  open blender
  2:  import your models
  3:  set your textures to the following mappings:

  warg mapping:      -> blender settings
  albedo map         -> diffuse - color
  emissive map       -> diffuse - intensity   (new: emission)
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
  const Material_Descriptor *get_descriptor() const
  {
    return &descriptor;
  }
  Material_Descriptor *get_modifiable_descriptor()
  {
    reload_from_descriptor = true;
    return &descriptor;
  }

  bool reload_from_descriptor = true;
  Material_Descriptor descriptor;
  friend struct Renderer;
  Texture albedo;
  Texture normal;
  Texture emissive;
  Texture roughness;
  Texture metalness;
  Texture ambient_occlusion;
  Texture displacement;
  Shader shader;
  void load();
  void bind();
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
  float32 ambient = 0.000014f;
  float32 radius = .1f;
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
  Light_Array()
  {
    for (auto &l : light_spheres)
    {
      l = Node_Index(NODE_NULL);
    }
  }
  void bind(Shader &shader);
  std::array<Light, MAX_LIGHTS> lights;
  std::array<Node_Index, MAX_LIGHTS> light_spheres;
  Environment_Map_Descriptor environment;
  uint32 light_count = 0;
};

// A render entity/render instance is a complete prepared representation of an
// object to be rendered by a draw call
// this should eventually contain the necessary skeletal animation data
struct Render_Entity
{
  Render_Entity() {}
  Render_Entity(Array_String name, Mesh *mesh, Material *material, mat4 world_to_model, Node_Index node_index);
  mat4 transformation;
  Mesh *mesh;
  Material *material;
  Array_String name;
  uint32 ID;
  Node_Index node = NODE_NULL;
  bool casts_shadows = true;
};

struct World_Object
{
  mat4 transformation;
  Mesh_Descriptor *mesh_descriptor;
  Material_Descriptor *material_descriptor;
  Array_String name;
  uint32 ID;
  Node_Index node = NODE_NULL;
};

// Similar to Render_Entity, but rendered with instancing
struct Render_Instance
{
  Render_Instance() {}
  Mesh *mesh;
  Material *material;
  GLuint mvp_buffer = 0;
  GLuint model_buffer = 0;
  GLuint attribute0_buffer = 0;
  GLuint attribute1_buffer = 0;
  GLuint attribute2_buffer = 0;
  GLuint attribute3_buffer = 0;
  GLuint instance_billboard_location_buffer = 0;
  bool use_attribute0 = false;
  bool use_attribute1 = false;
  bool use_attribute2 = false;
  bool use_attribute3 = false;
  bool use_billboarding = false;
  GLuint size = 0;
};
// struct Buffer_Handle
//{
//  ~Buffer_Handle() { glDeleteBuffers(1, &buf); }
//  GLuint buf = 0;
//};

// struct Buffer
//{
//  Buffer()
//  {
//    buffer = std::make_shared<Buffer_Handle>();
//    glGenBuffers(1, &buffer->buf);
//  }
//  ~Buffer() {}
//  Buffer(const Buffer &rhs)
//  {
//    buffer = std::make_shared<Buffer_Handle>();
//    glGenBuffers(1, &buffer->buf);
//    glBufferData()
//    glBindBuffer(GL_COPY_READ_BUFFER, rhs.buffer->buf);
//    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer->buf);
//    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, rhs.size);
//    size = rhs.size;
//  }
//  Buffer(Buffer &&rhs)
//  {
//    std::swap(buffer, rhs.buffer);
//    std::swap(size, rhs.size);
//  }
//  Buffer &operator=(const Buffer &rhs)
//  {
//    buffer = std::make_shared<Buffer_Handle>();
//    glGenBuffers(1, &buffer->buf);
//    glBindBuffer(GL_COPY_READ_BUFFER, rhs.buffer->buf);
//    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer->buf);
//    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, rhs.size);
//    size = rhs.size;
//  }
//  Buffer &operator=(Buffer &&rhs)
//  {
//    buffer = rhs.buffer;
//    size = rhs.size;
//    rhs.buffer = nullptr;
//    rhs.size = 0;
//  }
//
//  std::shared_ptr<Buffer_Handle> buffer;
//  GLuint size = 0;
//};

struct Framebuffer_Handle
{
  ~Framebuffer_Handle();
  GLuint fbo = 0;
};

struct Framebuffer
{
  Framebuffer();
  void init();
  void bind_for_drawing_dst();
  std::shared_ptr<Framebuffer_Handle> fbo;
  std::vector<Texture> color_attachments = {Texture()};
  std::shared_ptr<Renderbuffer_Handle> depth;
  Texture depth_texture;
  glm::ivec2 depth_size = glm::ivec2(0);
  GLenum depth_format = GL_DEPTH_COMPONENT;
  bool depth_enabled = false;
  bool use_renderbuffer_depth = true;
  std::vector<GLenum> draw_buffers;
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
void run_pixel_shader(Shader *shader, std::vector<Texture> *src_textures, Framebuffer *dst, bool clear_dst = false);
void run_pixel_shader(Shader *shader, std::vector<Texture *> *src_textures, Framebuffer *dst, bool clear_dst = false);

struct Particle
{
  glm::vec3 position;
  glm::quat orientation;
  glm::vec3 velocity;
  glm::vec3 angular_velocity;
  glm::vec3 scale;
  float32 lifespan;
  float32 time_left_to_live;
  bool billboard;
  bool billboard_lock_z;
  float32 billboard_angle;
  float32 billboard_rotation_velocity;
  float32 distance_to_camera;

  // generic shader-specific per-particle attributes
  glm::vec4 attribute0;
  glm::vec4 attribute1;
  glm::vec4 attribute2;
  glm::vec4 attribute3;
  bool use_attribute0 = false;
  bool use_attribute1 = false;
  bool use_attribute2 = false;
  bool use_attribute3 = false;
};
struct Particle_Array
{
  Particle_Array() {}
  void init();
  void destroy();
  ~Particle_Array()
  {
    ASSERT(!initialized); // destroy wasnt called
  }

  // todo: copy/move functions
  Particle_Array(Particle_Array &rhs) = delete;
  Particle_Array(Particle_Array &&rhs);
  Particle_Array &operator=(Particle_Array &rhs);
  Particle_Array &operator=(Particle_Array &&rhs);

  void compute_attributes(mat4 projection, mat4 camera);
  bool prepare_instance(std::vector<Render_Instance> *accumulator);
  std::vector<Particle> particles;

  std::vector<mat4> MVP_Matrices;
  std::vector<mat4> Model_Matrices;
  std::vector<vec4> billboard_locations;
  std::vector<vec4> attributes0;
  std::vector<vec4> attributes1;
  std::vector<vec4> attributes2;
  std::vector<vec4> attributes3;
  bool use_attribute0 = false;
  bool use_attribute1 = false;
  bool use_attribute2 = false;
  bool use_attribute3 = false;

  bool use_billboarding = false;

  GLuint instance_mvp_buffer = 0;
  GLuint instance_model_buffer = 0;
  GLuint instance_billboard_location_buffer = 0;
  GLuint instance_attribute0_buffer = 0;
  GLuint instance_attribute1_buffer = 0;
  GLuint instance_attribute2_buffer = 0;
  GLuint instance_attribute3_buffer = 0;
  bool initialized = false;
};

/*
a particle emitter needs to generate particles via an interface that
controls all the desired variables such as

mesh to emit
material of mesh
initial position and velocity
spread
emission rate
time to live
if 2d, always facing camera or not
function ptr for a custom physics interaction mathod
function ptr for custom shader variables/animation such as color or scale transitions with time? or all handled in the
custom shader itself? opaque or translucent or additive instance rendering method opaque: depth test on, depth write on
translucent and additive: depth test on, depth write off

ideally, the particle physics would be done on the gpu with a compute shader

requires jsonification for configuration of emitter



the renderer wants a Render_Instance
which has a Mesh* Material* and stdvectors for MVP and Model matrices

*/

enum Particle_Physics_Type
{
  simple,
  wind
};

enum Particle_Emission_Type
{
  stream,
  explosion
};

struct Particle_Emission_Method_Descriptor
{
  Particle_Emission_Type type = Particle_Emission_Type::stream;

  // available to all types:
  bool snap_to_basis = false;   // particles 'follow' the emitter
  bool inherit_velocity = true; // particles gain the velocity of the emitter when spawned
  float32 simulate_for_n_secs_on_init = 0.0f; // particles will be generated as if the emitter has been on for n seconds
                                              // when emitter is initialized, this is useful for things like fog or any
                                              // object that constantly emits particles
  float32 minimum_time_to_live = 4.0f;
  float32 extra_time_to_live_variance = 1.0f;
  vec3 initial_position_variance = vec3(0);
 

  // first generated orientation - use to orient within a cone or an entire unit sphere
  // these are args to random_3D_unit_vector: azimuth_min, azimuth_max, altitude_min, altitude_max
  vec4 randomized_orientation_axis = vec4(0.f, two_pi<float32>(), -1.f, 1.f);
  float32 randomized_orientation_angle_variance = 0.f;

  // post-spawn - use to orient the model relative to the emitter:
  glm::vec3 intitial_orientation_axis = glm::vec3(0, 0, 1);
  float32 initial_orientation_angle = 0.0f;

  glm::vec3 initial_scale = glm::vec3(1);
  glm::vec3 initial_extra_scale_variance = glm::vec3(0);
  float32 initial_extra_scale_uniform_variance = 0.0f;
  glm::vec3 initial_velocity = glm::vec3(0, 0, 0);
  glm::vec3 initial_velocity_variance = glm::vec3(0.15, 0.15, 0.15);
  glm::vec3 initial_angular_velocity = glm::vec3(0);
  glm::vec3 initial_angular_velocity_variance = glm::vec3(0.15, 0.15, 0.15);

  float32 billboard_initial_angle = 0.f;
  float32 billboard_rotation_velocity = 0.f; //spin the billboard
  float32 initial_billboard_rotation_velocity_variance = 0.f; //spin the billboard
  bool billboard_lock_z = false;
  bool billboarding = false;
  uint32 particles_per_spawn = 1;

  // stream
  bool generate_particles = true;
  float32 particles_per_second = 10.f;

  // explosion
  uint32 explosion_particle_count = 1000;
  float32 boom_t = 0.f;
  float32 power = 1.0f;
  bool low_discrepency_position_variance = false;
  bool hammersley_sphere = true; //true for sphere, false for hemisphere
  vec3 impulse_center_offset_min = vec3(0);//centered on emitter position
  vec3 impulse_center_offset_max = vec3(0);//centered on emitter position
  float32 hammersley_radius = 1.0f;
  bool enforce_velocity_position_offset_match = false; // all particles will go 'exactly out' from emitter


  //generics
  bool use_attribute0 = false;
  bool use_attribute1 = false;
  bool use_attribute2 = false;
  bool use_attribute3 = false;
  vec4 initial_state_of_attribute0 = vec4(0);
  vec4 initial_state_of_attribute1 = vec4(0);
  vec4 initial_state_of_attribute2 = vec4(0);
  vec4 initial_state_of_attribute3 = vec4(0);
  void* data0 = nullptr;
  uint32 size0 = 0;
  void* data1 = nullptr;
  uint32 size1 = 0;
};
struct Particle_Physics_Method_Descriptor
{
  Particle_Physics_Type type = Particle_Physics_Type::simple;

  bool static_geometry_collision = true;
  bool dynamic_geometry_collision = true;
  bool abort_when_late = true;


  // available to all types
  float32 mass = 1.0f;
  vec3 gravity = vec3(0, 0, -9.8);
  float32 bounce_min = 0.45;
  float32 bounce_max = 0.75;
  float32 size_multiply_uniform_min = 1.f;
  float32 size_multiply_uniform_max = 1.f;
  vec3 size_multiply_min = vec3(1);
  vec3 size_multiply_max = vec3(1);
  vec3 die_when_size_smaller_than = vec3(0);
  vec3 friction = vec3(1);
  float32 stiction_velocity = 0.005f;
  float32 billboard_rotation_velocity_multiply = 1.f; //should be good for growing smoke particles
  Octree* static_octree = nullptr;
  Octree* dynamic_octree = nullptr;

  // simple

  // wind
  vec3 direction = vec3(1, .4, .3);
  float32 wind_intensity = 1.0f;



  //generics
  vec4 iteration_of_attribute0 = vec4(0);
  vec4 iteration_of_attribute1 = vec4(0);
  vec4 iteration_of_attribute2 = vec4(0);
  vec4 iteration_of_attribute3 = vec4(0);
  void* data0 = nullptr;
  uint32 size0 = 0;
  void* data1 = nullptr;
  uint32 size1 = 0;
};
//todo make an emission method "onto heightmap"
//can spawn them all in one go, but then turn them on/off each frame based on dist?
//alternatively deterministically spawn them each frame in a radius..
//and a physics method that locks them in place but shears with wind
//and fades/despawns at distance

struct Particle_Emitter_Descriptor;
struct Physics_Shared_Data
{ // todo :proper rigid body physics algorithm
  mat4 projection;
  mat4 camera;
  Timer idle = Timer(100u);
  Timer active = Timer(100u);
  Timer per_static_octree_test = Timer(100000);
  Timer per_dynamic_octree_test = Timer(100000);

  uint32 static_collider_count_max = 0;
  uint32 static_collider_count_sum = 0;
  uint32 static_collider_count_samples = 0;

  uint32 dynamic_collider_count_max = 0;
  uint32 dynamic_collider_count_sum = 0;
  uint32 dynamic_collider_count_samples = 0;

  Particle_Emitter_Descriptor* descriptor = nullptr;        // thread reads/writes
  std::atomic<bool> request_thread_exit = false; // thread reads
  std::atomic<uint64> requested_tick = 0;        // thread reads
  std::atomic<uint64> completed_update = 0;      // thread reads/writes
  Particle_Array particles;                      // thread reads/writes
};

//a warning about these - the methods are allowed to modify the descriptors themselves
//however we should never modify the descriptors elsewhere if the emitter is not spun up to date
//or else there will be race conditions on the descriptor values
//only change values in the descriptors if you are certain that the emitter is up to date
//for example, it is a new state update tick and you have yet to call update on the emitter - the
//emitter would have been synchronized at the end of the update...?
struct Particle_Emission_Method
{
  virtual void update(Particle_Array *particles, Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel,
      quat o, float32 time, float32 dt, Physics_Shared_Data* data) = 0;
};
struct Particle_Stream_Emission : Particle_Emission_Method
{
  void update(Particle_Array *particles, Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel, quat o,
      float32 time, float32 dt, Physics_Shared_Data* data) final override;
};
struct Particle_Explosion_Emission : Particle_Emission_Method
{
  void update(Particle_Array *p, Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel, quat o, float32 time,
      float32 dt, Physics_Shared_Data* data) final override;
};


struct Particle_Physics_Method
{
  virtual void step(Particle_Array *p, Particle_Physics_Method_Descriptor *d, float32 t, float32 dt, Physics_Shared_Data* data) = 0;
};
struct Wind_Particle_Physics : Particle_Physics_Method
{
  void step(Particle_Array *p, Particle_Physics_Method_Descriptor *d, float32 t, float32 dt, Physics_Shared_Data* data) final override;
};
struct Simple_Particle_Physics : Particle_Physics_Method
{
  void step(Particle_Array *p, Particle_Physics_Method_Descriptor *d, float32 t, float32 dt, Physics_Shared_Data* data) final override;
};

struct Particle_Emitter_Descriptor
{
  Particle_Emission_Method_Descriptor emission_descriptor;
  Particle_Physics_Method_Descriptor physics_descriptor;

  // for emitter itself:
  glm::vec3 position = glm::vec3(0);
  glm::vec3 velocity = glm::vec3(0);
  glm::quat orientation = angleAxis(0.f, vec3(1, 0, 0)); // glm::quat(0, 0, 1, 0);
};

struct Particle_Emitter
{
  Particle_Emitter();
  Particle_Emitter(Particle_Emitter_Descriptor d, Mesh_Index m, Material_Index mat);
  Particle_Emitter(Particle_Emitter &rhs);
  Particle_Emitter(Particle_Emitter &&rhs);
  Particle_Emitter &Particle_Emitter::operator=(Particle_Emitter &rhs);
  Particle_Emitter &Particle_Emitter::operator=(Particle_Emitter &&rhs);
  void init()
  {
    shared_data->particles.init();
  }
  void update(mat4 projection, mat4 camera, float32 dt);
  void clear();
  bool prepare_instance(std::vector<Render_Instance> *accumulator);
  void fence();
  Mesh_Index mesh_index = NODE_NULL;
  Material_Index material_index = NODE_NULL;
  Particle_Emitter_Descriptor descriptor;

  //don't actually use these as timers directly, they are being overwritten by the thread's timers when we fence
  Timer idle = Timer(100u);
  Timer active = Timer(100u);
  Timer per_static_octree_test = Timer(100000);
  Timer per_dynamic_octree_test = Timer(100000);
  uint32 static_collider_count_max = 0;
  uint32 static_collider_count_sum = 0;
  uint32 static_collider_count_samples = 0;
  uint32 dynamic_collider_count_max = 0;
  uint32 dynamic_collider_count_sum = 0;
  uint32 dynamic_collider_count_samples = 0;
  

  // private:
  static std::unique_ptr<Particle_Physics_Method> construct_physics_method(Particle_Emitter_Descriptor d);
  static std::unique_ptr<Particle_Emission_Method> construct_emission_method(Particle_Emitter_Descriptor d);


  static void thread(std::shared_ptr<Physics_Shared_Data> shared_data);
  bool thread_launched = false;
  std::thread t;
  std::unique_ptr<Particle_Physics_Method> physics_method;
  std::unique_ptr<Particle_Emission_Method> emission_method;

  // this is the data on the heap through which the main thread and the particle physics thread communicate
  std::shared_ptr<Physics_Shared_Data> shared_data;
};

// todo:
// browse file and load from disk
// ability to select out of any texture in the cache to edit
// put a door in blades edge and put it on the map

struct Texture_Paint
{
  Texture_Paint() {}
  void run(std::vector<SDL_Event> *imgui_event_accumulator);
  void iterate(Texture *t, float32 time);
  Texture create_new_texture(glm::ivec2 size, const char *name = nullptr);
  Texture *change_texture_to(int32 index);
  bool window_open = true;
  Shader drawing_shader;
  Texture display_surface;
  std::vector<Texture> textures;
  Shader postprocessing_shader;
  void preset_pen();
  void preset_pen2();
  // private:
  Framebuffer fbo_drawing;
  Framebuffer fbo_intermediate;
  Framebuffer fbo_preview;
  Framebuffer fbo_display;
  Shader copy;
  Mesh quad;
  Texture intermediate;
  Texture preview;
  float32 zoom = .40f;
  int32 selected_texture = 0;
  bool hdr = true;
  bool initialized = false;
  int8 clear = 0;
  int8 apply_exposure = false;
  bool apply_tonemap = false;
  int32 brush_selection = 1;
  int32 blendmode = 1;
  vec4 brush_color = vec4(1);
  vec4 clear_color = vec4(0.05);
  float32 intensity = 1.0f;
  float32 exponent = 1.0f;
  float32 size = 155.0f;
  float32 exposure_delta = 0.1f;
  vec2 last_drawn_ndc = vec2(0, 0);
  vec2 last_seen_mouse_ndc = vec2(0);
  bool is_new_click = true;
  float32 accumulator = 0.0f;
  float32 density = 25.f;
  float32 pow = 1.5f;
  float32 previous_speed = 0.0f;
  float32 last_run_visit_time = 0.0f;
  bool apply_pow = false;
  bool constant_density = true;
  int32 smoothing_count = 1;
  bool postprocess_aces = 0;
  uint32 imgui_visit_count = 0;
  bool save_dialogue = false;
  int32 save_type_radio_button_state = 0;
  std::string filename;
  float32 sim_time = 0.f;
  ivec4 mask = ivec4(1);
  bool draw_cursor = true;
  ivec2 new_texture_size = ivec2(1024);
  int32 custom_draw_mode_set = 0;
  File_Picker texture_picker = File_Picker(".");
};

struct Liquid_Surface
{
  Liquid_Surface() {}
  void init(State *state, vec3 pos, float32 scale, ivec2 resolution);
  void run(State *state);
  ~Liquid_Surface();
  void set_heightmap(Texture texture);
  void generate_geometry_from_heightmap(vec4 *heightmap_pixel_array = nullptr, vec4 *velocity_pixel_array = nullptr);
  bool finish_texture_download_and_generate_geometry(); // version without the pixel copy
  void start_texture_download();
  bool finish_texture_download();
  void zero_velocity();
  bool apply_geometry_to_octree_if_necessary(Flat_Scene_Graph *scene);
  Texture_Paint painter;
  Node_Index ground = NODE_NULL;
  Node_Index water = NODE_NULL;
  int32 iterations = 0;
  float64 my_time = 0.f;
  vec3 ambient_wave_scale = vec3(30.f, 3.f, 1.0f);
  // private:
  GLuint heightmap_pbo = 0;
  GLuint velocity_pbo = 0;
  GLsync read_sync = 0;
  std::vector<vec4> heightmap_pixels;
  std::vector<vec4> velocity_pixels;
  bool generate_terrain_from_heightmap = false;
  Mesh_Descriptor terrain_geometry;
  uint32 last_applied_terrain_tick = 0;
  uint32 last_generated_terrain_tick = 0;
  ivec2 last_generated_terrain_geometry_resolution;
  Shader liquid_shader;
  Framebuffer heightmap_fbo;
  Framebuffer velocity_fbo;
  Framebuffer copy_heightmap_fbo;
  Framebuffer copy_velocity_fbo;
  Framebuffer liquid_shader_fbo;

  // for resized downloading:
  Framebuffer heightmap_resize_fbo;
  Framebuffer velocity_resize_fbo;
  Texture height_dst;
  Texture velocity_dst;
  ivec2 desired_download_resolution = ivec2(HEIGHTMAP_RESOLUTION);

  Texture heightmap;
  Texture velocity;
  Texture velocity_copy;
  Texture heightmap_copy;
  Mesh quad;
  uint32 frames_until_check = 33;
  State *state = nullptr;
  ivec2 heightmap_resolution;
  uint32 current_buffer_size = 0;

  bool window_open = false;

  bool invalidated = true;
  bool initialized = false;
};

mat4 fullscreen_quad();
struct Renderer
{

  // todo: forward+ rendering

  // the way it seems to work is that lights themselves are placed into a voxelized frustrum
  // strong lights will touch more than 1 voxel, but each light will only be placed
  // into the voxels it is likely to affect
  // and then the pixels in the pixel shader only ask for the lights in the voxel it actually 
  // occupies and instead of the actual light data being duplicated, its an index into a 
  // buffer array, a 'pointer' to the light data it wants

  //if you have a hundred teeny tiny lights in the distance, and one big sun that touches all
  //pixels, almost all pixels will do only 1 light computation, and only the ones close to the lights will do 2 or very rarely more


  // todo: irradiance map generation
  // todo: skeletal animation
  // todo: screen space reflections
  // todo: parallax mapping
  // todo: procedural raymarched volumetric effects
  // todo: godrays
  // todo: fbm noise in water depth
  // todo: rain particles
  // todo: spawn object gui - manual entry for new node and its resource indices
  // todo: dithering in tonemapper
  // todo: instanced grass, use same heightmap to displace - can resize biome to 1/2 res and draw jittered grass at each
  // grass pixel
  Renderer() {}
  Renderer(SDL_Window *window, ivec2 window_size, std::string name);
  ~Renderer();
  void render(float64 state_time);
  std::string name = "Unnamed Renderer";
  bool use_txaa = false;
  bool use_fxaa = true;
  void resize_window(ivec2 window_size);
  float32 get_render_scale() const
  {
    return render_scale;
  }
  float32 get_vfov()
  {
    return vfov;
  }

  vec3 ray_from_screen_pixel(ivec2 pixel)
  {
    pixel.y = window_size.y - pixel.y;
    const vec2 pixel_normalized = vec2(pixel) / vec2(window_size);
    const vec2 screen_ndc = 2.0f * (pixel_normalized - vec2(0.5f));
    // set_message("cursor_ndc:", s(vec4(screen_ndc, 0, 0)), 1.0f);
    mat4 inv = glm::inverse(projection * camera);
    return normalize(inv * vec4(screen_ndc, 1, 1));
  }
  void set_render_scale(float32 scale);
  void set_camera(vec3 camera_pos, vec3 dir);
  void set_camera_gaze(vec3 camera_pos, vec3 p);
  void set_vfov(float32 vfov); // vertical field of view in degrees
  SDL_Window *window;
  void set_render_entities(std::vector<Render_Entity> *entities);

  float64 target_frame_time = 1.0 / 60.0;
  uint64 frame_count = 0;
  vec3 clear_color = vec3(1, 0, 0);
  float32 blur_radius = 0.0021;
  float32 blur_factor = 2.12f;
  bool show_tonemap = false;
  float exposure = 1.0f;
  vec3 exposure_color = vec3(1);
  uint32 draw_calls_last_frame = 0;

  Environment_Map environment;

  bool imgui_this_tick = false;
  bool show_renderer_window = true;
  bool show_imgui_fxaa = false;
  bool show_bloom = false;
  void draw_imgui();
  Mesh quad;
  Mesh cube;
  Shader temporalaa = Shader("passthrough.vert", "TemporalAA.frag");
  Shader passthrough = Shader("passthrough.vert", "passthrough.frag");
  Shader tonemapping = Shader("passthrough.vert", "tonemapping.frag");
  Shader variance_shadow_map = Shader("passthrough.vert", "variance_shadow_map.frag");
  Shader variance_shadow_map_displacement = Shader("passthrough_displacement.vert", "variance_shadow_map.frag");
  Shader gamma_correction = Shader("passthrough.vert", "gamma_correction.frag");
  Shader fxaa = Shader("passthrough.vert", "fxaa.frag");
  Shader equi_to_cube = Shader("equi_to_cube.vert", "equi_to_cube.frag");
  Shader gaussian_blur_7x = Shader("passthrough.vert", "gaussian_blur_7x.frag");
  Shader gaussian_blur_15x = Shader("passthrough.vert", "gaussian_blur_15x.frag");
  Shader bloom_mix = Shader("passthrough.vert", "bloom_mix.frag");
  Shader high_pass_shader = Shader("passthrough.vert", "high_pass_filter.frag");
  Shader simple = Shader("vertex_shader.vert", "passthrough.frag");
  Shader skybox = Shader("vertex_shader.vert", "skybox.frag");
  Shader refraction = Shader("vertex_shader.vert", "refraction.frag");
  Shader water = Shader("vertex_shader.vert", "water.frag");
  void bind_white_to_all_textures();

  Texture uv_map_grid;
  Texture brdf_integration_lut;

  bool previous_color_target_missing = true;

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
  void skybox_pass(float32 time);
  void copy_to_primary_framebuffer_and_txaa(float32 time);
  void use_fxaa_shader(float32 state_time);
  Texture bloom_target;
  Texture bloom_result;
  Texture bloom_intermediate;
  Framebuffer bloom_fbo;
  uint32 bloom_mip_levels;
  float64 time_of_last_scale_change = 0.;
  void init_render_targets();
  void dynamic_framerate_target();
  mat4 get_next_TXAA_sample();
  float32 render_scale = CONFIG.render_scale;
  ivec2 window_size;        // actual window size
  ivec2 render_target_size; // render target size
  float32 vfov = CONFIG.fov;
  mat4 camera;
  mat4 previous_camera;
  mat4 projection;
  float32 aspect = (float32)window_size.x / (float32)window_size.y;
  float32 znear = 0.1f;
  float32 zfar = 10000;
  vec3 camera_position = vec3(0);
  vec3 camera_gaze = vec3(0);
  vec3 prev_camera_position = vec3(0);
  bool jitter_switch = false;
  mat4 txaa_jitter = mat4(1);
  bool do_depth_prepass = true;
  Framebuffer previous_draw_target; // full render scaled, float linear

  // in order:
  Framebuffer draw_target;
  Framebuffer tonemapping_target_srgb8;
  Framebuffer fxaa_target_srgb8;
  Framebuffer translucent_sample_source;
  Framebuffer self_object_depth;

  // instance attribute buffers
  // GLuint instance_mvp_buffer = 0;
  // GLuint instance_model_buffer = 0;
  // GLuint instance_attribute0_buffer = 0;
};
