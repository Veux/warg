#pragma once
#include <SDL2/SDL.h>
#include "Forward_Declarations.h"
#include "General_Purpose.h"
#include "Mesh_Loader.h"
#include "Shader.h"
#include "Third_party/stb/stb_image.h"
#include "Physics.h"
#include "Globals.h"

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
  refraction,
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

  const std::string &peek_filename()
  {
    return filename;
  }

  GLenum get_format()
  {
    return format;
  }
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
  bool ibl_mipmaps_started = false;
  bool ibl_mipmaps_generated = false;

  bool is_cubemap = false;
};

struct Texture_Descriptor
{
  Texture_Descriptor() {}
  Texture_Descriptor(const char *filename)
  {
    this->name = filename;
  }
  Texture_Descriptor(std::string filename)
  {
    this->name = filename;
  }

  // solid colors can be specified with "color(r,g,b,a)" float values
  std::string name = "";

  vec4 mod = MISSING_TEXTURE_MOD;
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

  bool is_initialized()
  {
    return initialized;
  }

  // do not modify the handle
  std::shared_ptr<Texture_Handle> texture;

private:
  bool initialized = false;
};
struct Image
{
  Image() {}
  Image(std::string filename);
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
  std::array<Image, 6> sources;
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

/*
when one of the constructors that take arguments is called
load() is called:
it initializes the cubemaps - first checks if the cubemap has already been created/cached and uses the shared ptr for
that if theres no shared ptr for it, it creates a new cubemap and loads the source texture and stalls on it then draws
the cubemap, and drops the source handle, and returns after both cubemaps are constructed, it checks the cubemap handles
for whether or not theyve had their ibl mipmaps generated, and runs that if not.

in both of these some opengl state may be set that isnt unset
cubemap seems good



*/

struct Environment_Map
{
  Environment_Map() {}
  Environment_Map(std::string environment, std::string irradiance, bool equirectangular = true);
  Environment_Map(Environment_Map_Descriptor d);
  Cubemap radiance;
  Cubemap irradiance;
  bool radiance_is_gamma_encoded = true;

  void load();
  void bind(GLuint base_texture_unit, GLuint irradiance_texture_unit,float32 time,vec2 size);

  Environment_Map_Descriptor m;

  // todo: irradiance map generation
  void irradiance_convolution();
  void generate_ibl_mipmaps();

  // todo: rendered environment map
  void probe_world(glm::vec3 p, glm::vec2 resolution);

private:
  //std::shared_ptr<Texture_Handle> ibl_source;
  GLuint ibl_source = 0;
  GLuint ibl_texture_target = 0;
  GLsync ibl_sync= 0;
  GLuint ibl_fbo = 0;
  GLuint ibl_rbo = 0;
  uint32 ibl_tile_max = 15;
  int32 tilex = 0;
  int32 tiley = 0;
  bool working_on_ibl = false;
  float32 time = 0;
  vec2 size = vec2(0);
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
  }
};

struct Material_Descriptor
{
  Material_Descriptor() {}
  void mod_by(const Material_Descriptor *override);
  Texture_Descriptor albedo;
  Texture_Descriptor normal;
  Texture_Descriptor roughness;
  Texture_Descriptor metalness;
  Texture_Descriptor emissive;
  Texture_Descriptor ambient_occlusion;
  Texture_Descriptor tangent; // anisotropic surface roughness    - unused for now
  Uniform_Set_Descriptor uniform_set;
  std::string vertex_shader;
  std::string frag_shader;
  vec2 uv_scale = vec2(1);
  vec2 normal_uv_scale = vec2(1);
  float albedo_alpha_override = -1.f;
  bool backface_culling = true;
  bool uses_transparency = false;
  bool discard_on_alpha = true;
  bool casts_shadows = true;
  bool wireframe = false;
  bool blending = false;

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

private:
  bool reload_from_descriptor = true;
  Material_Descriptor descriptor;
  friend struct Renderer;
  Texture albedo;
  Texture normal;
  Texture emissive;
  Texture roughness;
  Texture metalness;
  Texture ambient_occlusion;
  Shader shader;
  void load();
  void bind();
  void unbind_textures();
  void bind_texture_or_set_default(Texture &t, const char *uniform, const glm::vec4 &default_mod, Texture_Location loc);
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
void run_pixel_shader(Shader *shader, std::vector<Texture> *src_textures, Framebuffer *dst, bool clear_dst = false);
void run_pixel_shader(Shader *shader, std::vector<Texture *> *src_textures, Framebuffer *dst, bool clear_dst = false);

struct Particle
{
  glm::vec3 position;
  glm::quat orientation;
  glm::vec3 velocity;
  glm::vec3 angular_velocity;
  glm::vec3 scale;
  float32 time_to_live;
  float32 time_left_to_live;

  // generic shader-specific per-particle attributes
  glm::vec4 attribute0;
  glm::vec4 attribute1;
  glm::vec4 attribute2;
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
  std::vector<vec4> attributes0;
  std::vector<vec4> attributes1;
  std::vector<vec4> attributes2;

  GLuint instance_mvp_buffer;
  GLuint instance_model_buffer;
  GLuint instance_attribute0_buffer;
  GLuint instance_attribute1_buffer;
  GLuint instance_attribute2_buffer;
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
  Particle_Emission_Type type = stream;

  // available to all types:
  bool snap_to_basis = false;   // particles 'follow' the emitter
  bool inherit_velocity = true; // particles gain the velocity of the emitter when spawned
  bool static_geometry_collision = true;
  bool dynamic_geometry_collision = true;
  float32 simulate_for_n_secs_on_init = 0.0f; // particles will be generated as if the emitter has been on for n seconds
                                              // when emitter is initialized, this is useful for things like fog or any
                                              // object that constantly emits particles
  float32 minimum_time_to_live = 4.0f;
  float32 extra_time_to_live_variance = 1.0f;
  glm::vec3 initial_position_variance = glm::vec3(0);

  // first generated orientation - use to orient within a cone or an entire unit sphere
  // these are args to random_3D_unit_vector: azimuth_min, azimuth_max, altitude_min, altitude_max
  glm::vec4 randomized_orientation_axis = vec4(0.f, two_pi<float32>(), -1.f, 1.f);
  float32 randomized_orientation_angle_variance = 0.f;

  // post-spawn - use to orient the model relative to the emitter:
  glm::vec3 intitial_orientation_axis = glm::vec3(0, 0, 1);
  float32 initial_orientation_angle = 0.0f;

  glm::vec3 initial_scale = glm::vec3(1);
  glm::vec3 initial_extra_scale_variance = glm::vec3(0);
  glm::vec3 initial_velocity = glm::vec3(0, 0, 1);
  glm::vec3 initial_velocity_variance = glm::vec3(0.15, 0.15, 0);
  glm::vec3 initial_angular_velocity = glm::vec3(0);
  glm::vec3 initial_angular_velocity_variance = glm::vec3(0.15, 0.15, 0.15);

  // stream
  bool generate_particles = true;
  float32 particles_per_second = 10.f;

  // explosion
  float32 particle_count = 1000.f;
};
struct Particle_Physics_Method_Descriptor
{
  Particle_Physics_Type type = simple;

  // available to all types
  float32 mass = 1.0f;
  vec3 gravity = vec3(0, 0, -9.8);
  bool oriented_towards_camera = false;
  float32 bounce_min = 0.45;
  float32 bounce_max = 0.75;

  // simple

  // wind
  vec3 direction = vec3(1, .4, .3);
  float32 intensity = 1.0f;

  Octree *octree = nullptr;
};

struct Particle_Emission_Method
{
  virtual void update(Particle_Array *particles, const Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel,
      quat o, float32 time, float32 dt) = 0;
};

struct Particle_Stream_Emission : Particle_Emission_Method
{
  void update(Particle_Array *particles, const Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel, quat o,
      float32 time, float32 dt) final override;
};
struct Particle_Explosion_Emission : Particle_Emission_Method
{
  void update(Particle_Array *p, const Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel, quat o, float32 time,
      float32 dt) final override;
};

struct Particle_Physics_Method
{
  virtual void step(Particle_Array *p, const Particle_Physics_Method_Descriptor *d, float32 t, float32 dt) = 0;
};
struct Wind_Particle_Physics : Particle_Physics_Method
{
  void step(Particle_Array *p, const Particle_Physics_Method_Descriptor *d, float32 t, float32 dt) final override;
};
struct Simple_Particle_Physics : Particle_Physics_Method
{
  void step(Particle_Array *p, const Particle_Physics_Method_Descriptor *d, float32 t, float32 dt) final override;
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
  Particle_Emitter(const Particle_Emitter &rhs);
  Particle_Emitter(Particle_Emitter &&rhs);
  Particle_Emitter &Particle_Emitter::operator=(const Particle_Emitter &rhs);
  Particle_Emitter &Particle_Emitter::operator=(Particle_Emitter &&rhs);
  void init()
  {
    shared_data->particles.init();
  }
  void update(mat4 projection, mat4 camera, float32 dt);
  void clear();
  bool prepare_instance(std::vector<Render_Instance> *accumulator);
  void spin_until_up_to_date() const;
  Mesh_Index mesh_index;
  Material_Index material_index;
  Particle_Emitter_Descriptor descriptor;

//private:
  static std::unique_ptr<Particle_Physics_Method> construct_physics_method(Particle_Emitter_Descriptor d);
  static std::unique_ptr<Particle_Emission_Method> construct_emission_method(Particle_Emitter_Descriptor d);

  struct Physics_Shared_Data
  { // todo :

    /*
    collision
    collision flag for nodes in scene graph,
    collision bounds draw flag in nodes
    proper rigid body physics algorithm


    */
    mat4 projection;
    mat4 camera;
    Particle_Emitter_Descriptor descriptor;        // thread reads
    std::atomic<bool> request_thread_exit = false; // thread reads
    std::atomic<uint64> requested_tick = 0;        // thread reads
    std::atomic<uint64> completed_update = 0;      // thread reads/writes
    Particle_Array particles;                      // thread reads/writes
  };
  static void thread(std::shared_ptr<Physics_Shared_Data> shared_data);
  bool thread_launched = false;
  std::thread t;
  std::unique_ptr<Particle_Physics_Method> physics_method;
  std::unique_ptr<Particle_Emission_Method> emission_method;

  // this is the data on the heap through which the main thread and the particle physics thread communicate
  std::shared_ptr<Physics_Shared_Data> shared_data;
};

struct Renderer
{
  // todo: irradiance map generation
  // todo: water shader
  // todo: skeletal animation
  // todo: particle system/instancing
  // todo: deferred rendering
  // todo: screen space reflections
  // todo: parallax mapping
  // todo: negative glow
  // not describe them
  // with shader3 and materialB  these special effect methods would be hardcoded and the objects would point to them,
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
  uint32 draw_calls_last_frame = 0;
  static mat4 ortho_projection(ivec2 dst_size);
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
  float32 aspect = (float32)window_size.x / (float32)window_size.y;
  float32 znear = 0.1f;
  float32 zfar = 10000;
  vec3 camera_position = vec3(0);
  vec3 camera_gaze = vec3(0);
  vec3 prev_camera_position = vec3(0);
  bool jitter_switch = false;
  mat4 txaa_jitter = mat4(1);

  Framebuffer previous_draw_target; // full render scaled, float linear

  // in order:
  Framebuffer draw_target;       // full render scaled, float linear
  Framebuffer draw_target_srgb8; // full render scaled, srgb
  Framebuffer draw_target_fxaa;  // full render scaled. srgb

  // instance attribute buffers
  // GLuint instance_mvp_buffer = 0;
  // GLuint instance_model_buffer = 0;
  // GLuint instance_attribute0_buffer = 0;
};
