#include "Globals.h"
#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "Mesh_Loader.h"
#include "Render.h"
#include "Shader.h"
#include "Timer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unordered_map>
#include <vector>
using namespace glm;
using namespace std;
using namespace gl33core;
// our frag shader output attachment points for deferred rendering
#define DIFFUSE_TARGET GL_COLOR_ATTACHMENT0
#define DEPTH_TARGET GL_COLOR_ATTACHMENT1
#define NORMAL_TARGET GL_COLOR_ATTACHMENT2
#define POSITION_TARGET GL_COLOR_ATTACHMENT3

const GLenum RENDER_TARGETS[] = {DIFFUSE_TARGET};
#define TARGET_COUNT sizeof(RENDER_TARGETS) / sizeof(GLenum)

static Timer FRAME_TIMER = Timer(60);
static Timer SWAP_TIMER = Timer(60);
static GLuint TARGET_FRAMEBUFFER = 0; // fbo that gets rendered to
static GLuint COLOR_TARGET_TEXTURE =
    0; // color texture that is bound to target framebuffer
static GLuint DEPTH_TARGET_TEXTURE =
    0; // depth texture that is bound to target framebuffer
static GLuint PREV_COLOR_TARGET = 0; // color texture from previous frame
static bool PREV_COLOR_TARGET_MISSING = true;
static GLuint INSTANCE_MVP_BUFFER = 0;   // buffer object holding MVP matrices
static GLuint INSTANCE_MODEL_BUFFER = 0; // buffer object holding model matrices
static Mesh QUAD;
static Shader TEMPORALAA;
static Shader PASSTHROUGH;
static Shader VARIANCE_SHADOW_MAP;
static Shader GAUSSIAN_BLUR;
static GLuint SCRATCH_FRAMEBUFFER = 0;
static GLuint GAUSSIAN_INTERMEDIATE;
static ivec2 CURRENT_GAUSSIAN_SIZE = vec2(0);
GLenum CURRENT_GAUSSIAN_FORMAT = GL_RGBA;
static bool INIT = false;
static std::unordered_map<std::string, std::weak_ptr<Mesh_Handle>> MESH_CACHE;
static std::unordered_map<std::string, std::weak_ptr<Texture_Handle>>
    TEXTURE_CACHE;

  
void check_FBO_status();

void INIT_RENDERER()
{
  set_message("INIT_RENDERER()");
  if (INIT)
  {
    set_message("Renderer already initialized");
    return;
  }
  INIT = true;
  set_message("Initializing renderer...");
  set_message("Creating renderer QUAD");

  // the uid for the renderer's quad must be different than the
  // planes used in the game world because that plane
  // will have its instance attributes enabled when it gets bound in the
  // renderer loop and  the passthrough shader doesn't have attribute slots for
  // them

  // alternatively enable/disable the attributes every frame as needed
  Mesh_Data quad_data = load_mesh_plane();
  quad_data.unique_identifier = "RENDERER's PLANE";
  QUAD = Mesh(quad_data, "RENDERER's PLANE");

  set_message("Creating TXAA and PASSTHROUGH shaders");
  TEMPORALAA = Shader("passthrough.vert", "TemporalAA.frag");
  PASSTHROUGH = Shader("passthrough.vert", "passthrough.frag");
  VARIANCE_SHADOW_MAP = Shader("passthrough.vert", "variance_shadow_map.frag");
  GAUSSIAN_BLUR = Shader("passthrough.vert","gaussian_blur.frag");

  set_message("Initializing instance buffers");
  // instancing MVP Matrix buffer init
  const GLuint mat4_size = sizeof(GLfloat) * 4 * 4;
  const GLuint instance_buffer_size = MAX_INSTANCE_COUNT * mat4_size;

  glGenBuffers(1, &INSTANCE_MVP_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MVP_BUFFER);
  glBufferData(GL_ARRAY_BUFFER, instance_buffer_size, (void *)0,
               GL_DYNAMIC_DRAW);

  // instancing Model Matrix buffer init
  glGenBuffers(1, &INSTANCE_MODEL_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MODEL_BUFFER);
  glBufferData(GL_ARRAY_BUFFER, instance_buffer_size, (void *)0,
               GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  set_message("Renderer init finished");
}
void CLEANUP_RENDERER()
{
  set_message("CLEANUP_RENDERER()");
  QUAD = Mesh();
  TEMPORALAA = Shader();
  PASSTHROUGH = Shader();

  set_message("Deleting 2 FBOs, 3 textures, 2 instance buffers:",
              s(TARGET_FRAMEBUFFER, " ", SCRATCH_FRAMEBUFFER ,"",COLOR_TARGET_TEXTURE, " ",
                PREV_COLOR_TARGET, " ", DEPTH_TARGET_TEXTURE, " ",
                INSTANCE_MVP_BUFFER, " ", INSTANCE_MODEL_BUFFER));

  glDeleteFramebuffers(1, &TARGET_FRAMEBUFFER);
  glDeleteFramebuffers(1, &SCRATCH_FRAMEBUFFER);
  glDeleteTextures(1, &COLOR_TARGET_TEXTURE);
  glDeleteTextures(1, &PREV_COLOR_TARGET);
  glDeleteTextures(1,&GAUSSIAN_INTERMEDIATE);
  glDeleteRenderbuffers(1, &DEPTH_TARGET_TEXTURE);
  glDeleteBuffers(1, &INSTANCE_MVP_BUFFER);
  glDeleteBuffers(1, &INSTANCE_MODEL_BUFFER);
}

void check_and_clear_expired_textures()
{
  auto it = TEXTURE_CACHE.begin();
  while (it != TEXTURE_CACHE.end())
  {
    auto texture_handle = *it;
    const std::string *path = &texture_handle.first;
    struct stat attr;
    stat(path->c_str(), &attr);
    time_t t = attr.st_mtime;

    auto ptr = texture_handle.second.lock();
    if (ptr) // texture could exist but not yet be loaded
    {
      time_t t1 = ptr.get()->file_mod_t;
      if (t1 != t)
      {
        it = TEXTURE_CACHE.erase(it);
        continue;
      }
    }
    ++it;
  }
}

void save_and_log_screen()
{
  static uint64 i = 0;
  ++i;

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  GLint width = viewport[2];
  GLint height = viewport[3];

  SDL_Surface *surface = SDL_CreateRGBSurface(
      0, width, height, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
  if (surface)
  {
    std::string name = s("screen_", i, ".png");
    set_message("Saving Screenshot: ", " With name: " + name);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                 surface->pixels);
    IMG_SavePNG(surface, name.c_str());
    SDL_FreeSurface(surface);
  }
  else
  {
    set_message("surface creation failed in save_and_log_screen()");
  }
}

void save_and_log_texture(GLuint texture)
{
  static uint64 i = 0;
  ++i;

  glBindTexture(GL_TEXTURE_2D, texture);

  GLint width = 0;
  GLint height = 0;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

  SDL_Surface *surface = SDL_CreateRGBSurface(
      0, width, height, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
  if (surface)
  {
    std::string name = s("Texture object ", texture, " #", i, ".png");
    set_message("Saving Texture: ", s(texture, " With name: ", name));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    IMG_SavePNG(surface, name.c_str());
    SDL_FreeSurface(surface);
  }
  else
  {
    set_message("surface creation failed in save_and_log_texture()");
  }
}

void dump_gl_float32_buffer(GLenum target, GLuint buffer, uint32 parse_stride)
{
  set_message("Dumping buffer: ", s(buffer));
  glBindBuffer(target, buffer);
  GLint size = 0;
  glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size); // bytes
  set_message("size in bytes: ", s(size));
  uint8 *data = new uint8[size];
  glGetBufferSubData(target, 0, size, data);

  float32 *fp = (float32 *)data;
  uint32 count = size / sizeof(float32);

  std::string result = "\n";
  for (uint32 i = 0; i < count;)
  {
    if (i + parse_stride > count)
    {
      set_message("warning: stride likely wrong in dump buffer: size mismatch. "
                  "missing data in this dump");
      break;
    }
    result += "(";
    for (uint32 j = 0; j < parse_stride; ++j)
    {
      float32 f = fp[i + j];
      result += s(f);
      if (j != parse_stride - 1)
      {
        result += ",";
      }
    }
    result += ")";
    i += parse_stride;
  }

  set_message("GL buffer dump: ", result);
}


Texture::Texture() { file_path = ERROR_TEXTURE_PATH; }
Texture_Handle::~Texture_Handle()
{
  set_message("Deleting texture: ", s(texture));
  glDeleteTextures(1, &texture);
  texture = 0;
}
Texture::Texture(std::string path, bool premul)
{
  process_premultiply = premul;
  path = fix_filename(path);
  if (path.size() == 0)
  {
    texture = nullptr;
    return;
  }
  if (path.substr(0, 6) == "color(")
  { // custom color
    file_path = path;
    load();
    return;
  }

  if (path.find_last_of("/") == path.npos)
  { // no specified directory, so use base path
    file_path = BASE_TEXTURE_PATH + path;
    load();
    return;
  }
  else
  { // assimp imported model or user specified a directory
    file_path = path;
    load();
    return;
  }
}

void Texture::load()
{
  if (file_path == "" || file_path == BASE_TEXTURE_PATH)
  {
    // a null texture here shows as 0,0,0,0 in the shader
    texture = nullptr;
    return;
  }
  auto ptr = TEXTURE_CACHE[file_path].lock();
  if (ptr)
  {
    texture = ptr;
    return;
  }
  texture = std::make_shared<Texture_Handle>();
  TEXTURE_CACHE[file_path] = texture;
  int32 width, height, n;
  if (file_path.substr(0, 6) == "color(")
  {
    width = height = 1;
    Uint32 color = string_to_color(file_path);
    glGenTextures(1, &texture->texture);
    glBindTexture(GL_TEXTURE_2D, texture->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, storage_type, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_INT_8_8_8_8, &color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    return;
  }
  auto *data = stbi_load(file_path.c_str(), &width, &height, &n, 4);
  if (!data)
  { // error loading file...
#if DYNAMIC_TEXTURE_RELOADING
    // retry next frame
    set_message("Warning: missing texture:" + file_path);
    texture = nullptr;
    return;
#else
    if (!data)
    {
      set_message("STBI failed to find or load texture: " + file_path);
      file_path = "";
      texture = nullptr;
      return;
    }
#endif
  }

  set_message("Texture load cache miss. Texture from disk: ", file_path, 1.0);
  struct stat attr;
  stat(file_path.c_str(), &attr);
  texture.get()->file_mod_t = attr.st_mtime;

  if (process_premultiply)
  {
    for (uint32 i = 0; i < width * height; ++i)
    {
      uint32 pixel = data[i];
      uint8 r = (uint8)(0x000000FF & pixel);
      uint8 g = (uint8)((0x0000FF00 & pixel) >> 8);
      uint8 b = (uint8)((0x00FF0000 & pixel) >> 16);
      uint8 a = (uint8)((0xFF000000 & pixel) >> 24);

      r = round(r * ((float)a / 255));
      g = round(g * ((float)a / 255));
      b = round(b * ((float)a / 255));

      data[i] = (24 << a) | (16 << b) | (8 << g) | r;
    }
  }
  glGenTextures(1, &texture->texture);
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, storage_type, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
  glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(data);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::bind(GLuint binding)
{
#if DYNAMIC_TEXTURE_RELOADING
  load();
#endif
  //set_message(s("binding texture: ",this->file_path," handle:", texture ? texture->texture : 0," to unit:", binding));
  glActiveTexture(GL_TEXTURE0 + binding);
  glBindTexture(GL_TEXTURE_2D, texture ? texture->texture : 0);
}

Mesh::Mesh() {}
Mesh::Mesh(Mesh_Primitive p, std::string mesh_name) : name(mesh_name)
{

  unique_identifier = identifier_for_primitive(p);
  auto ptr = MESH_CACHE[unique_identifier].lock();
  if (ptr)
  {
    // assert that the data is actually exactly the same
    mesh = ptr;
    return;
  }
  set_message("caching mesh with uid: ", unique_identifier);
  MESH_CACHE[unique_identifier] = mesh = upload_data(load_mesh(p));
  mesh->enable_assign_attributes();
}
Mesh::Mesh(Mesh_Data data, std::string mesh_name) : name(mesh_name)
{
  unique_identifier = data.unique_identifier;
  if (unique_identifier == "NULL")
  { // lets not cache custom meshes thx
    mesh = upload_data(data);
    mesh->enable_assign_attributes();
    return;
  }
  auto ptr = MESH_CACHE[unique_identifier].lock();
  if (ptr)
  {
    // assert that the data is actually exactly the same
    mesh = ptr;
    return;
  }
  set_message("caching mesh with uid: ", unique_identifier);
  MESH_CACHE[unique_identifier] = mesh = upload_data(data);
  mesh->enable_assign_attributes();
}
Mesh::Mesh(const aiMesh *aimesh, std::string unique_identifier)
{
  ASSERT(aimesh);
  this->unique_identifier = unique_identifier;
  auto ptr = MESH_CACHE[unique_identifier].lock();
  if (ptr)
  {
    // assert that the data is actually exactly the same
    mesh = ptr;
    return;
  }
  set_message("caching mesh with uid: ", unique_identifier);
  MESH_CACHE[unique_identifier] = mesh =
      upload_data(load_mesh(aimesh, unique_identifier));  
      mesh->enable_assign_attributes();
}



void Mesh_Handle::enable_assign_attributes()
{
  if (WARG_SERVER)
    return;

  ASSERT(vao);
  glBindVertexArray(vao);

  uint32 loc = 0;
  glEnableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
  loc = 1;
  glEnableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
  loc = 2;
  glEnableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
  glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(float32) * 2, 0);
  loc = 3;
  glEnableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, tangents_buffer);
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
  loc = 4;
  glEnableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, bitangents_buffer);
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
}

std::shared_ptr<Mesh_Handle> Mesh::upload_data(const Mesh_Data &mesh_data)
{
  std::shared_ptr<Mesh_Handle> mesh = std::make_shared<Mesh_Handle>();
  mesh->data = mesh_data;
  if (WARG_SERVER)
    return mesh;


  if (sizeof(decltype(mesh_data.indices)::value_type) != sizeof(uint32))
  {
    set_message(
        "Mesh::upload_data error: render() assumes index type to be 32 bits");
    ASSERT(0);
  }

  mesh->indices_buffer_size = mesh_data.indices.size();
  glGenVertexArrays(1, &mesh->vao);
  glBindVertexArray(mesh->vao);
  set_message("uploading mesh data...", s("created vao: ", mesh->vao), 1);
  glGenBuffers(1, &mesh->position_buffer);
  glGenBuffers(1, &mesh->normal_buffer);
  glGenBuffers(1, &mesh->indices_buffer);
  glGenBuffers(1, &mesh->uv_buffer);
  glGenBuffers(1, &mesh->tangents_buffer);
  glGenBuffers(1, &mesh->bitangents_buffer);

  uint32 positions_buffer_size = mesh_data.positions.size();
  uint32 normal_buffer_size = mesh_data.normals.size();
  uint32 uv_buffer_size = mesh_data.texture_coordinates.size();
  uint32 tangents_size = mesh_data.tangents.size();
  uint32 bitangents_size = mesh_data.bitangents.size();
  uint32 indices_buffer_size = mesh_data.indices.size();

  ASSERT(all_equal(positions_buffer_size, normal_buffer_size, uv_buffer_size,
                   tangents_size, bitangents_size));

  // positions
  uint32 buffer_size = mesh_data.positions.size() *
                       sizeof(decltype(mesh_data.positions)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->position_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.positions[0],
               GL_STATIC_DRAW);

  // normals
  buffer_size = mesh_data.normals.size() *
                sizeof(decltype(mesh_data.normals)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->normal_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.normals[0],
               GL_STATIC_DRAW);

  // texture_coordinates
  buffer_size = mesh_data.texture_coordinates.size() *
                sizeof(decltype(mesh_data.texture_coordinates)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.texture_coordinates[0],
               GL_STATIC_DRAW);

  // indices
  buffer_size = mesh_data.indices.size() *
                sizeof(decltype(mesh_data.indices)::value_type);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer_size, &mesh_data.indices[0],
               GL_STATIC_DRAW);

  // tangents
  buffer_size = mesh_data.tangents.size() *
                sizeof(decltype(mesh_data.tangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->tangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.tangents[0],
               GL_STATIC_DRAW);

  // bitangents
  buffer_size = mesh_data.bitangents.size() *
                sizeof(decltype(mesh_data.bitangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->bitangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.bitangents[0],
               GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  return mesh;
}

Material::Material() {}
Material::Material(Material_Descriptor m) { load(m); }
Material::Material(aiMaterial *ai_material, std::string working_directory,
                   Material_Descriptor *material_override)
{

  if (WARG_SERVER)
    return;

  ASSERT(ai_material);
  const int albedo_n = ai_material->GetTextureCount(aiTextureType_DIFFUSE);
  const int specular_n = ai_material->GetTextureCount(aiTextureType_SPECULAR);
  const int emissive_n = ai_material->GetTextureCount(aiTextureType_EMISSIVE);
  const int normal_n = ai_material->GetTextureCount(aiTextureType_NORMALS);
  const int roughness_n = ai_material->GetTextureCount(aiTextureType_SHININESS);

  // unused:
  aiTextureType_HEIGHT;
  aiTextureType_OPACITY;
  aiTextureType_DISPLACEMENT;
  aiTextureType_AMBIENT;
  aiTextureType_LIGHTMAP;

  Material_Descriptor m;
  aiString name;
  ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &name);
  m.albedo = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_SPECULAR, 0, &name);
  m.specular = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_EMISSIVE, 0, &name);
  m.emissive = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_NORMALS, 0, &name);
  m.normal = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_LIGHTMAP, 0, &name);
  m.ambient_occlusion = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_SHININESS, 0, &name);
  m.roughness = copy(&name);
  name.Clear();

  if (albedo_n)
    m.albedo = working_directory + m.albedo;
  if (specular_n)
    m.specular = working_directory + m.specular;
  if (emissive_n)
    m.emissive = working_directory + m.emissive;
  if (normal_n)
    m.normal = working_directory + m.normal;
  if (roughness_n)
    m.roughness = working_directory + m.roughness;

  if (material_override)
  {
    if (material_override->albedo != "")
      m.albedo = material_override->albedo;
    if (material_override->roughness != "")
      m.roughness = material_override->roughness;
    if (material_override->specular != "")
      m.specular = material_override->specular;
    if (material_override->metalness != "")
      m.metalness = material_override->metalness;
    if (material_override->tangent != "")
      m.tangent = material_override->tangent;
    if (material_override->normal != "")
      m.normal = material_override->normal;
    if (material_override->ambient_occlusion != "")
      m.ambient_occlusion = material_override->ambient_occlusion;
    if (material_override->emissive != "")
      m.emissive = material_override->emissive;
    m.vertex_shader = material_override->vertex_shader;
    m.frag_shader = material_override->frag_shader;
    m.backface_culling = material_override->backface_culling;
    m.uv_scale = material_override->uv_scale;
    m.uses_transparency = material_override->uses_transparency;
    m.casts_shadows = material_override->casts_shadows;
  }
  load(m);
}
void Material::load(Material_Descriptor m)
{

  if (WARG_SERVER)
    return;


  this->m = m;
  albedo = Texture(m.albedo, m.uses_transparency);
  // specular_color = Texture(m.specular);
  normal = Texture(m.normal);
  emissive = Texture(m.emissive);
  roughness = Texture(m.roughness);
  shader = Shader(m.vertex_shader, m.frag_shader);
}
void Material::bind()
{
  if (m.backface_culling)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  albedo.bind(Texture_Location::albedo);
  normal.bind(Texture_Location::normal);
  emissive.bind(Texture_Location::emissive);
  roughness.bind(Texture_Location::roughness);
}
void Material::unbind_textures()
{
  glActiveTexture(GL_TEXTURE0 + Texture_Location::albedo);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::specular);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::normal);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::emissive);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::roughness);
  glBindTexture(GL_TEXTURE_2D, 0);
}

bool Light::operator==(const Light &rhs) const
{
  bool b1 = position == rhs.position;
  bool b2 = direction == rhs.direction;
  bool b3 = color == rhs.color;
  bool b4 = attenuation == rhs.attenuation;
  bool b5 = ambient == rhs.ambient;
  bool b6 = cone_angle == rhs.cone_angle;
  bool b7 = type == rhs.type;
  return b1 & b2 & b3 & b4 & b5 & b6 & b7;
}

bool Light_Array::operator==(const Light_Array &rhs)
{
  if (light_count != rhs.light_count)
    return false;
  if (lights != rhs.lights)
  {
    return false;
  }
  if (additional_ambient != rhs.additional_ambient)
  {
    return false;
  }
  return true;
}
Render_Entity::Render_Entity(Mesh *mesh, Material *material,
                             mat4 world_to_model)
    : mesh(mesh), material(material), transformation(world_to_model)
{
  ASSERT(mesh);
  ASSERT(material);
}

Render::Render(SDL_Window *window, ivec2 window_size)
{
  if (WARG_SERVER)
    return;
  set_message("Render constructor");
  this->window = window;
  this->window_size = window_size;
  SDL_DisplayMode current;
  SDL_GetCurrentDisplayMode(0, &current);
  target_frame_time = 1.0f / (float32)current.refresh_rate;
  set_vfov(vfov);
  init_render_targets();
  FRAME_TIMER.start();
}

void Render::set_uniform_lights(Shader &shader)
{
  for (uint32 i = 0; i < lights.light_count; ++i)
  {
    Light_Uniform_Value_Cache* value_cache = &shader.program->light_values_cache[i];
    const Light_Uniform_Location_Cache* location_cache = &shader.program->light_locations_cache[i];
    const Light* new_light = &lights.lights[i];

    if (value_cache->position != new_light->position)
    {
      value_cache->position = new_light->position;
      glUniform3fv(location_cache->position, 1, &new_light->position[0]);
    }

    if (value_cache->direction != new_light->direction)
    {
      value_cache->direction = new_light->direction;
      glUniform3fv(location_cache->direction, 1, &new_light->direction[0]);
    }

    if (value_cache->color != new_light->color)
    {
      value_cache->color = new_light->color;
      glUniform3fv(location_cache->color, 1, &new_light->color[0]);
    }

    if (value_cache->attenuation != new_light->attenuation)
    {
      value_cache->attenuation = new_light->attenuation;
      glUniform3fv(location_cache->attenuation, 1, &new_light->attenuation[0]);
    }

    const vec3 new_ambient = new_light->ambient * new_light->color;
    if (shader.program->light_values_cache[i].ambient != new_ambient)
    {
      value_cache->ambient = new_ambient;
      glUniform3fv(location_cache->ambient, 1, &new_ambient[0]);
    }

    const float new_cone_angle = new_light->cone_angle;
    if (value_cache->cone_angle != new_cone_angle)
    {
      value_cache->cone_angle = new_cone_angle;
      glUniform1fv(location_cache->cone_angle, 1, &new_cone_angle);
    }

    const Light_Type new_type = new_light->type;
    if (value_cache->type != new_type)
    {
      value_cache->type = new_type;
      glUniform1i(location_cache->type, (int32)new_type);
    }
  }

  const uint32 new_light_count = lights.light_count;
  if (shader.program->light_count != new_light_count)
  {
    shader.program->light_count = (int32)lights.light_count;
    glUniform1i(shader.program->light_count_location, new_light_count);
  }

  const vec3 new_ambient = lights.additional_ambient;
  if (shader.program->additional_ambient != new_ambient)
  {
    shader.program->additional_ambient = new_ambient;
    glUniform3fv(shader.program->additional_ambient_location, 1, &new_ambient[0]);
  }
}

void Render::set_uniform_shadowmaps(Shader &shader)
{
  ASSERT(spotlight_shadow_maps.size() == MAX_LIGHTS);
 
  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    const Spotlight_Shadow_Map *shadow_map = &spotlight_shadow_maps[i];

    glActiveTexture(GL_TEXTURE0 + (GLuint)Texture_Location::s0 + i);
    glBindTexture(GL_TEXTURE_2D, spotlight_shadow_maps[i].color.texture);

    const mat4 offset = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5,
                       0.0, 0.5, 0.5, 0.5, 1.0);
    const mat4 shadow_map_transform = offset * shadow_map->projection_camera;

    const Light_Uniform_Location_Cache* location_cache = &shader.program->light_locations_cache[i];
    Light_Uniform_Value_Cache* value_cache = &shader.program->light_values_cache[i]; 

    float32 new_max_variance = lights.lights[i].max_variance;
    if (value_cache->max_variance != new_max_variance)
    {
      value_cache->max_variance = new_max_variance;
      glUniform1f(location_cache->max_variance,new_max_variance);
    }
    if (value_cache->shadow_map_transform != shadow_map_transform)
    {
      value_cache->shadow_map_transform = shadow_map_transform;
      glUniformMatrix4fv(location_cache->shadow_map_transform, 1, GL_FALSE, &shadow_map_transform[0][0]);
    }
    bool casts_shadows = lights.lights[i].casts_shadows;
    if (value_cache->shadow_map_enabled != casts_shadows)
    {
      value_cache->shadow_map_enabled = casts_shadows;
      glUniform1i(location_cache->shadow_map_enabled, casts_shadows);
    }
  }
}

mat4 Render::ortho_projection(ivec2 dst_size)
{
  return ortho(0.0f, (float32)dst_size.x, 0.0f, (float32)dst_size.y, 0.1f, 100.0f) *
    translate(vec3(vec2(0.5f * dst_size.x, 0.5f * dst_size.y), -1)) *
    scale(vec3(dst_size, 1));
}
void Render::build_shadow_maps()
{
  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    spotlight_shadow_maps[i].enabled = false;
  }
  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    if (i > lights.light_count - 1)
      return;

    const Light *light = &lights.lights[i];
    Spotlight_Shadow_Map *shadow_map = &spotlight_shadow_maps[i];
    if (light->type != Light_Type::spot || !light->casts_shadows)
      continue;

    shadow_map->enabled = true;
    if (shadow_map->size != shadow_map_size)
    {
      shadow_map->load(shadow_map_size);
    }
    glViewport(0, 0, shadow_map_size.x, shadow_map_size.y);
   // glEnable(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glCullFace(GL_FRONT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_map->target.fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const mat4 camera =
        glm::lookAt(light->position, light->direction, vec3(0, 0, 1));
    const mat4 projection = glm::perspective(light->shadow_fov, 1.0f, light->shadow_near_plane, light->shadow_far_plane);
    shadow_map->projection_camera = projection * camera;
    for (Render_Entity &entity : render_entities)
    {
      set_message(s("entity name: ",entity.name," casts shadows: ",entity.material->m.casts_shadows));
       if (entity.material->m.casts_shadows)
      {
        ASSERT(entity.mesh);
        int vao = entity.mesh->get_vao();
        glBindVertexArray(vao);
        VARIANCE_SHADOW_MAP.use();
        VARIANCE_SHADOW_MAP.set_uniform(
            "transform", shadow_map->projection_camera * entity.transformation);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                     entity.mesh->get_indices_buffer());
        glDrawElements(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(),
                       GL_UNSIGNED_INT, nullptr);
      }
    }

    for (uint32 i = 0; i < light->shadow_blur_iterations; ++i)
    {
      shadow_map->blur(light->shadow_blur_radius);
    }

  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Render::gaussian_blur(GLuint src, GLuint dst, ivec2 dst_size, GLenum dst_format, float radius)
{
  if(dst_size != CURRENT_GAUSSIAN_SIZE || CURRENT_GAUSSIAN_FORMAT != dst_format)
  {
    glBindTexture(GL_TEXTURE_2D, GAUSSIAN_INTERMEDIATE);

  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl::GL_CLAMP);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl::GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, dst_format, dst_size.x, dst_size.y, 0, dst_format,GL_UNSIGNED_BYTE, 0);

    CURRENT_GAUSSIAN_SIZE = dst_size;
    CURRENT_GAUSSIAN_FORMAT = dst_format;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, SCRATCH_FRAMEBUFFER);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GAUSSIAN_INTERMEDIATE, 0);
  check_FBO_status();

  glViewport(0,0,dst_size.x,dst_size.y);
  glDisable(GL_DEPTH_TEST);

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  vec2 gaus_scale = vec2(radius / dst_size.x ,0.0);
  glBindVertexArray(QUAD.get_vao());
  GAUSSIAN_BLUR.use();
  GAUSSIAN_BLUR.set_uniform("gauss_axis_scale",gaus_scale);
  GAUSSIAN_BLUR.set_uniform("transform",ortho_projection(dst_size));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,src);

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, QUAD.mesh->position_buffer);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, QUAD.mesh->uv_buffer);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float32) * 2, 0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(),GL_UNSIGNED_INT, (void *)0);

  gaus_scale.y = gaus_scale.x;
  gaus_scale.x = 0;
  GAUSSIAN_BLUR.set_uniform("gauss_axis_scale",gaus_scale);

  glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,dst,0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,GAUSSIAN_INTERMEDIATE);
  check_FBO_status();

  glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(),GL_UNSIGNED_INT, (void *)0);

  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  glBindTexture(GL_TEXTURE_2D,0);
  glEnable(GL_DEPTH_TEST);
}

//if dst_texture is 0, use main framebuffer
//shader must use passthrough.vert, and must use texture1, texture2 ... texturen for input texture sampler names
void Render::run_pixel_shader(Shader* shader, vector<GLuint> src_textures, GLuint dst_texture, ivec2 dst_size, bool clear_dst)
{
    for(uint32 i = 0; i < src_textures.size();++i)
    {
      glActiveTexture(GL_TEXTURE0+i);
      glBindTexture(GL_TEXTURE_2D,src_textures[i]);
    }
    
    ivec2 viewport_size;
    
    if(dst_texture == 0)
    {//render to screen
      glBindFramebuffer(GL_FRAMEBUFFER,0);
      viewport_size = window_size;
    }
    else
    {//render to dst texture
      glBindFramebuffer(GL_FRAMEBUFFER,SCRATCH_FRAMEBUFFER);
      glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,dst_texture,0);
      viewport_size = dst_size;
    }

    glViewport(0,0,viewport_size.x,viewport_size.y);
    
    if(clear_dst)
    {
      glClearColor(0,0,0,0);
      glClear(GL_COLOR_BUFFER_BIT);
    }
    
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(QUAD.get_vao());
    shader->use();


   // glEnableVertexAttribArray(0); 
   // glEnableVertexAttribArray(1);
    

    shader->set_uniform("transform", ortho_projection(viewport_size));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(),
                   GL_UNSIGNED_INT, (void *)0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Render::opaque_pass(float32 time)
{
  glViewport(0, 0, size.x, size.y);
  glBindFramebuffer(GL_FRAMEBUFFER, TARGET_FRAMEBUFFER);
  glFramebufferTexture(GL_FRAMEBUFFER, DIFFUSE_TARGET, COLOR_TARGET_TEXTURE, 0);

  glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  //todo: depth pre-pass
  for (Render_Entity &entity : render_entities)
  {
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    //set_message(s("binding vao:", vao));
    glBindVertexArray(vao);
    Shader &shader = entity.material->shader;
    shader.use();
    entity.material->bind();
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->m.uv_scale);
    shader.set_uniform("MVP", projection * camera * entity.transformation);
    shader.set_uniform("Model", entity.transformation);
    shader.set_uniform("discard_over_blend", true);
    set_uniform_lights(shader);
    set_uniform_shadowmaps(shader);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    glDrawElements(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(),
                   GL_UNSIGNED_INT, nullptr);
    entity.material->unbind_textures();
  }
}

void Render::instance_pass(float32 time)
{
  for (auto &entity : render_instances)
  {
    ASSERT(0); //disables vertex attribs, so non instanced draw calls will need to re-enable and re-point
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    glBindVertexArray(vao);
    Shader &shader = entity.material->shader;
    ASSERT(shader.vs == "instance.vert");
    shader.use();
    entity.material->bind();
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->m.uv_scale);
    set_uniform_lights(shader);
    //// verify sizes of data, mat4 floats
    ASSERT(entity.Model_Matrices.size() > 0);
    ASSERT(entity.MVP_Matrices.size() == entity.Model_Matrices.size());
    ASSERT(sizeof(decltype(entity.Model_Matrices[0])) ==
           sizeof(decltype(entity.MVP_Matrices[0])));
    ASSERT(sizeof(decltype(entity.MVP_Matrices[0][0][0])) ==
           sizeof(GLfloat)); // buffer init code assumes these
    ASSERT(sizeof(decltype(entity.MVP_Matrices[0])) == sizeof(mat4));

    uint32 num_instances = entity.MVP_Matrices.size();
    uint32 buffer_size = num_instances * sizeof(mat4);

    glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MVP_BUFFER);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size,
                    &entity.MVP_Matrices[0][0][0]);
    glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MODEL_BUFFER);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size,
                    &entity.Model_Matrices[0][0][0]);

    const GLuint mat4_size = sizeof(GLfloat) * 4 * 4;
    // shader attribute locations
    // 4 locations for mat4
    int32 loc = glGetAttribLocation(shader.program->program, "instanced_MVP");
    ASSERT(loc != -1);

    GLuint loc1 = loc + 0;
    GLuint loc2 = loc + 1;
    GLuint loc3 = loc + 2;
    GLuint loc4 = loc + 3;
    glEnableVertexAttribArray(loc1);
    glEnableVertexAttribArray(loc2);
    glEnableVertexAttribArray(loc3);
    glEnableVertexAttribArray(loc4);
    glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MVP_BUFFER);
    glVertexAttribPointer(loc1, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(0));
    glVertexAttribPointer(loc2, 4, GL_FLOAT, GL_FALSE, mat4_size,
                          (void *)(sizeof(GLfloat) * 4));
    glVertexAttribPointer(loc3, 4, GL_FLOAT, GL_FALSE, mat4_size,
                          (void *)(sizeof(GLfloat) * 8));
    glVertexAttribPointer(loc4, 4, GL_FLOAT, GL_FALSE, mat4_size,
                          (void *)(sizeof(GLfloat) * 12));
    glVertexAttribDivisor(loc1, 1);
    glVertexAttribDivisor(loc2, 1);
    glVertexAttribDivisor(loc3, 1);
    glVertexAttribDivisor(loc4, 1);

    loc = glGetAttribLocation(shader.program->program, "instanced_model");
    ASSERT(loc != -1);

    GLuint loc_1 = loc + 0;
    GLuint loc_2 = loc + 1;
    GLuint loc_3 = loc + 2;
    GLuint loc_4 = loc + 3;
    glEnableVertexAttribArray(loc_1);
    glEnableVertexAttribArray(loc_2);
    glEnableVertexAttribArray(loc_3);
    glEnableVertexAttribArray(loc_4);
    glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MODEL_BUFFER);
    glVertexAttribPointer(loc_1, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(0));
    glVertexAttribPointer(loc_2, 4, GL_FLOAT, GL_FALSE, mat4_size,
                          (void *)(sizeof(GLfloat) * 4));
    glVertexAttribPointer(loc_3, 4, GL_FLOAT, GL_FALSE, mat4_size,
                          (void *)(sizeof(GLfloat) * 8));
    glVertexAttribPointer(loc_4, 4, GL_FLOAT, GL_FALSE, mat4_size,
                          (void *)(sizeof(GLfloat) * 12));
    glVertexAttribDivisor(loc_1, 1);
    glVertexAttribDivisor(loc_2, 1);
    glVertexAttribDivisor(loc_3, 1);
    glVertexAttribDivisor(loc_4, 1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    glDrawElementsInstanced(GL_TRIANGLES,
                            entity.mesh->get_indices_buffer_size(),
                            GL_UNSIGNED_INT, (void *)0, num_instances);

    entity.material->unbind_textures();
    glDisableVertexAttribArray(loc1);
    glDisableVertexAttribArray(loc2);
    glDisableVertexAttribArray(loc3);
    glDisableVertexAttribArray(loc4);
    glDisableVertexAttribArray(loc_1);
    glDisableVertexAttribArray(loc_2);
    glDisableVertexAttribArray(loc_3);
    glDisableVertexAttribArray(loc_4);
  }
}

void Render::translucent_pass(float32 time)
{
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);

  for (Render_Entity &entity : translucent_entities)
  {
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    glBindVertexArray(vao);
    Shader &shader = entity.material->shader;
    shader.use();
    entity.material->bind();
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->m.uv_scale);
    shader.set_uniform("MVP", projection * camera * entity.transformation);
    shader.set_uniform("Model", entity.transformation);
    shader.set_uniform("discard_over_blend", false);
    set_uniform_lights(shader);
    set_uniform_shadowmaps(shader);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    glDrawElements(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(),
                   GL_UNSIGNED_INT, nullptr);
  }
}
void Render::render(float64 state_time)
{
#if DYNAMIC_FRAMERATE_TARGET
  dynamic_framerate_target();
#endif
#if DYNAMIC_TEXTURE_RELOADING
  check_and_clear_expired_textures();
#endif

  float32 time = (float32)get_real_time();
  float64 t = (time - state_time) / dt;

  set_message("FRAME_START", "");
  set_message("BUILDING SHADOW MAPS START", "");
  build_shadow_maps();
  set_message("OPAQUE PASS START", "");
  opaque_pass(time);
  // instance_pass(time);
  // translucent_pass(time);

  draw_calls_last_frame = render_entities.size();
  set_message("COPYING TO SCREEN", "");

  if (use_txaa)
  {
    // TODO: implement motion vector vertex attribute

    // render to main framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_size.x, window_size.y);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    TEMPORALAA.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, COLOR_TARGET_TEXTURE);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,
                  PREV_COLOR_TARGET_MISSING ? COLOR_TARGET_TEXTURE
                                            : PREV_COLOR_TARGET);
    TEMPORALAA.set_uniform("transform", ortho_projection(window_size));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(),
                   GL_UNSIGNED_INT, (void *)0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    // glFinish(); // intent is to time just the swap itself
    FRAME_TIMER.stop();
    SWAP_TIMER.start();
    SDL_GL_SwapWindow(window);
    // glFinish();
    SWAP_TIMER.stop();
    FRAME_TIMER.start();

    // copy this frame to previous framebuffer
    // so it will be usable next frame, when its the old frame

    // assign previous_color as the target to overwrite
    glViewport(0, 0, size.x, size.y);
    //todo: make a framebuffer for each target, rather than swapping textures
    glBindFramebuffer(GL_FRAMEBUFFER, TARGET_FRAMEBUFFER);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         PREV_COLOR_TARGET, 0);
    PASSTHROUGH.use();
    // sample the current color_target as source
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, COLOR_TARGET_TEXTURE);
    PASSTHROUGH.set_uniform("transform", ortho_projection(window_size));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(),
                   GL_UNSIGNED_INT, (void *)0);

    PREV_COLOR_TARGET_MISSING = false;
    // place color_target back in target_fbo
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         COLOR_TARGET_TEXTURE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    txaa_jitter = get_next_TXAA_sample();
  }
  else
  {
    //sponge test blur
    static bool first = true;
    static GLuint test_target1 = 0;
    static GLuint test_target2 = 0;
    if (first)
    {
      glGenTextures(1, &test_target1);
      glGenTextures(1, &test_target2);

      first = false;

      glBindTexture(GL_TEXTURE_2D, test_target1);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl::GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl::GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_size.x, window_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

      glBindTexture(GL_TEXTURE_2D, test_target2);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl::GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl::GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_size.x, window_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }

    float radius = 0;
    gaussian_blur(COLOR_TARGET_TEXTURE, test_target2, window_size, GL_RGBA, radius);

    gaussian_blur(test_target2, test_target1, window_size, GL_RGBA, radius);

    gaussian_blur(test_target1, test_target2, window_size, GL_RGBA, radius);

    gaussian_blur(test_target2, test_target1, window_size, GL_RGBA, radius);

    gaussian_blur(test_target1, test_target2, window_size, GL_RGBA, radius);

    gaussian_blur(test_target2, test_target1, window_size, GL_RGBA, radius);


  
    // render to main framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_size.x, window_size.y);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(QUAD.get_vao());
    PASSTHROUGH.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, test_target1);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, QUAD.mesh->position_buffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, QUAD.mesh->uv_buffer);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float32) * 2, 0);
    PASSTHROUGH.set_uniform("transform", ortho_projection(window_size));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

    glBindTexture(GL_TEXTURE_2D, 0);
    FRAME_TIMER.stop();
    SWAP_TIMER.start();
    SDL_GL_SwapWindow(window);
    set_message("FRAME END", "");
    SWAP_TIMER.stop();
    FRAME_TIMER.start();
    glBindVertexArray(0);

    #if 0
    ///sponge



    // render to main framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_size.x, window_size.y);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(QUAD.get_vao());
    PASSTHROUGH.use();
    //glEnableVertexAttribArray(0);
    //glBindBuffer(GL_ARRAY_BUFFER, QUAD.mesh->position_buffer);
    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);

    //glEnableVertexAttribArray(1);
    //glBindBuffer(GL_ARRAY_BUFFER, QUAD.mesh->uv_buffer);
    //glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float32) * 2, 0);

   // GLuint loc = glGetUniformLocation(PASSTHROUGH.program->program, "albedo");
   // ASSERT(loc != -1);
   // glUniform1i(loc, Texture_Location::albedo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, COLOR_TARGET_TEXTURE);

    PASSTHROUGH.set_uniform("transform", ortho_projection(window_size));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(),
                   GL_UNSIGNED_INT, (void *)0);

    glBindTexture(GL_TEXTURE_2D, 0);
    // glFinish(); // intent is to time just the swap itself
    FRAME_TIMER.stop();
    SWAP_TIMER.start();
    SDL_GL_SwapWindow(window);
    set_message("FRAME END", "");
    // glFinish();
    SWAP_TIMER.stop();
    FRAME_TIMER.start();
    glBindVertexArray(0);
    // set_message("Swap complete... Saving default framebuffer");
    // save_and_log_screen();
    #endif
  }
  frame_count += 1;
}

void Render::resize_window(ivec2 window_size)
{
  this->window_size = window_size;
  set_vfov(vfov);
  ASSERT(0); // not yet implemented
}

void Render::set_render_scale(float32 scale)
{
  if (scale < 0.1f)
    scale = 0.1f;
  if (scale > 2.0f)
    scale = 2.0f;
  if (render_scale == scale)
    return;
  render_scale = scale;
  init_render_targets();
  time_of_last_scale_change = get_real_time();
}

void Render::set_camera(vec3 pos, vec3 camera_gaze_dir)
{
  vec3 p = pos + camera_gaze_dir;
  camera_position = pos;
  camera = glm::lookAt(pos, p, {0, 0, 1});
}
void Render::set_camera_gaze(vec3 pos, vec3 p)
{
  camera_position = pos;
  camera = glm::lookAt(pos, p, {0, 0, 1});
}

void Render::set_vfov(float32 vfov)
{
  const float32 aspect = (float32)window_size.x / (float32)window_size.y;
  const float32 znear = 0.51f;
  const float32 zfar = 1000;
  projection = glm::perspective(radians(vfov), aspect, znear, zfar);
}

void Render::set_render_entities(vector<Render_Entity> *new_entities)
{
  previous_render_entities = move(render_entities);
  render_entities.clear();

  // calculate the distance from the camera for each entity at index in
  // new_entities  sort them  split opaque and translucent entities
  std::vector<std::pair<uint32, float32>>
      index_distances; // for insert sorted, -1 means index-omit

  for (uint32 i = 0; i < new_entities->size(); ++i)
  {
    Render_Entity *entity = &(*new_entities)[i];
    mat4 *m = &entity->transformation;
    vec3 translation = vec3((*m)[3][0], (*m)[3][1], (*m)[3][2]);

    float32 dist = length(translation - camera_position);
    bool is_transparent = entity->material->m.uses_transparency;
    if (is_transparent)
    {
      ASSERT(entity->material->albedo.storage_type == GL_RGBA);
      index_distances.push_back({i, dist});
    }
    else
    {
      index_distances.push_back({i, -1.0f});
    }
  }

  std::sort(index_distances.begin(), index_distances.end(),
            [](std::pair<uint32, float32> p1, std::pair<uint32, float32> p2) {
              return p1.second > p2.second;
            });

  for (auto i : index_distances)
  {
    if (i.second != -1.0f)
    {
      translucent_entities.push_back((*new_entities)[i.first]);
      ASSERT(0); // test this, the furthest objects should be first
    }
    else
    {
      render_entities.push_back((*new_entities)[i.first]);
    }
  }
}

void Render::set_lights(Light_Array new_lights)
{
  lights = new_lights; /*

   for (uint32 i = 0; i < MAX_LIGHTS; ++i)
   {
     spotlight_shadow_maps[i].enabled = false;
     Light* l = &lights.lights[i];
     if (l->type == spot && l->casts_shadows)
     {
       spotlight_shadow_maps[i].enabled = true;
     }
   }*/
}

void check_FBO_status()
{
  auto result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  std::string str;
  switch (result)
  {
    case GL_FRAMEBUFFER_UNDEFINED:
      str = "GL_FRAMEBUFFER_UNDEFINED";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      str = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      str = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
      str = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
      str = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      str = "GL_FRAMEBUFFER_UNSUPPORTED";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
      str = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
      str = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
      break;
    case GL_FRAMEBUFFER_COMPLETE:
      str = "GL_FRAMEBUFFER_COMPLETE";
      break;
  }

  if (str != "GL_FRAMEBUFFER_COMPLETE")
  {
    set_message("FBO_ERROR", str);
    ASSERT(0);
  }
}

void Render::init_render_targets()
{
  set_message("init_render_targets()");
  set_message("Deleting screen FBO", std::to_string(TARGET_FRAMEBUFFER));
  set_message("Deleting gaussian FBO", std::to_string(SCRATCH_FRAMEBUFFER));
  set_message("Deleting Textures",
              std::to_string(COLOR_TARGET_TEXTURE) + " " +
                  std::to_string(PREV_COLOR_TARGET) + " " +
                  std::to_string(DEPTH_TARGET_TEXTURE));

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &TARGET_FRAMEBUFFER);
  glDeleteFramebuffers(1, &SCRATCH_FRAMEBUFFER);
  glDeleteTextures(1, &COLOR_TARGET_TEXTURE);
  glDeleteTextures(1, &PREV_COLOR_TARGET);
  glDeleteRenderbuffers(1, &DEPTH_TARGET_TEXTURE);
  glDeleteTextures(1,&GAUSSIAN_INTERMEDIATE);

  glGenTextures(1,&GAUSSIAN_INTERMEDIATE);

  size = ivec2(render_scale * window_size.x, render_scale * window_size.y);
  glGenFramebuffers(1, &TARGET_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, TARGET_FRAMEBUFFER);

  glGenTextures(1, &COLOR_TARGET_TEXTURE);
  glBindTexture(GL_TEXTURE_2D, COLOR_TARGET_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  glGenTextures(1, &PREV_COLOR_TARGET);
  glBindTexture(GL_TEXTURE_2D, PREV_COLOR_TARGET);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  glFramebufferTexture(GL_FRAMEBUFFER, DIFFUSE_TARGET, COLOR_TARGET_TEXTURE, 0);
  glDrawBuffers(TARGET_COUNT, RENDER_TARGETS);
  glGenRenderbuffers(1, &DEPTH_TARGET_TEXTURE);
  glBindRenderbuffer(GL_RENDERBUFFER, DEPTH_TARGET_TEXTURE);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size.x, size.y);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, DEPTH_TARGET_TEXTURE);

  PREV_COLOR_TARGET_MISSING = true;

  glGenFramebuffers(1,&SCRATCH_FRAMEBUFFER);

  set_message("Init screen FBO", s(TARGET_FRAMEBUFFER));
  set_message("Init gaussian FBO", s(SCRATCH_FRAMEBUFFER));
  set_message("Init Textures",
              s(COLOR_TARGET_TEXTURE) + " " + s(PREV_COLOR_TARGET));
  set_message("Init renderbuffers", s(DEPTH_TARGET_TEXTURE));

  check_FBO_status();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Render::dynamic_framerate_target()
{
  // early out if we don't have enough samples to go by
  const uint32 min_samples = 5;
  // percent of frame+swap samples must be > target_time to downscale
  const float32 reduce_threshold = 0.35f;
  // render scale multiplication factors when changing resolution
  const float32 reduction_factor = 0.85f;
  const float32 increase_factor = 1.08f;
  // gives a little extra time before swap+frame > 1/hz counts as a missed vsync
  // not entirely sure why this is necessary, but at 0 swap+frame is almost
  // always > 1/hz
  // driver overhead and/or timer overhead inaccuracies
  const float64 epsilon = 0.005;
  // if swap alone took longer than this, we should increase the resolution
  // because we probably had a lot of extra gpu time because this blocked for so
  // long
  // not sure if there's a better way to find idle gpu use
  const float64 longest_swap_allowable = target_frame_time * .35;

  if (FRAME_TIMER.sample_count() < min_samples ||
      SWAP_TIMER.sample_count() < min_samples)
  { // don't adjust until we have good avg frame time data
    return;
  }
  // ALL time other than swap time
  const float64 moving_avg = FRAME_TIMER.moving_average();
  const float64 last_frame = FRAME_TIMER.get_last();

  // ONLY swap time
  const float64 last_swap = SWAP_TIMER.get_last();
  const float64 swap_avg = SWAP_TIMER.moving_average();

  auto frame_times = FRAME_TIMER.get_times();
  auto swap_times = SWAP_TIMER.get_times();
  ASSERT(frame_times.size() == swap_times.size());
  uint32 num_high_frame_times = 0;
  for (uint32 i = 0; i < frame_times.size(); ++i)
  {
    const float64 swap = swap_times[i];
    const float64 frame = frame_times[i];
    const bool high = swap + frame > target_frame_time + epsilon;
    num_high_frame_times += int32(high);
  }
  float32 percent_high = float32(num_high_frame_times) / frame_times.size();

  if (percent_high > reduce_threshold)
  {
    set_message(
        "Number of missed frames over threshold. Reducing resolution.: ",
        std::to_string(percent_high) + " " + std::to_string(target_frame_time));
    set_render_scale(render_scale * reduction_factor);

    // we need to clear the timers because the render scale change
    // means the old times arent really valid anymore - different res
    // frame_timer is currently running so we
    // must get the time it logged for the current
    // frame start, and put it back when we restart
    uint64 last_start = FRAME_TIMER.get_begin();
    SWAP_TIMER.clear_all();
    FRAME_TIMER.clear_all();
    FRAME_TIMER.start(last_start);
  }
  else if (swap_avg > longest_swap_allowable)
  {
    if (render_scale != 2.0) // max scale 2.0
    {
      float64 change_rate = .45;
      if (get_real_time() < time_of_last_scale_change + change_rate)
      { // increase resolution slowly
        return;
      }
      set_message("High avg swap. Increasing resolution.",
                  std::to_string(swap_avg) + " " +
                      std::to_string(target_frame_time));
      set_render_scale(render_scale * increase_factor);
      uint64 last_start = FRAME_TIMER.get_begin();
      SWAP_TIMER.clear_all();
      FRAME_TIMER.clear_all();
      FRAME_TIMER.start(last_start);
    }
  }
}

mat4 Render::get_next_TXAA_sample()
{
  vec2 translation;
  if (jitter_switch)
    translation = vec2(0.5, 0.5);
  else
    translation = vec2(-0.5, -0.5);

  mat4 result = glm::translate(vec3(translation / vec2(size), 0));
  jitter_switch = !jitter_switch;
  return result;
}

Mesh_Handle::~Mesh_Handle()
{
  if(WARG_SERVER)
  return;
  
  set_message("Deleting mesh: ", s(vao, " ", position_buffer));
  glDeleteBuffers(1, &position_buffer);
  glDeleteBuffers(1, &normal_buffer);
  glDeleteBuffers(1, &uv_buffer);
  glDeleteBuffers(1, &tangents_buffer);
  glDeleteBuffers(1, &bitangents_buffer);
  glDeleteBuffers(1, &indices_buffer);
  glDeleteVertexArrays(1, &vao);
  vao = 0;
  position_buffer = 0;
  normal_buffer = 0;
  uv_buffer = 0;
  tangents_buffer = 0;
  bitangents_buffer = 0;
  indices_buffer = 0;
  indices_buffer_size = 0;
}

FBO_Handle::~FBO_Handle() { glDeleteFramebuffers(1, &fbo); }

void Spotlight_Shadow_Map::blur(float radius)
{
  glBindFramebuffer(GL_FRAMEBUFFER, blurfbo.fbo);
  glViewport(0, 0, size.x, size.y);
  glDisable(GL_DEPTH_TEST);

  vec2 gaus_scale = vec2(radius / size.x, 0.0);
  glBindVertexArray(QUAD.get_vao());
  GAUSSIAN_BLUR.use();
  GAUSSIAN_BLUR.set_uniform("gauss_axis_scale", gaus_scale);
  GAUSSIAN_BLUR.set_uniform("transform", Render::ortho_projection(size));

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, color.texture);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  gaus_scale.y = gaus_scale.x;
  gaus_scale.x = 0;
  GAUSSIAN_BLUR.set_uniform("gauss_axis_scale", gaus_scale);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, blurtex.texture);
  glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glEnable(GL_DEPTH_TEST);
}

void Spotlight_Shadow_Map::load(ivec2 size)
{
  //target fbo
  glGenFramebuffers(1, &target.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);

  // gen target depth buffer
  glGenTextures(1, &color.texture);
  glBindTexture(GL_TEXTURE_2D, color.texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
    GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  //glTexParameterf(GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
  //glGenerateMipmap(GL_TEXTURE_2D);

  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color.texture, 0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);

  // gen actual depth buffer
  glGenRenderbuffers(1, &depth.rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, depth.rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size.x, size.y);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth.rbo);

  //gauss
  glGenTextures(1, &blurtex.texture);
  glBindTexture(GL_TEXTURE_2D, blurtex.texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
    GL_LINEAR);
  //glTexParameterf(GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);


  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, size.x, size.y, 0, GL_RGBA,
    GL_UNSIGNED_BYTE, 0);

  //glGenerateMipmap(GL_TEXTURE_2D);

  glGenFramebuffers(1, &blurfbo.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, blurfbo.fbo);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, blurtex.texture,0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);


  check_FBO_status();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  this->size = size;
}

RenderBuffer_Handle::~RenderBuffer_Handle() { glDeleteRenderbuffers(1, &rbo); }
