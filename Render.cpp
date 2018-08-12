#include "stdafx.h"
#include "Globals.h"
#define STB_IMAGE_IMPLEMENTATION
#include "Third_party/stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "Mesh_Loader.h"
#include "Render.h"
#include "SDL_Imgui_State.h"
#include "Shader.h"
#include "Timer.h"
#include "UI.h"
using glm::ivec2;
using glm::lookAt;
using glm::mat4;
using glm::max;
using glm::perspective;
using glm::translate;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using std::array;
using std::cout;
using std::endl;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::sort;
using std::string;
using std::unordered_map;
using std::vector;
using std::weak_ptr;
using namespace gl33core;

#define DIFFUSE_TARGET GL_COLOR_ATTACHMENT0
#define DEPTH_TARGET GL_COLOR_ATTACHMENT1
#define NORMAL_TARGET GL_COLOR_ATTACHMENT2
#define POSITION_TARGET GL_COLOR_ATTACHMENT3

const vec4 Conductor_Reflectivity::iron = vec4(0.56, 0.57, 0.58, 1.0f);
const vec4 Conductor_Reflectivity::copper = vec4(0.95, 0.64, 0.54, 1.0f);
const vec4 Conductor_Reflectivity::gold = vec4(1.0, 0.71, 0.29, 1.0f);
const vec4 Conductor_Reflectivity::aluminum = vec4(0.91, 0.92, 0.92, 1.0f);
const vec4 Conductor_Reflectivity::silver = vec4(0.95, 0.93, 0.88, 1.0f);

static unordered_map<string, weak_ptr<Mesh_Handle>> MESH_CACHE;
static unordered_map<string, weak_ptr<Texture_Handle>> TEXTURE2D_CACHE;
static unordered_map<string, weak_ptr<Texture_Handle>> TEXTURECUBEMAP_CACHE;

void check_FBO_status();
uint32 mip_levels_for_resolution(ivec2 resolution)
{
  uint count = 0;
  while (resolution.x > 2)
  {
    count += 1;
    resolution = resolution / 2;
  }
  return count;
}
Framebuffer_Handle::~Framebuffer_Handle()
{
  glDeleteFramebuffers(1, &fbo);
}
Framebuffer::Framebuffer() {}
void Framebuffer::init()
{
  // opengl calls no longer allowed in state.update()
  // render your imgui ui that requires textures in state.render()
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  if (!fbo)
  {
    fbo = make_shared<Framebuffer_Handle>();
    glGenFramebuffers(1, &fbo->fbo);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);

  std::vector<GLenum> draw_buffers;
  for (uint32 i = 0; i < color_attachments.size(); ++i)
  {
    Texture_Descriptor test;
    ASSERT(color_attachments[i].t.name != test.name); // texture must be named, or .load assumes null
    color_attachments[i].load();
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, color_attachments[i].get_handle(), 0);
    draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
  }
  glDrawBuffers(draw_buffers.size(), &draw_buffers[0]);
  // check_FBO_status();
  if (depth_enabled)
  {
    depth = make_shared<Renderbuffer_Handle>();
    glGenRenderbuffers(1, &depth->rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depth->rbo);

    glRenderbufferStorage(GL_RENDERBUFFER, depth_format, depth_size.x, depth_size.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth->rbo);
    depth->format = depth_format;
    depth->size = depth_size;
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
  }
  // check_FBO_status();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bind()
{
  ASSERT(fbo);
  ASSERT(fbo->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
  // check_FBO_status();
  if (encode_srgb_on_draw)
  {
    glEnable(GL_FRAMEBUFFER_SRGB);
  }
  else
  {
    glDisable(GL_FRAMEBUFFER_SRGB);
  }
}

Gaussian_Blur::Gaussian_Blur() {}
void Gaussian_Blur::init(Texture_Descriptor &td)
{
  if (!initialized)
    gaussian_blur_shader = Shader("passthrough.vert", "gaussian_blur_7x.frag");

  intermediate_fbo.color_attachments = {Texture(td)};
  intermediate_fbo.init();

  target.color_attachments = {Texture(td)};
  target.init();

  aspect_ratio_factor = (float32)td.size.y / (float32)td.size.x;

  initialized = true;
}
void Gaussian_Blur::draw(Renderer *renderer, Texture *src, float32 radius, uint32 iterations)
{
  ASSERT(initialized);
  ASSERT(renderer);
  ASSERT(intermediate_fbo.fbo);
  ASSERT(intermediate_fbo.color_attachments.size() == 1);
  ASSERT(src);
  ASSERT(src->get_handle());
  ASSERT(target.fbo);
  ASSERT(target.color_attachments.size() > 0);

  ivec2 *dst_size = &intermediate_fbo.color_attachments[0].t.size;

  if (iterations == 0)
  {
    vector<Texture *> texture = {src};
    run_pixel_shader(&renderer->passthrough, &texture, &target, false);
    return;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, intermediate_fbo.fbo->fbo);
  glViewport(0, 0, dst_size->x, dst_size->y);
  glDisable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  vec2 gaus_scale = vec2(aspect_ratio_factor * radius / dst_size->x, 0.0);
  glBindVertexArray(renderer->quad.get_vao());
  gaussian_blur_shader.use();
  gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
  gaussian_blur_shader.set_uniform("lod", uint32(1));
  gaussian_blur_shader.set_uniform("transform", Renderer::ortho_projection(*dst_size));
  src->bind(0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->quad.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  gaus_scale.y = radius / dst_size->x;
  gaus_scale.x = 0;
  gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo->fbo);

  intermediate_fbo.color_attachments[0].bind(0);

  glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  iterations -= 1;

  for (uint i = 0; i < iterations; ++i)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, intermediate_fbo.fbo->fbo);
    vec2 gaus_scale = vec2(aspect_ratio_factor * radius / dst_size->x, 0.0);
    gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
    target.color_attachments[0].bind(0);
    glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);
    gaus_scale.y = radius / dst_size->x;
    gaus_scale.x = 0;
    gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo->fbo);
    intermediate_fbo.color_attachments[0].bind(0);
    glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glEnable(GL_DEPTH_TEST);
}

Renderer::~Renderer() {}

void check_and_clear_expired_textures()
{ // todo: separate thread that does this
  auto it = TEXTURE2D_CACHE.begin();
  while (it != TEXTURE2D_CACHE.end())
  {
    auto texture_handle = *it;
    const string *path = &texture_handle.first;
    struct stat attr;
    stat(path->c_str(), &attr);
    time_t t = attr.st_mtime;

    auto ptr = texture_handle.second.lock();
    if (ptr) // texture could exist but not yet be loaded
    {
      time_t t1 = ptr.get()->file_mod_t;
      if (t1 != t)
      {
        it = TEXTURE2D_CACHE.erase(it);
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

  SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
  if (surface)
  {
    string name = s("screen_", i, ".png");
    set_message("Saving Screenshot: ", " With name: " + name);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
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

  SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
  if (surface)
  {
    string name = s("Texture object ", texture, " #", i, ".png");
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

  string result = "\n";
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

Texture::Texture(Texture_Descriptor &td)
{
  t = td;
  load();
}

Texture::Texture(string name, ivec2 size, GLenum format, GLenum minification_filter, GLenum magnification_filter,
    GLenum wrap_s, GLenum wrap_t, vec4 border_color)
{
  this->t.name = name;
  this->t.size = size;
  this->t.format = format;
  this->t.minification_filter = minification_filter;
  this->t.magnification_filter = magnification_filter;
  this->t.wrap_t = wrap_t;
  this->t.wrap_s = wrap_s;
  this->t.border_color = border_color;
  this->t.cache_as_unique = true;
  load();
}
Texture_Handle::~Texture_Handle()
{
  // opengl calls no longer allowed in state.update()
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  // set_message("Deleting texture: ", s(texture, " ", filename), 45.f);
  glDeleteTextures(1, &texture);
  texture = 0;
}

Texture::Texture(string path, bool premul)
{
  t.name = path; // note this will possibly be modified within .load()
  t.cache_as_unique = false;
  t.process_premultiply = premul;
  t.format = GL_RGBA;
  const bool is_hdr = stbi_is_hdr(path.c_str());
  if (is_hdr)
  {
    t.format = GL_RGBA16F;
  }
  load();
}

void Texture::load()
{
  // opengl calls no longer allowed in state.update()
  // render your imgui ui that requires textures in state.render()
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  if (initialized && !texture)
  { // file doesnt exist
#if !DYNAMIC_TEXTURE_RELOADING
    return;
#endif
  }

  if (t.name == "")
    return;

  const bool is_a_color = t.name.substr(0, 6) == "color(";

  if (is_a_color)
  {
    t.cache_as_unique = false;
  }

  if (!initialized && !is_a_color)
    t.cache_as_unique = !has_img_file_extension(t.name);

  // for generated textures
  if (t.cache_as_unique)
  {
    bool requires_reallocation = !((texture) && (texture->size == t.size) && (texture->format == t.format));
    if (!requires_reallocation)
      return;

    if (!texture)
    {
      static uint32 i = 0;
      ++i;
      string unique_key = s("Generated texture ID:", i, " Name:", t.name);
      TEXTURE2D_CACHE[unique_key] = texture = make_shared<Texture_Handle>();
      glGenTextures(1, &texture->texture);
      texture->filename = t.name;
    }

    glBindTexture(GL_TEXTURE_2D, texture->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, t.format, t.size.x, t.size.y, 0, GL_RGBA, GL_FLOAT, 0);
    texture->size = t.size;
    texture->format = t.format;
    initialized = true;
    ASSERT(texture);
    check_set_parameters();
    return;
  }

  if (!initialized && !is_a_color)
  {
    t.name = fix_filename(t.name);

    if (t.name.find_last_of("/") == t.name.npos)
    { // no directory included in name, so use base path
      t.name = BASE_TEXTURE_PATH + t.name;
    }
    // else: use t.name as the full directory
  }

  if (t.name == BASE_TEXTURE_PATH)
  {
    set_message(s("Warning: Invalid texture path:", t.name));
    texture = nullptr;
    return;
  }

  auto ptr = TEXTURE2D_CACHE[t.name].lock();
  if (ptr)
  {
    texture = ptr;

    t.size = texture->size;
    t.format = texture->format;
    initialized = true;
    return;
  }

  // todo: other software procedural texture generators here?
  if (t.name.substr(0, 6) == "color(")
  {
    vec4 color = string_to_float4_color(t.name);
    if (color != vec4(0))
    {
      texture = make_shared<Texture_Handle>();
      TEXTURE2D_CACHE[t.name] = texture;
      t.size = ivec2(1, 1);
      t.format = GL_RGBA16F; // could detect and support other formats in here
      t.magnification_filter = GL_NEAREST;
      t.minification_filter = GL_NEAREST;
      glGenTextures(1, &texture->texture);
      glBindTexture(GL_TEXTURE_2D, texture->texture);
      glTexImage2D(GL_TEXTURE_2D, 0, t.format, t.size.x, t.size.y, 0, GL_RGBA, GL_FLOAT, &color);
      glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &t.border_color[0]);
      glBindTexture(GL_TEXTURE_2D, 0);
      texture->size = t.size;
      texture->filename = t.name;
      texture->format = t.format;
      texture->border_color = t.border_color;
      initialized = true;
      return;
    }
    return;
  }

  void *data = nullptr;
  GLenum data_format = GL_UNSIGNED_BYTE;
  Image_Data imgdata;
  int32 width, height, n;
  if (IMAGE_LOADER.load(t.name, &imgdata))
  {
    texture = make_shared<Texture_Handle>();
    TEXTURE2D_CACHE[t.name] = texture;
    data = imgdata.data;
    width = imgdata.x;
    height = imgdata.y;
    n = imgdata.comp;
    data_format = imgdata.format;
  }

  if (!data)
  { // error loading file...
#if DYNAMIC_TEXTURE_RELOADING
    // retry next frame
    set_message("Warning: missing texture:" + t.name);
    texture = nullptr;
    return;
#else
    if (!data)
    {
      set_message("STBI failed to find or load texture: ", t.name, 3.0);
      texture = nullptr;
      initialized = imgdata.initialized;
      return;
    }
#endif
  }

  struct stat attr;
  stat(t.name.c_str(), &attr);
  texture.get()->file_mod_t = attr.st_mtime;

  if (t.process_premultiply)
  {
    ASSERT(t.format == GL_RGBA);
    ASSERT(data_format == GL_UNSIGNED_BYTE);
    for (int32 i = 0; i < width * height; ++i)
    {
      uint32 pixel = ((uint32 *)data)[i];
      uint8 r = (uint8)(0x000000FF & pixel);
      uint8 g = (uint8)((0x0000FF00 & pixel) >> 8);
      uint8 b = (uint8)((0x00FF0000 & pixel) >> 16);
      uint8 a = (uint8)((0xFF000000 & pixel) >> 24);

      r = (uint8)round(r * ((float)a / 255));
      g = (uint8)round(g * ((float)a / 255));
      b = (uint8)round(b * ((float)a / 255));

      ((uint32 *)data)[i] = (24 << a) | (16 << b) | (8 << g) | r;
    }
  }

  t.size = ivec2(width, height);
  glGenTextures(1, &texture->texture);

  set_message(s("Texture load cache miss. Texture from disk: ", t.name, " Generated handle: ", texture->texture,
                  "\nInternal_Format: ", texture_format_to_string(t.format)),
      "", 3.f);
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, t.format, t.size.x, t.size.y, 0, GL_RGBA, data_format, data);
  glTexParameterf(GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, MAX_ANISOTROPY);

  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &t.border_color[0]);
  check_set_parameters();
  glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(data);
  glBindTexture(GL_TEXTURE_2D, 0);
  texture->filename = t.name;
  texture->size = t.size;
  texture->format = t.format;
  texture->border_color = t.border_color;
  t.magnification_filter = GL_LINEAR;
  t.minification_filter = GL_LINEAR_MIPMAP_LINEAR;
  initialized = true;
}

void Texture::check_set_parameters()
{
  // check/set desired state:
  ASSERT(t.magnification_filter != GLenum(0));
  ASSERT(t.minification_filter != GLenum(0));
  ASSERT(t.wrap_t != GLenum(0));
  ASSERT(t.wrap_s != GLenum(0));

  if (t.magnification_filter != texture->magnification_filter)
  {
    texture->magnification_filter = t.magnification_filter;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t.magnification_filter);
  }
  if (t.minification_filter != texture->minification_filter)
  {
    texture->minification_filter = t.minification_filter;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t.minification_filter);
  }
  if (t.wrap_t != texture->wrap_t)
  {
    texture->wrap_t = t.wrap_t;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t.wrap_t);
  }
  if (t.wrap_s != texture->wrap_s)
  {
    texture->wrap_s = t.wrap_s;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, t.wrap_s);
  }
  if (t.border_color != texture->border_color)
  {
    texture->border_color = t.border_color;
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &t.border_color[0]);
  }
  if (t.anisotropic_filtering != texture->anisotropic_filtering)
  {
    texture->anisotropic_filtering = t.anisotropic_filtering;
    glTexParameterf(GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, MAX_ANISOTROPY);
  }
}

void Texture::bind(GLuint binding)
{
// set_message(s("Texture::bind(", binding, ")"));
#if DYNAMIC_TEXTURE_RELOADING
  load();
#endif

  if (!initialized)
  {
    load();
  }
  if (!texture)
  {
    // set_message("Null texture for binding:", s(binding));
    glActiveTexture(GL_TEXTURE0 + binding);
    glBindTexture(GL_TEXTURE_2D, 0);
    return;
  }

  // descriptor desync - the handle should have been dropped and reloaded:
  ASSERT(t.name == texture->filename);
  ASSERT(t.format == texture->format);
  ASSERT(t.size == texture->size);

  // set_message(s("Binding texture name:",texture->filename," with handle:",
  // texture->texture, " to binding:", s(binding)));
  glActiveTexture(GL_TEXTURE0 + binding);
  glBindTexture(GL_TEXTURE_2D, texture->texture);

  check_set_parameters();
}

// not guaranteed to be constant for every call -
// handle lifetime only guaranteed for as long as the Texture object
// is held, and if dynamic texture reloading has not been triggered
// by a file modification - if it is, it will gracefully display
// black, or a random other texture

GLuint Texture::get_handle()
{
  if (!initialized)
    load();

  return texture ? texture->texture : 0;
}

Mesh::Mesh() {}

Mesh::Mesh(const Mesh_Descriptor &d) : Mesh(&d) {}
Mesh::Mesh(const Mesh_Descriptor *d)
{
  name = d->name;
  std::string unique_identifier = d->build_unique_identifier();
  shared_ptr<Mesh_Handle> ptr = MESH_CACHE[unique_identifier].lock();
  if (ptr)
  {
    mesh = ptr;
    return;
  }
  ptr = std::make_shared<Mesh_Handle>();
  ptr->descriptor = *d;

  if (ptr->descriptor.primitive != null)
  {
    ASSERT(ptr->descriptor.mesh_data.positions.size() == 0); // a primitive set and custom mesh data??
    ptr->descriptor.mesh_data = load_mesh(ptr->descriptor.primitive);
  }
  MESH_CACHE[unique_identifier] = mesh = upload_data(ptr->descriptor.mesh_data);
  mesh->enable_assign_attributes();
}

void Mesh::draw()
{
  mesh->enable_assign_attributes(); // also binds vao
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, get_indices_buffer());
  glDrawElements(GL_TRIANGLES, get_indices_buffer_size(), GL_UNSIGNED_INT, nullptr);
}

void Mesh_Handle::enable_assign_attributes()
{
  ASSERT(vao);
  glBindVertexArray(vao);

  if (position_buffer)
  {
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
  }

  if (normal_buffer)
  {
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
  }

  if (uv_buffer)
  {
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float32) * 2, 0);
  }

  if (tangents_buffer)
  {
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, tangents_buffer);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
  }

  if (bitangents_buffer)
  {
    glEnableVertexAttribArray(4);
    glBindBuffer(GL_ARRAY_BUFFER, bitangents_buffer);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
  }
}

shared_ptr<Mesh_Handle> Mesh::upload_data(const Mesh_Data &mesh_data)
{
  // opengl calls no longer allowed in state.update()
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);

  shared_ptr<Mesh_Handle> mesh = make_shared<Mesh_Handle>();
  mesh->descriptor.mesh_data = mesh_data;

  if (sizeof(decltype(mesh_data.indices)::value_type) != sizeof(uint32))
  {
    set_message("Mesh::upload_data error: render() assumes index type to be 32 bits");
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

  ASSERT(all_equal(positions_buffer_size, normal_buffer_size, uv_buffer_size, tangents_size, bitangents_size));

  // positions
  uint32 buffer_size = mesh_data.positions.size() * sizeof(decltype(mesh_data.positions)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->position_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.positions[0], GL_STATIC_DRAW);

  // normals
  buffer_size = mesh_data.normals.size() * sizeof(decltype(mesh_data.normals)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->normal_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.normals[0], GL_STATIC_DRAW);

  // texture_coordinates
  buffer_size = mesh_data.texture_coordinates.size() * sizeof(decltype(mesh_data.texture_coordinates)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.texture_coordinates[0], GL_STATIC_DRAW);

  // indices
  buffer_size = mesh_data.indices.size() * sizeof(decltype(mesh_data.indices)::value_type);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer_size, &mesh_data.indices[0], GL_STATIC_DRAW);

  // tangents
  buffer_size = mesh_data.tangents.size() * sizeof(decltype(mesh_data.tangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->tangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.tangents[0], GL_STATIC_DRAW);

  // bitangents
  buffer_size = mesh_data.bitangents.size() * sizeof(decltype(mesh_data.bitangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->bitangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.bitangents[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  return mesh;
}

Material::Material() {}
Material::Material(Material_Descriptor &m)
{
  load(m);
}

void Material::load(Material_Descriptor &mat)
{
  // the formats set here may be overriden in the texture constructor
  // based on if its a generated color or not

  descriptor = mat;

  descriptor.albedo.format = GL_SRGB8_ALPHA8;
  albedo = Texture(descriptor.albedo);

  descriptor.normal.format = GL_RGBA;
  normal = Texture(descriptor.normal);

  descriptor.emissive.format = GL_SRGB8_ALPHA8;
  emissive = Texture(descriptor.emissive);

  descriptor.roughness.format = GL_R8;
  roughness = Texture(descriptor.roughness);

  descriptor.metalness.format = GL_R8;
  metalness = Texture(descriptor.metalness);

  descriptor.ambient_occlusion.format = GL_R8;
  ambient_occlusion = Texture(descriptor.ambient_occlusion);

  shader = Shader(descriptor.vertex_shader, descriptor.frag_shader);
}
void Material::bind(Shader *shader)
{
  if (descriptor.backface_culling)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  shader->set_uniform("discard_on_alpha", descriptor.discard_on_alpha);

  albedo.bind(Texture_Location::albedo);
  shader->set_uniform("texture0_mod", descriptor.albedo.mod);

  emissive.bind(Texture_Location::emissive);
  shader->set_uniform("texture1_mod", descriptor.emissive.mod);

  roughness.bind(Texture_Location::roughness);
  shader->set_uniform("texture2_mod", descriptor.roughness.mod);

  normal.bind(Texture_Location::normal);

  metalness.bind(Texture_Location::metalness);
  shader->set_uniform("texture4_mod", descriptor.metalness.mod);

  ambient_occlusion.bind(Texture_Location::ambient_occlusion);
  shader->set_uniform("texture5_mod", descriptor.ambient_occlusion.mod);
}
void Material::unbind_textures()
{
  glActiveTexture(GL_TEXTURE0 + Texture_Location::albedo);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::normal);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::emissive);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::roughness);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::ambient_occlusion);
  glBindTexture(GL_TEXTURE_2D, 0);
}
//
// bool Light::operator==(const Light &rhs) const
//{
//  bool b1 = position == rhs.position;
//  bool b2 = direction == rhs.direction;
//  bool b3 = color == rhs.color;
//  bool b4 = attenuation == rhs.attenuation;
//  bool b5 = ambient == rhs.ambient;
//  bool b6 = cone_angle == rhs.cone_angle;
//  bool b7 = type == rhs.type;
//  bool b8 = brightness == rhs.brightness;
//  return b1 & b2 & b3 & b4 & b5 & b6 & b7 & b8;
//}
//
// bool Light_Array::operator==(const Light_Array &rhs)
//{
//  if (light_count != rhs.light_count)
//    return false;
//  if (lights != rhs.lights)
//  {
//    return false;
//  }
//  if (additional_ambient != rhs.additional_ambient)
//  {
//    return false;
//  }
//  return true;
//}
Render_Entity::Render_Entity(Array_String n, Mesh *mesh, Material *material, mat4 world_to_model)
    : mesh(mesh), material(material), transformation(world_to_model), name(n)
{
  ASSERT(mesh);
  ASSERT(material);
}

Renderer::Renderer(SDL_Window *window, ivec2 window_size, string name)
{
  set_message("Renderer constructor");
  this->name = name;
  if (!window)
    return;

  this->window = window;
  this->window_size = window_size;
  SDL_DisplayMode current;
  SDL_GetCurrentDisplayMode(0, &current);
  target_frame_time = 1.0f / (float32)current.refresh_rate;
  set_vfov(vfov);

  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  set_message("Initializing renderer...");

  quad = Mesh(Mesh_Descriptor(plane, "RENDERER's PLANE"));

  Texture_Descriptor uvtd;
  uvtd.name = "uvgrid.png";
  uv_map_grid = uvtd;

  const ivec2 brdf_lut_size = ivec2(512, 512);
  Shader brdf_lut_generator = Shader("passthrough.vert", "brdf_lut_generator.frag");
  brdf_integration_lut = Texture("brdf_lut", brdf_lut_size, GL_RG16F, GL_LINEAR);
  Framebuffer lut_fbo;
  lut_fbo.color_attachments[0] = brdf_integration_lut;
  lut_fbo.init();
  lut_fbo.bind();
  auto mat = ortho_projection(brdf_lut_size);
  glViewport(0, 0, brdf_lut_size.x, brdf_lut_size.y);
  brdf_lut_generator.set_uniform("transform", mat);
  quad.draw();

  set_message("Renderer init finished");
  init_render_targets();
  FRAME_TIMER.start();
}

void Renderer::set_uniform_shadowmaps(Shader &shader)
{
  ASSERT(spotlight_shadow_maps.size() == MAX_LIGHTS);

  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    if (!(i < lights.light_count))
      return;

    const Spotlight_Shadow_Map *shadow_map = &spotlight_shadow_maps[i];

    if (shadow_map->enabled)
      spotlight_shadow_maps[i].blur.target.color_attachments[0].bind(Texture_Location::s0 + i);

    const mat4 offset = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.5, 0.5, 0.5, 1.0);
    const mat4 shadow_map_transform = offset * shadow_map->projection_camera;

    const Light_Uniform_Location_Cache *location_cache = &shader.program->light_locations_cache[i];
    Light_Uniform_Value_Cache *value_cache = &shader.program->light_values_cache[i];

    float32 new_max_variance = lights.lights[i].max_variance;
    if (value_cache->max_variance != new_max_variance)
    {
      value_cache->max_variance = new_max_variance;
      glUniform1f(location_cache->max_variance, new_max_variance);
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

mat4 Renderer::ortho_projection(ivec2 dst_size)
{
  mat4 o = ortho(0.0f, (float32)dst_size.x, 0.0f, (float32)dst_size.y, 0.1f, 100.0f);
  mat4 t = translate(vec3(vec2(0.5f * dst_size.x, 0.5f * dst_size.y), -1));
  mat4 s = scale(vec3(dst_size, 1));

  return o * t * s;
}

void Renderer::draw_imgui()
{
  if (!imgui_this_tick)
    return;

  if (show_renderer_window)
  {
    ImGui::Begin("Renderer", &show_renderer_window);
    ImGui::BeginChild("ScrollingRegion");
    if (ImGui::Button("FXAA Settings"))
    {
      show_imgui_fxaa = !show_imgui_fxaa;
    }
    ImGui::SameLine();
    if (ImGui::Button("Bloom Settings"))
    {
      show_bloom = !show_bloom;
    }
    if (ImGui::CollapsingHeader("GPU Textures"))
    { // todo improve texture names for generated textures

      ImGui::Indent(5);
      vector<Imgui_Texture_Descriptor> imgui_texture_array; // all the unique textures
      for (auto &tex : TEXTURE2D_CACHE)
      {
        auto ptr = tex.second.lock();
        if (ptr)
        {
          ASSERT(!ptr->is_cubemap);
          Imgui_Texture_Descriptor iid;
          iid.ptr = ptr;
          imgui_texture_array.push_back(iid);
        }
      }
      for (auto &tex : TEXTURECUBEMAP_CACHE)
      {
        auto ptr = tex.second.lock();
        if (ptr)
        {
          ASSERT(ptr->is_cubemap);
          Imgui_Texture_Descriptor iid;
          iid.ptr = ptr;
          imgui_texture_array.push_back(iid);
        }
      }
      sort(imgui_texture_array.begin(), imgui_texture_array.end(),
          [](Imgui_Texture_Descriptor a, Imgui_Texture_Descriptor b) {
            return a.ptr->peek_filename().compare(b.ptr->peek_filename()) < 0;
          });
      for (uint32 i = 0; i < imgui_texture_array.size(); ++i)
      {
        Imgui_Texture_Descriptor *itd = &imgui_texture_array[i];
        shared_ptr<Texture_Handle> ptr = itd->ptr;

        ImGui::PushID(s(i).c_str());
        if (ImGui::TreeNode(ptr->peek_filename().c_str()))
        {
          ImGui::Text(s("Heap Address:", (uint32)ptr.get()).c_str());
          ImGui::Text(s("Ptr Refcount:", (uint32)ptr.use_count()).c_str());
          ImGui::Text(s("OpenGL handle:", ptr->texture).c_str());
          ImGui::Text(s("Format:", texture_format_to_string(ptr->format)).c_str());

          if (ptr->is_cubemap)
            ImGui::Text(s("Type:", "Cubemap").c_str());
          else
            ImGui::Text(s("Type:", "Texture2D").c_str());
          ImGui::Text(s("Size:", ptr->size.x, "x", ptr->size.y).c_str());
          Imgui_Texture_Descriptor descriptor;
          descriptor.ptr = ptr;
          GLenum format = descriptor.ptr->get_format();
          bool gamma_flag = format == GL_SRGB8_ALPHA8 || format == GL_SRGB || format == GL_RGBA16F ||
                            format == GL_RGBA32F || format == GL_RG16F || format == GL_RG32F || format == GL_RGB16F;

          descriptor.gamma_encode = gamma_flag;
          descriptor.is_cubemap = ptr->is_cubemap;

          ImGui::InputFloat("Thumbnail Size", &ptr->imgui_size_scale, 0.1f);
          ImGui::InputFloat("LOD", &ptr->imgui_mipmap_setting, 0.1f);
          descriptor.mip_lod_to_draw = ptr->imgui_mipmap_setting;
          descriptor.aspect = (float32)ptr->size.x / (float32)ptr->size.y;
          descriptor.size = ptr->imgui_size_scale * vec2(256);
          descriptor.y_invert = true;
          put_imgui_texture(&descriptor);

          if (ImGui::TreeNode("List Mipmaps"))
          {
            uint32 mip_levels = mip_levels_for_resolution(ptr->size);
            ImGui::Text(s("Mipmap levels: ", mip_levels).c_str());
            for (uint32 i = 0; i < mip_levels; ++i)
            {
              Imgui_Texture_Descriptor d = descriptor;
              d.mip_lod_to_draw = (float)i;
              d.is_mipmap_list_command = true;
              put_imgui_texture(&d);
            }
            ImGui::TreePop();
          }
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
      ImGui::Unindent(5.f);
    }
    ImGui::EndChild();
    ImGui::End();
  }
  else
  {
    show_imgui_fxaa = false;
  }
}
void Renderer::build_shadow_maps()
{
  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    spotlight_shadow_maps[i].enabled = false;
  }
  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    if (!(i < lights.light_count))
      return;

    const Light *light = &lights.lights[i];
    Spotlight_Shadow_Map *shadow_map = &spotlight_shadow_maps[i];

    if (!light->casts_shadows)
    {
      shadow_map->enabled = false; /*
       glBindFramebuffer(GL_FRAMEBUFFER, shadow_map->pre_blur.fbo->fbo);
       glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
       glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/
    }

    if (light->type != Light_Type::spot || !light->casts_shadows)
      continue;

    // todo improve shadow maps:
    // dont sample or clear disabled shadow maps every frame
    //

    shadow_map->enabled = true;
    ivec2 shadow_map_size = shadow_map_scale * vec2(light->shadow_map_resolution);
    shadow_map->init(shadow_map_size);
    shadow_map->blur.target.color_attachments[0].bind(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glViewport(0, 0, shadow_map_size.x, shadow_map_size.y);
    // glEnable(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glCullFace(GL_FRONT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_map->pre_blur.fbo->fbo);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const mat4 camera = lookAt(light->position, light->direction, vec3(0, 0, 1));
    const mat4 projection = perspective(light->shadow_fov, 1.0f, light->shadow_near_plane, light->shadow_far_plane);
    shadow_map->projection_camera = projection * camera;
    for (Render_Entity &entity : render_entities)
    {
      if (entity.material->descriptor.casts_shadows)
      {
        ASSERT(entity.mesh);
        int vao = entity.mesh->get_vao();
        glBindVertexArray(vao);
        variance_shadow_map.use();
        variance_shadow_map.set_uniform("transform", shadow_map->projection_camera * entity.transformation);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
        glDrawElements(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(), GL_UNSIGNED_INT, nullptr);
      }
    }

    shadow_map->blur.draw(
        this, &shadow_map->pre_blur.color_attachments[0], light->shadow_blur_radius, light->shadow_blur_iterations);

    shadow_map->blur.target.color_attachments[0].bind(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void run_pixel_shader(Shader *shader, vector<Texture> *src_textures, Framebuffer *dst, bool clear_dst)
{
  std::vector<Texture *> src;
  for (auto &t : *src_textures)
  {
    src.push_back(&t);
  }
  run_pixel_shader(shader, &src, dst, clear_dst);
}
// shader must use passthrough.vert, and must use texture1, texture2 ...
// texturen for input texture sampler names
void run_pixel_shader(Shader *shader, vector<Texture *> *src_textures, Framebuffer *dst, bool clear_dst)
{

  // opengl calls no longer allowed in state.update()
  // render your imgui ui that requires textures in state.render()
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  ASSERT(shader);
  ASSERT(shader->vs == "passthrough.vert");

  Mesh_Descriptor d;
  d.primitive = plane;
  d.name = "run_pixel_shader_quad";
  static Mesh quad = Mesh(d);

  if (dst)
  {
    ASSERT(dst->fbo);
    ASSERT(dst->color_attachments.size());
    dst->bind();
  }
  else
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

  for (uint32 i = 0; i < src_textures->size(); ++i)
  {
    (*src_textures)[i]->bind(i);
  }
  ivec2 viewport_size = dst->color_attachments[0].t.size;
  glViewport(0, 0, viewport_size.x, viewport_size.y);

  if (clear_dst)
  {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  // glBindVertexArray(quad.get_vao());
  shader->use();
  shader->set_uniform("transform", Renderer::ortho_projection(viewport_size));
  shader->set_uniform("time", (float32)get_real_time());
  // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer());
  // glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);
  quad.draw();

  for (uint32 i = 0; i < src_textures->size(); ++i)
  {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

void Renderer::opaque_pass(float32 time)
{
  // set_message("opaque_pass()");
  glViewport(0, 0, size.x, size.y);
  draw_target.bind();

  glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  // todo: depth pre-pass

  // set_message("opaque_pass projection:", s(projection), 1.0f);
  // set_message("opaque_pass camera:", s(camera), 1.0f);

#if SHOW_UV_TEST_GRID
  uv_map_grid.bind(uv_grid);
#endif
  brdf_integration_lut.bind(brdf_ibl_lut);
  environment.bind(Texture_Location::environment, Texture_Location::irradiance);
  for (Render_Entity &entity : render_entities)
  {
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    // set_message(s("drawing entity with name:", entity.name));
    // set_message(s("binding vao:", vao));
    glBindVertexArray(vao);
    Shader &shader = CONFIG.render_simple ? simple : entity.material->shader;
    shader.use();
    entity.material->bind(&shader);
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->descriptor.uv_scale);
    shader.set_uniform("normal_uv_scale", entity.material->descriptor.normal_uv_scale);
    shader.set_uniform("MVP", projection * camera * entity.transformation);
    shader.set_uniform("Model", entity.transformation);
    shader.set_uniform("alpha_albedo_override", -1.0f); //-1 is disabled
#if SHOW_UV_TEST_GRID
    shader.set_uniform("texture10_mod", uv_map_grid.t.mod);
#endif
    lights.bind(shader);
    set_uniform_shadowmaps(shader);
    entity.mesh->draw();
    entity.material->unbind_textures();
  }
  glActiveTexture(GL_TEXTURE0 + Texture_Location::environment);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::irradiance);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Renderer::instance_pass(float32 time)
{
  for (auto &entity : render_instances)
  {
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    glBindVertexArray(vao);
    Shader &shader = entity.material->shader;
    ASSERT(shader.vs == "instance.vert");
    shader.use();
    entity.material->bind(&shader);
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->descriptor.uv_scale);
    lights.bind(shader);

    ASSERT(glGetAttribLocation(shader.program->program, "instanced_MVP") == 5);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    glEnableVertexAttribArray(7);
    glEnableVertexAttribArray(8);
    glBindBuffer(GL_ARRAY_BUFFER, entity.mvp_buffer);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(0));
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(sizeof(GLfloat) * 4));
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(sizeof(GLfloat) * 8));
    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(sizeof(GLfloat) * 12));
    glVertexAttribDivisor(5, 1); //"update attribute at location <N> every <1> instance"
    glVertexAttribDivisor(6, 1);
    glVertexAttribDivisor(7, 1);
    glVertexAttribDivisor(8, 1);

    GLint instanced_model_location = glGetAttribLocation(shader.program->program, "instanced_model");
    if (instanced_model_location != -1)
    {
      ASSERT(instanced_model_location == 9);
      glEnableVertexAttribArray(9);
      glEnableVertexAttribArray(10);
      glEnableVertexAttribArray(11);
      glEnableVertexAttribArray(12);
      glBindBuffer(GL_ARRAY_BUFFER, entity.model_buffer);
      glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(0));
      glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(sizeof(GLfloat) * 4));
      glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(sizeof(GLfloat) * 8));
      glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(sizeof(GLfloat) * 12));
      glVertexAttribDivisor(9, 1);
      glVertexAttribDivisor(10, 1);
      glVertexAttribDivisor(11, 1);
      glVertexAttribDivisor(12, 1);
    }

    int32 loc0 = glGetAttribLocation(shader.program->program, "attribute0");
    if (loc0 != -1)
    {
      ASSERT(loc0 == 13);
      glEnableVertexAttribArray(13);
      glBindBuffer(GL_ARRAY_BUFFER, entity.attribute0_buffer);
      glVertexAttribPointer(13, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)(0));
      glVertexAttribDivisor(13, 1);
    }

    int32 loc1 = glGetAttribLocation(shader.program->program, "attribute1");
    if (loc1 != -1)
    {
      ASSERT(loc1 == 14);
      glEnableVertexAttribArray(14);
      glBindBuffer(GL_ARRAY_BUFFER, entity.attribute1_buffer);
      glVertexAttribPointer(14, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)(0));
      glVertexAttribDivisor(14, 1);
    }

    int32 loc2 = glGetAttribLocation(shader.program->program, "attribute2");
    if (loc2 != -1)
    {
      ASSERT(loc2 == 15);
      glEnableVertexAttribArray(15);
      glBindBuffer(GL_ARRAY_BUFFER, entity.attribute2_buffer);
      glVertexAttribPointer(15, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)(0));
      glVertexAttribDivisor(15, 1);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    glDrawElementsInstanced(
        GL_TRIANGLES, entity.mesh->get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0, entity.size);

    entity.material->unbind_textures();

    glDisableVertexAttribArray(6);
    glDisableVertexAttribArray(7);
    glDisableVertexAttribArray(8);
    glDisableVertexAttribArray(9);
    glDisableVertexAttribArray(10);
    glDisableVertexAttribArray(11);
    glDisableVertexAttribArray(12);
    glDisableVertexAttribArray(13);
    glDisableVertexAttribArray(14);
    glDisableVertexAttribArray(15);
  }
}

void Renderer::translucent_pass(float32 time)
{

  // ivec2 current_size = size;
  // bool need_to_allocate_textures = downscaled_render_targets_for_translucent_pass.size == 0;
  // const uint32 min_width = 40;
  // while (size.x > min_width)
  //{
  //  // allocate texture if needed

  //  // render draw_target, blurred downscaled by 50% from previous resolution

  //  // blur needs to be resolution independent - kernel distance must be % of
  //  // resolution not fixed
  //}

  // put these in a mipmap like the specular convolution
  // sample these maps in the translucent pass - use the material roughness to
  // pick a LOD  overwrite all pixels touched by translucent objects of the
  // original render target with this new blurred-by-roughness value, as well as
  // blending on top with the object itself?

  // how to handle blurry window in front of blurry window?
  // you have to either let it be wrong, or re-do the downscaling after each
  // translucent object draw

  // touch all translucent object pixels
  // store the total accumulated roughness for each pixel into a texture
  // draw full screen quad, passthrough, but selecting lod blur level by the
  // accumulated roughness

  // render all translucent objects back to front
  // store
  // sample accumulated roughness for each

  // glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_BACK);
  brdf_integration_lut.bind(brdf_ibl_lut);
  environment.bind(Texture_Location::environment, Texture_Location::irradiance);
  for (Render_Entity &entity : translucent_entities)
  {
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    glBindVertexArray(vao);
    Shader &shader = CONFIG.render_simple ? simple : entity.material->shader;
    shader.use();
    entity.material->bind(&shader);
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->descriptor.uv_scale);
    shader.set_uniform("MVP", projection * camera * entity.transformation);
    shader.set_uniform("Model", entity.transformation);
    shader.set_uniform("discard_on_alpha", false);
    shader.set_uniform("alpha_albedo_override", entity.material->descriptor.albedo_alpha_override);

#if SHOW_UV_TEST_GRID
    uv_map_grid.bind(10);
    shader.set_uniform("texture10_mod", uv_map_grid.t.mod);
#endif

    lights.bind(shader);
    set_uniform_shadowmaps(shader);

    glDisable(GL_CULL_FACE);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    glDrawElements(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(), GL_UNSIGNED_INT, nullptr);

    entity.material->unbind_textures();
  }
  glEnable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

void Renderer::postprocess_pass(float32 time)
{
  // Scene Luminance

  //    This step calculates the average luminance of the scene,
  //    it is one of the parameters fed to the tonemapper later.

  //    The HDR lighting buffer is downscaled to half its resolution in a loop
  //        until it becomes a 2 x 2 texture,
  //    each iteration calculates the pixel color value as the average of the
  //            luminance of its 4 parent pixels from the higher -
  //        resolution map.

  {

    // calc scene luminance
  }

  // bloom:
  bool need_to_allocate_texture = bloom_target.get_handle() == 0;
  const float32 scale_relative_to_1080p = this->size.x / 1920.f;
  const int32 min_width = (int32)(80.f * scale_relative_to_1080p);
  vector<ivec2> resolutions = {size};
  while (resolutions.back().x / 2 > min_width)
  {
    resolutions.push_back(resolutions.back() / 2);
  }

  uint32 mip_levels = resolutions.size() - 1;

  if (need_to_allocate_texture)
  {
    bloom_intermediate = Texture(name + s("'s Bloom Intermediate"), size, GL_RGB16F, GL_LINEAR_MIPMAP_LINEAR);
    bloom_intermediate.bind(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip_levels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    bloom_target = Texture(name + s("'s Bloom Target"), size, GL_RGB16F, GL_LINEAR_MIPMAP_LINEAR);
    bloom_target.bind(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip_levels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    for (uint32 i = 0; i < resolutions.size(); ++i)
    {
      ivec2 resolution = resolutions[i];
      bloom_intermediate.bind(0);
      glTexImage2D(GL_TEXTURE_2D, i, GL_RGB16F, resolution.x, resolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);
      bloom_target.bind(0);
      glTexImage2D(GL_TEXTURE_2D, i, GL_RGB16F, resolution.x, resolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);
    }
  }
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_BACK);
  glDisable(GL_DEPTH_TEST);

  // apply high pass filter
  bloom_fbo.color_attachments[0] = bloom_target;
  bloom_fbo.init();
  bloom_fbo.bind();
  high_pass_shader.use();
  high_pass_shader.set_uniform("transform", ortho_projection(size));
  glViewport(0, 0, size.x, size.y);
  draw_target.color_attachments[0].bind(0);
  quad.draw();

  static bool enabled = true;
  if (imgui_this_tick)
  {
    if (show_bloom)
    {
      ImGui::Begin("bloom", &show_bloom);
      ImGui::Checkbox("Enabled", &enabled);
      ImGui::DragFloat("blur_radius", &blur_radius, 0.00001, 0.0f, 0.5f, "%.6f");
      ImGui::DragFloat("mip_scale_factor", &blur_factor, 0.001, 0.0f, 10.5f, "%.3f");
      ImGui::Text(s("mip_count:", mip_levels).c_str());
      ImGui::End();
    }
  }

  float32 original = blur_radius;
  // in a loop
  // blur: src:bloom_target, dst:intermediate (x), src:intermediate, dst:target (y)
  for (uint32 i = 0; i < mip_levels; ++i)
  {
    ivec2 resolution = resolutions[i + 1];
    gaussian_blur_15x.use();
    glViewport(0, 0, resolution.x, resolution.y);

    bloom_target.bind(0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_intermediate.get_handle(), i + 1);
    float32 aspect_ratio_factor = (float32)resolution.y / (float32)resolution.x;
    vec2 gaus_scale = vec2(aspect_ratio_factor * blur_radius, 0.0);
    gaussian_blur_15x.set_uniform("gauss_axis_scale", gaus_scale);
    gaussian_blur_15x.set_uniform("lod", i);
    gaussian_blur_15x.set_uniform("transform", ortho_projection(resolution));
    quad.draw();

    gaus_scale = vec2(0.0f, blur_radius);
    gaussian_blur_15x.set_uniform("gauss_axis_scale", gaus_scale);
    bloom_intermediate.bind(0);
    gaussian_blur_15x.set_uniform("lod", i + 1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_target.get_handle(), i + 1);
    quad.draw();

    blur_radius += (blur_factor * blur_radius);
  }
  blur_radius = original;
  // bloom target is now the screen high pass filtered, lod0 no blur, increasing bluriness on each mip level below that

  if (!bloom_result.get_handle())
  {
    Texture_Descriptor td;
    td.name = name + "'s bloom_result";
    // td.name = "color(0.15,0,0,1)";
    td.size = size / 1;
    td.format = GL_RGB16F;
    td.minification_filter = GL_LINEAR;
    td.wrap_s = GL_CLAMP_TO_EDGE;
    td.wrap_t = GL_CLAMP_TO_EDGE;
    bloom_result = td;
    bloom_result.bind(0);
  }
  // now we need to mix the lods and put them in the bloom_result texture
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_result.get_handle(), 0);
  bloom_mix.use();
  bloom_target.bind(0);
  glViewport(0, 0, bloom_result.t.size.x, bloom_result.t.size.y);
  bloom_mix.set_uniform("transform", ortho_projection(bloom_result.t.size));
  bloom_mix.set_uniform("mip_count", mip_levels + 1); //+1 because we index from 0
  quad.draw();
  // all of these bloom textures should now be averaged and drawn into a texture at 1/4 the source resolution

  // add to draw_target
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  draw_target.bind();
  bloom_result.bind(0);
  glViewport(0, 0, size.x, size.y);
  passthrough.use();
  passthrough.set_uniform("transform", ortho_projection(size));
  if (enabled)
    quad.draw();
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_BLEND);
}

void Renderer::render(float64 state_time)
{
  check_gl_error();
// set_message("sin(time) [-1,1]:", s(sin(state_time)), 1.0f);
// set_message("sin(time) [ 0,1]:", s(0.5f + 0.5f * sin(state_time)), 1.0f);
#if DYNAMIC_FRAMERATE_TARGET
  dynamic_framerate_target();
#endif
#if DYNAMIC_TEXTURE_RELOADING
  check_and_clear_expired_textures();
#endif
#if SHOW_UV_TEST_GRID
  uv_map_grid.t.mod = vec4(1, 1, 1, clamp((float32)pow(sin(state_time), .25f), 0.0f, 1.0f));
#endif
  if (imgui_this_tick)
  {
    draw_imgui();
  }

  float32 time = (float32)get_real_time();
  float64 t = (time - state_time) / dt;

  // set_message("FRAME_START", "");
  // set_message("BUILDING SHADOW MAPS START", "");
  if (!CONFIG.render_simple)
  {
    build_shadow_maps();
  }
  // set_message("OPAQUE PASS START", "");

  opaque_pass(time);

  instance_pass(time);

  translucent_pass(time);

#if POSTPROCESSING
  postprocess_pass(time);
#else

#endif

  glDisable(GL_DEPTH_TEST);
  if (previous_camera != camera)
  {
    previous_color_target_missing = true;
  }

  if (use_txaa && previous_camera == camera)
  {
    txaa_jitter = get_next_TXAA_sample();
    // TODO: implement motion vector vertex attribute

    // 1: blend draw_target with previous_draw_target, store in
    // draw_target_srgb8  2: copy draw_target to previous_draw_target  3: run
    // fxaa on draw_target_srgb8, drawing to screen

    glBindVertexArray(quad.get_vao());
    draw_target_srgb8.bind();
    glViewport(0, 0, size.x, size.y);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    temporalaa.use();

    // blend draw_target with previous_draw_target, store in draw_target_srgb8
    if (previous_color_target_missing)
    {
      draw_target.color_attachments[0].bind(0);
      draw_target.color_attachments[0].bind(1);
    }
    else
    {
      draw_target.color_attachments[0].bind(0);
      previous_draw_target.color_attachments[0].bind(1);
    }

    temporalaa.set_uniform("transform", ortho_projection(window_size));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);

    // copy draw_target to previous_draw_target
    previous_draw_target.bind();
    glViewport(0, 0, size.x, size.y);
    passthrough.use();
    draw_target.color_attachments[0].bind(0);
    passthrough.set_uniform("transform", ortho_projection(window_size));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

    previous_color_target_missing = false;
    glBindTexture(GL_TEXTURE_2D, 0);
  }
  else
  {
    // draw to srgb8 framebuffer
    glViewport(0, 0, size.x, size.y);
    glBindVertexArray(quad.get_vao());
    draw_target_srgb8.bind();
    glEnable(GL_FRAMEBUFFER_SRGB); // draw_target_srgb8 will do its own gamma
                                   // encoding
    passthrough.use();
    passthrough.set_uniform("transform", ortho_projection(size));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_target.color_attachments[0].bind(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  if (use_fxaa)
    previous_camera = camera;

  // do fxaa or passthrough if disabled
  draw_target_fxaa.bind();
  if (use_fxaa)
  {
    static float EDGE_THRESHOLD_MIN = 0.0312;
    static float EDGE_THRESHOLD_MAX = 0.125;
    static float SUBPIXEL_QUALITY = 0.75f;
    static int ITERATIONS = 8;
    static int QUALITY = 1;

    if (imgui_this_tick && show_imgui_fxaa)
    {
      ImGui::Begin("fxaa adjustment", &show_imgui_fxaa);
      ImGui::SetWindowSize(ImVec2(300, 160));
      ImGui::DragFloat("EDGE_MIN", &EDGE_THRESHOLD_MIN, 0.001f);
      ImGui::DragFloat("EDGE_MAX", &EDGE_THRESHOLD_MAX, 0.001f);
      ImGui::DragFloat("SUBPIXEL", &SUBPIXEL_QUALITY, 0.001f);
      ImGui::DragInt("ITERATIONS", &ITERATIONS, 0.1f);
      ImGui::DragInt("QUALITY", &QUALITY, 0.1f);
      ImGui::End();
    }

    fxaa.use();
    fxaa.set_uniform("EDGE_THRESHOLD_MIN", EDGE_THRESHOLD_MIN);
    fxaa.set_uniform("EDGE_THRESHOLD_MAX", EDGE_THRESHOLD_MAX);
    fxaa.set_uniform("SUBPIXEL_QUALITY", SUBPIXEL_QUALITY);
    fxaa.set_uniform("ITERATIONS", ITERATIONS);
    fxaa.set_uniform("QUALITY", QUALITY);
    fxaa.set_uniform("transform", ortho_projection(window_size));
    fxaa.set_uniform("inverseScreenSize", vec2(1.0f) / vec2(window_size));
    fxaa.set_uniform("time", (float32)state_time);
  }
  else
  {
    passthrough.use();
    passthrough.set_uniform("transform", ortho_projection(window_size));
  }

  // this is drawn to another intermediate framebuffer so fxaa uses all the
  // fragments of the original render scale
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  draw_target_srgb8.color_attachments[0].bind(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  // draw draw_target_post_fxaa to screen
  glViewport(0, 0, window_size.x, window_size.y);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glEnable(GL_FRAMEBUFFER_SRGB);
  passthrough.use();
  passthrough.set_uniform("transform", ortho_projection(window_size));
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  draw_target_fxaa.color_attachments[0].bind(0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  FRAME_TIMER.stop();
  SWAP_TIMER.start();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glBindVertexArray(0);

  frame_count += 1;
}

vec3 Renderer::ray_from_screen_pixel(ivec2 pixel)
{
  const vec3 origin = camera_position;
  set_message("origin", s(origin.x, " ", origin.y, " ", origin.z), 5);
  const vec3 camera_direction = camera_gaze - origin;
  set_message("camera_direction", s(camera_direction.x, " ", camera_direction.y, " ", camera_direction.z), 5);
  const vec3 origin_to_center = origin + (znear * normalize(camera_direction));
  set_message("origin_to_center", s(origin_to_center.x, " ", origin_to_center.y, " ", origin_to_center.z), 5);
  const vec2 pixel_normalized = vec2(pixel) / vec2(window_size);
  set_message("pixel_normalized", s(pixel_normalized.x, " ", pixel_normalized.y), 5);
  const vec3 screen_ndc = vec3(2.0f * (pixel_normalized - vec2(0.5f)), 0);
  set_message("screen_ndc", s(screen_ndc.x, " ", screen_ndc.y, " ", screen_ndc.z), 5);
  const vec3 x_axis = camera[0];
  set_message("x_axis", s(x_axis.x, " ", x_axis.y, " ", x_axis.z), 5);
  const vec3 y_axis = camera[1];
  set_message("y_axis", s(y_axis.x, " ", y_axis.y, " ", y_axis.z), 5);
  const vec3 z_axis = camera[2];
  set_message("z_axis", s(z_axis.x, " ", z_axis.y, " ", z_axis.z), 5);
  const vec3 center_to_p = (screen_ndc.x * x_axis) + (screen_ndc.y * y_axis);
  set_message("center_to_p", s(center_to_p.x, " ", center_to_p.y, " ", center_to_p.z), 5);
  const vec3 origin_to_p = origin_to_center + center_to_p;
  set_message("origin_to_p", s(origin_to_p.x, " ", origin_to_p.y, " ", origin_to_p.z), 5);
  const vec3 returnval = vec3(0) - normalize(origin_to_p);
  set_message("returnval", s(returnval.x, " ", returnval.y, " ", returnval.z), 5);

  return returnval;
}

void Renderer::resize_window(ivec2 window_size)
{
  ASSERT(0);
  // todo: implement window resize, must notify imgui
  this->window_size = window_size;
  set_vfov(vfov);
}

void Renderer::set_render_scale(float32 scale)
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

void Renderer::set_camera(vec3 pos, vec3 camera_gaze_dir)
{
  vec3 p = pos + camera_gaze_dir;
  camera_gaze = p;
  camera_position = pos;
  camera = lookAt(pos, p, {0, 0, 1});
}
void Renderer::set_camera_gaze(vec3 pos, vec3 p)
{
  camera_gaze = p;
  camera_position = pos;
  camera = lookAt(pos, p, {0, 0, 1});
}

void Renderer::set_vfov(float32 vfov)
{
  const float32 aspect = (float32)window_size.x / (float32)window_size.y;
  const float32 znear = 0.1f;
  const float32 zfar = 10000;
  projection = perspective(radians(vfov), aspect, znear, zfar);
  this->znear = znear;
}

void Renderer::set_render_entities(vector<Render_Entity> *new_entities)
{
  previous_render_entities = move(render_entities);
  render_entities.clear();
  translucent_entities.clear();

  // calculate the distance from the camera for each entity at index in
  // new_entities  sort them  split opaque and translucent entities
  vector<pair<uint32, float32>> index_distances; // for insert sorted, -1 means index-omit

  for (uint32 i = 0; i < new_entities->size(); ++i)
  {
    Render_Entity *entity = &(*new_entities)[i];
    mat4 *m = &entity->transformation;
    vec3 translation = vec3((*m)[3][0], (*m)[3][1], (*m)[3][2]);

    float32 dist = length(translation - camera_position);
    bool is_transparent = entity->material->descriptor.uses_transparency;
    if (is_transparent)
    {
      index_distances.push_back({i, dist});
    }
    else
    {
      index_distances.push_back({i, -1.0f});
    }
  }

  sort(index_distances.begin(), index_distances.end(),
      [](pair<uint32, float32> p1, pair<uint32, float32> p2) { return p1.second > p2.second; });

  for (auto i : index_distances)
  {
    if (i.second != -1.0f)
    {
      translucent_entities.push_back((*new_entities)[i.first]);
    }
    else
    {
      render_entities.push_back((*new_entities)[i.first]);
    }
  }
}

void Renderer::set_lights(Light_Array new_lights)
{
  lights = new_lights;
  environment = lights.environment;
}

void check_FBO_status()
{
  auto result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  string str;
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

void Renderer::init_render_targets()
{
  set_message("init_render_targets()");

  size = ivec2(render_scale * window_size.x, render_scale * window_size.y);

  // these are setup in the bloom function itself
  bloom_result = Texture();
  bloom_intermediate = Texture();
  bloom_target = Texture();
  bloom_fbo = Framebuffer();

  Texture_Descriptor td;
  td.name = name + " Renderer::draw_target.color[0]";
  td.size = size;
  td.format = FRAMEBUFFER_FORMAT;
  td.minification_filter = GL_LINEAR;
  draw_target.color_attachments[0] = Texture(td);
  draw_target.depth_enabled = true;
  draw_target.depth_size = size;
  draw_target.init();

  td.name = name + " Renderer::previous_frame.color[0]";
  previous_draw_target.color_attachments[0] = Texture(td);
  previous_draw_target.init();

  // full render scaled, clamped and encoded srgb
  Texture_Descriptor srgb8;
  srgb8.name = name + " Renderer::draw_target_srgb8.color[0]";
  srgb8.size = size;
  srgb8.format = GL_SRGB8;
  srgb8.minification_filter = GL_LINEAR;
  draw_target_srgb8.color_attachments[0] = Texture(srgb8);
  draw_target_srgb8.encode_srgb_on_draw = true;
  draw_target_srgb8.init();

  // full render scaled, fxaa or passthrough target
  Texture_Descriptor fxaa;
  fxaa.name = name + " Renderer::draw_target_post_fxaa.color[0]";
  fxaa.size = size;
  fxaa.format = GL_SRGB8;
  fxaa.minification_filter = GL_LINEAR;
  draw_target_fxaa.color_attachments[0] = Texture(fxaa);
  draw_target_fxaa.encode_srgb_on_draw = true;
  draw_target_fxaa.init();
}

void Renderer::dynamic_framerate_target()
{
  // todo: fix dynamic framerate target
  ASSERT(0); // broken af
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

  if (FRAME_TIMER.sample_count() < min_samples || SWAP_TIMER.sample_count() < min_samples)
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
        "Number of missed frames over threshold. Reducing resolution.: ", s(percent_high) + " " + s(target_frame_time));
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
      set_message("High avg swap. Increasing resolution.", s(swap_avg) + " " + s(target_frame_time));
      set_render_scale(render_scale * increase_factor);
      uint64 last_start = FRAME_TIMER.get_begin();
      SWAP_TIMER.clear_all();
      FRAME_TIMER.clear_all();
      FRAME_TIMER.start(last_start);
    }
  }
}

mat4 Renderer::get_next_TXAA_sample()
{
  vec2 translation;
  if (jitter_switch)
    translation = vec2(0.5, 0.5);
  else
    translation = vec2(-0.5, -0.5);

  mat4 result = translate(vec3(translation / vec2(size), 0));
  jitter_switch = !jitter_switch;
  return result;
}

Mesh_Handle::~Mesh_Handle()
{
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

void Spotlight_Shadow_Map::init(ivec2 size)
{
  if (initialized)
  {
    if (pre_blur.depth_size == size)
    {
      ASSERT(blur.target.color_attachments.size() != 0);
      ASSERT(blur.target.color_attachments[0].t.size == size);
      return;
    }
  }

  pre_blur.depth_enabled = true;
  pre_blur.depth_size = size;
  pre_blur.depth_format = GL_DEPTH_COMPONENT32;

  Texture_Descriptor pre_blur_td;
  pre_blur_td.name = "Spotlight Shadow Map pre_blur[0]";
  pre_blur_td.size = size;
  pre_blur_td.format = format;
  pre_blur_td.minification_filter = GL_LINEAR;
  pre_blur.color_attachments[0] = Texture(pre_blur_td);
  pre_blur.init();

  ASSERT(pre_blur.color_attachments.size() == 1);

  Texture_Descriptor td;
  td.name = "Spotlight Shadow Map";
  td.size = size;
  td.format = format;
  td.border_color = vec4(999999, 999999, 0, 0);
  td.wrap_s = GL_CLAMP_TO_BORDER;
  td.wrap_t = GL_CLAMP_TO_BORDER;
  td.minification_filter = GL_LINEAR;
  blur.init(td);

  initialized = true;
}

Renderbuffer_Handle::~Renderbuffer_Handle()
{
  glDeleteRenderbuffers(1, &rbo);
}

Cubemap::Cubemap() {}

void Cubemap::bind(GLuint texture_unit)
{
  if (handle)
  {
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
  }
}

struct Image
{
  Image() {}
  Image(string filename)
  {
    float *d = stbi_loadf(filename.c_str(), &width, &height, &n, 4);
    n = 4;
    if (d)
    {
      this->filename = filename;
      data.resize(width * height * 4);

      for (int32 i = 0; i < width * height * 4; ++i)
      {
        float src = d[i];
        data[i] = src;
      }
      stbi_image_free(d);
    }
    else
    {
      set_message("Warning: Missing Texture:", filename, 5.0f);
    }
  }
  void rotate90()
  {
    const int floats_per_pixel = 4;
    int32 size = width * height * floats_per_pixel;
    Image result;
    result.data.resize(size);

    for (int32 y = 0; y < height; ++y)
    {
      for (int32 x = 0; x < width; ++x)
      {
        int32 src = floats_per_pixel * (y * width + x);

        int32 dst_x = (height - 1) - y;
        int32 dst_y = x;

        int32 dst = floats_per_pixel * (width * dst_y + dst_x);

        for (int32 i = 0; i < floats_per_pixel; ++i)
        {
          int32 src_index = src + i;
          int32 dst_index = dst + i;
          ASSERT(dst_index < size);
          result.data[dst_index] = data[src_index];
        }
      }
    }
    data = result.data;
  }
  string filename = "NULL";
  vector<float> data;
  int32 width = 0, height = 0, n = 0;
};

// opengl z-forward space is ordered: right left top bottom back front
// our coordinate space is +Z up, +X right, +Y forward
vector<Image> load_cubemap_faces(array<string, 6> filenames)
{
  Image right = filenames[0];
  Image left = filenames[1];
  Image top = filenames[2];
  Image bottom = filenames[3];
  Image back = filenames[4];
  Image front = filenames[5];

  right.rotate90();
  right.rotate90();
  right.rotate90();
  left.rotate90();
  bottom.rotate90();
  bottom.rotate90();
  front.rotate90();
  front.rotate90();

  vector<Image> result;
  result.resize(6);
  result[0] = left;
  result[1] = right;
  result[2] = back;
  result[3] = front;
  result[4] = bottom;
  result[5] = top;
  return result;
}

Cubemap::Cubemap(string equirectangular_filename)
{
  static Shader equi_to_cube("equi_to_cube.vert", "equi_to_cube.frag");
  static Mesh cube(Mesh_Descriptor(cube, "Cubemap(string equirectangular_filename)'s cube"));

  handle = TEXTURECUBEMAP_CACHE[equirectangular_filename].lock();
  if (handle)
  {
    size = handle->size;
    return;
  }
  handle = make_shared<Texture_Handle>();
  TEXTURECUBEMAP_CACHE[equirectangular_filename] = handle;
  handle->filename = equirectangular_filename;
  handle->is_cubemap = true;
  mat4 projection = perspective(half_pi<float>(), 1.0f, 0.01f, 10.0f);
  vec3 origin = vec3(0);
  vec3 posx = vec3(1.0f, 0.0f, 0.0f);
  vec3 negx = vec3(-1.0f, 0.0f, 0.0f);
  vec3 posy = vec3(0.0f, 1.0f, 0.0f);
  vec3 negy = vec3(0.0f, -1.0f, 0.0f);
  vec3 posz = vec3(0.0f, 0.0f, 1.0f);
  vec3 negz = vec3(0.0f, 0.0f, -1.0f);
  mat4 cameras[] = {lookAt(origin, posx, negy), lookAt(origin, negx, negy), lookAt(origin, posy, posz),
      lookAt(origin, negy, negz), lookAt(origin, posz, negy), lookAt(origin, negz, negy)};

  size = CONFIG.use_low_quality_specular ? ivec2(1024, 1024) : ivec2(2048, 2048);
  handle->size = size;

  // initialize targets and result cubemap
  GLuint fbo;
  GLuint rbo;
  glGenFramebuffers(1, &fbo);
  glGenRenderbuffers(1, &rbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &handle->texture);
  glActiveTexture(GL_TEXTURE0 + 6);
  glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
  for (uint32 i = 0; i < 6; ++i)
  {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, size.x, size.y, 0, GL_RGBA, GL_FLOAT, nullptr);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  handle->format = GL_RGB16F;

  float angle = radians(0.f);
  mat4 rot = toMat4(quat(1, 0, 0, angle));
  glViewport(0, 0, size.x, size.y);
  equi_to_cube.use();
  equi_to_cube.set_uniform("projection", projection);
  equi_to_cube.set_uniform("rotation", rot);

  check_gl_error();
  // load source texture
  Texture_Descriptor d;
  d.name = equirectangular_filename;
  d.format = GL_RGBA16F;
  Texture source(d);
  while (!source.is_initialized())
  { // stall for resource
    source.load();
  }

  check_gl_error();
  source.bind(0);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_FRONT);
  // draw to cubemap target
  for (uint32 i = 0; i < 6; ++i)
  {
    equi_to_cube.set_uniform("camera", cameras[i]);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, handle->texture, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    cube.draw();
  }
  glCullFace(GL_BACK);

  glActiveTexture(GL_TEXTURE0 + 6);
  glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  handle->magnification_filter = GL_LINEAR;
  handle->minification_filter = GL_LINEAR_MIPMAP_LINEAR;
  handle->wrap_s = GL_CLAMP_TO_EDGE;
  handle->wrap_t = GL_CLAMP_TO_EDGE;
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

  // cleanup targets
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteRenderbuffers(1, &rbo);
  glDeleteFramebuffers(1, &fbo);
  check_gl_error();
}

Cubemap::Cubemap(array<string, 6> filenames)
{
  string key = filenames[0] + filenames[1] + filenames[2] + filenames[3] + filenames[4] + filenames[5];
  handle = TEXTURECUBEMAP_CACHE[key].lock();
  if (handle)
  {
    return;
  }
  handle = make_shared<Texture_Handle>();
  TEXTURECUBEMAP_CACHE[key] = handle;
  handle->is_cubemap = true;

  set_message("Cubemap load cache miss. Textures from disk: ", key, 1.0);
  vector<Image> faces = load_cubemap_faces(filenames);

  for (uint32 i = 0; i < 6; ++i)
  {
    Image &face = faces[i];
    if (face.filename == "NULL")
    {
      handle = nullptr;
      return;
    }
  }
  size = ivec2(faces[0].width, faces[0].height);
  glGenTextures(1, &handle->texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
  for (uint32 i = 0; i < 6; ++i)
  {
    Image &face = faces[i];
    glTexImage2D(
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, face.width, face.height, 0, GL_RGBA, GL_FLOAT, &face.data[0]);
    ASSERT(face.width == size.x && face.height == size.y); // generate_ibl_mipmaps() assumes all sizes are equal
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Environment_Map::generate_ibl_mipmaps()
{
  static bool loaded = false;
  static Shader specular_filter;
  if (!loaded)
  {
    loaded = true;
    if (CONFIG.use_low_quality_specular)
    {
      specular_filter = Shader("equi_to_cube.vert", "specular_brdf_convolution - low.frag");
    }
    else
    {
      specular_filter = Shader("equi_to_cube.vert", "specular_brdf_convolution.frag");
    }
  }
  static Mesh cube(Mesh_Descriptor(cube, "generate_ibl_mipmaps()'s cube"));

  const uint32 mip_levels = 6;

  mat4 projection = perspective(half_pi<float>(), 1.0f, 0.01f, 10.0f);
  vec3 origin = vec3(0);
  vec3 posx = vec3(1.0f, 0.0f, 0.0f);
  vec3 negx = vec3(-1.0f, 0.0f, 0.0f);
  vec3 posy = vec3(0.0f, 1.0f, 0.0f);
  vec3 negy = vec3(0.0f, -1.0f, 0.0f);
  vec3 posz = vec3(0.0f, 0.0f, 1.0f);
  vec3 negz = vec3(0.0f, 0.0f, -1.0f);
  mat4 cameras[] = {lookAt(origin, posx, negy), lookAt(origin, negx, negy), lookAt(origin, posy, posz),
      lookAt(origin, negy, negz), lookAt(origin, posz, negy), lookAt(origin, negz, negy)};

  // replace cached texture with a new one that has the filtered mipmaps
  Cubemap source = radiance;
  radiance.handle = make_shared<Texture_Handle>();
  TEXTURECUBEMAP_CACHE[source.handle->filename] = radiance.handle;
  radiance.handle->is_cubemap = true;
  radiance.handle->filename = source.handle->filename;
  glGenTextures(1, &radiance.handle->texture);
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_CUBE_MAP, radiance.handle->texture);
  ASSERT(radiance.size != ivec2(0));
  for (uint32 i = 0; i < 6; ++i)
  {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, radiance.size.x, radiance.size.y, 0, GL_RGBA,
        GL_FLOAT, nullptr);
  }
  radiance.handle->size = radiance.size;
  ASSERT(radiance.handle->size != ivec2(0));
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  radiance.handle->magnification_filter = GL_LINEAR;
  radiance.handle->minification_filter = GL_LINEAR_MIPMAP_LINEAR;
  radiance.handle->wrap_s = GL_CLAMP_TO_EDGE;
  radiance.handle->wrap_t = GL_CLAMP_TO_EDGE;
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, mip_levels);
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

  // framebuffer for rendering filtering result to new cubemap mipmaps
  GLuint fbo;
  GLuint rbo;
  glGenFramebuffers(1, &fbo);
  glGenRenderbuffers(1, &rbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, source.size.x, source.size.y);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

  specular_filter.use();
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_CUBE_MAP, source.handle->texture);

  float angle = radians(0.f);
  mat4 rot = toMat4(quat(1, 0, 0, angle));
  specular_filter.set_uniform("projection", projection);
  specular_filter.set_uniform("rotation", rot);

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_FRONT);
  for (uint32 mip_level = 0; mip_level < mip_levels; ++mip_level)
  {
    uint32 width = (uint32)floor(radiance.size.x * pow(0.5, mip_level));
    uint32 height = (uint32)floor(radiance.size.y * pow(0.5, mip_level));
    glViewport(0, 0, width, height);
    float roughness = (float)mip_level / (float)(mip_levels - 1);
    specular_filter.set_uniform("roughness", roughness);
    for (unsigned int i = 0; i < 6; ++i)
    {
      specular_filter.set_uniform("camera", cameras[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
          radiance.handle->texture, mip_level);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      cube.draw();
    }
  }
  glCullFace(GL_BACK);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glDeleteRenderbuffers(1, &rbo);
  glDeleteFramebuffers(1, &fbo);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  glActiveTexture(GL_TEXTURE0);
  radiance.handle->ibl_mipmaps_generated = true;
}

Environment_Map::Environment_Map(std::string environment, std::string irradiance, bool equirectangular)
{
  Environment_Map_Descriptor d(environment, irradiance, equirectangular);
  m = d;
  m.radiance = m.radiance;
  m.irradiance = m.irradiance;
  load();
}
Environment_Map::Environment_Map(Environment_Map_Descriptor d)
{
  m = d;
  m.radiance = m.radiance;
  m.irradiance = m.irradiance;
  load();
}
void Environment_Map::load()
{
  check_gl_error();
  if (m.source_is_equirectangular)
  {
    radiance = Cubemap(m.radiance);
    irradiance = Cubemap(m.irradiance);
  }
  else
  {
    radiance = Cubemap(m.environment_faces);
    irradiance = Cubemap(m.irradiance_faces);
  }
  bool irradiance_exists = irradiance.handle.get();
  if (!irradiance_exists)
  {
    irradiance_convolution();
  }
  if (!radiance.handle->ibl_mipmaps_generated)
    generate_ibl_mipmaps();
}

void Environment_Map::bind(GLuint radiance_texture_unit, GLuint irradiance_texture_unit)
{
  bool irradiance_exists = irradiance.handle.get();
  if (!irradiance_exists)
  {
    irradiance_convolution();
  }

  irradiance_exists = irradiance.handle.get();
  ASSERT(irradiance_exists);

  radiance.bind(radiance_texture_unit);
  irradiance.bind(irradiance_texture_unit);
}

void Environment_Map::irradiance_convolution()
{
  ASSERT(0);
}

Environment_Map_Descriptor::Environment_Map_Descriptor(
    std::string environment, std::string irradiance, bool equirectangular)
    : radiance(environment), irradiance(irradiance), source_is_equirectangular(equirectangular)
{
}

void Light_Array::bind(Shader &shader)
{
  for (uint32 i = 0; i < light_count; ++i)
  {
    Light_Uniform_Value_Cache *value_cache = &shader.program->light_values_cache[i];
    const Light_Uniform_Location_Cache *location_cache = &shader.program->light_locations_cache[i];
    const Light *new_light = &lights[i];

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
    vec3 new_flux = new_light->color * new_light->brightness;
    if (value_cache->flux != new_flux)
    {
      value_cache->flux = new_flux;
      glUniform3fv(location_cache->flux, 1, &new_flux[0]);
    }

    if (value_cache->attenuation != new_light->attenuation)
    {
      value_cache->attenuation = new_light->attenuation;
      glUniform3fv(location_cache->attenuation, 1, &new_light->attenuation[0]);
    }

    const vec3 new_ambient = new_light->brightness * new_light->ambient * new_light->color;
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
  const uint32 new_light_count = light_count;
  if (shader.program->light_count != new_light_count)
  {
    shader.program->light_count = (int32)light_count;
    glUniform1i(shader.program->light_count_location, new_light_count);
  }
}

void Material_Descriptor::mod_by(const Material_Descriptor *override)
{
  const static Material_Descriptor defaults;
  { // only apply the override for the material descriptor slots
    // that are different than default
    if (override->albedo.name != defaults.albedo.name)
      albedo = override->albedo;
    if (override->roughness.name != defaults.roughness.name)
      roughness = override->roughness;
    if (override->metalness.name != defaults.metalness.name)
      metalness = override->metalness;
    if (override->tangent.name != defaults.tangent.name)
      tangent = override->tangent;
    if (override->normal.name != defaults.normal.name)
      normal = override->normal;
    if (override->ambient_occlusion.name != defaults.ambient_occlusion.name)
      ambient_occlusion = override->ambient_occlusion;
    if (override->emissive.name != defaults.emissive.name)
      emissive = override->emissive;

    albedo.mod = override->albedo.mod;
    roughness.mod = override->roughness.mod;
    metalness.mod = override->metalness.mod;
    tangent.mod = override->tangent.mod;
    emissive.mod = override->emissive.mod;
    ambient_occlusion.mod = override->ambient_occlusion.mod;
    vertex_shader = override->vertex_shader;
    frag_shader = override->frag_shader;
    backface_culling = override->backface_culling;
    uv_scale = override->uv_scale;
    uses_transparency = override->uses_transparency;
    casts_shadows = override->casts_shadows;
    albedo_alpha_override = override->albedo_alpha_override;
    discard_on_alpha = override->discard_on_alpha;
  }
}

Particle_Emitter::Particle_Emitter(Particle_Emitter_Descriptor d, Mesh_Index m, Material_Index mat)
    : descriptor(d), mesh_index(m), material_index(mat)
{
  shared_data = std::make_unique<Physics_Shared_Data>();
  shared_data->descriptor = descriptor;
  t = std::thread(thread, shared_data);
  t.detach();
  thread_launched = true;
}

Particle_Emitter::Particle_Emitter()
{
  shared_data = std::make_unique<Physics_Shared_Data>();
  t = std::thread(thread, shared_data);
  t.detach();
  thread_launched = true;
}

Particle_Emitter::Particle_Emitter(const Particle_Emitter &rhs)
    : descriptor(rhs.descriptor), mesh_index(rhs.mesh_index), material_index(rhs.material_index)
{
  shared_data = std::make_unique<Physics_Shared_Data>();
  construct_physics_method(descriptor);
  construct_emission_method(descriptor);
  shared_data->descriptor = descriptor;

  rhs.spin_until_up_to_date();
  shared_data->particles = rhs.shared_data->particles;

  t = std::thread(thread, shared_data);
  t.detach();
  thread_launched = true;
}

Particle_Emitter::Particle_Emitter(Particle_Emitter &&rhs)
{
  ASSERT(rhs.shared_data->request_thread_exit == false);
  rhs.spin_until_up_to_date();
  t = std::move(rhs.t);
  thread_launched = rhs.thread_launched;
  shared_data = std::move(rhs.shared_data);
  physics_method = std::move(rhs.physics_method);
  emission_method = std::move(rhs.emission_method);
  mesh_index = rhs.mesh_index;
  material_index = rhs.material_index;

  if (rhs.shared_data)
    shared_data->particles = std::move(rhs.shared_data->particles);
  descriptor = rhs.descriptor;
}

void Particle_Emitter::update(mat4 projection, mat4 camera, float32 dt)
{
  // todo: geometry collision interface
  // todo: option to add parent node velocity or not
  // todo: option to snap all particles to basis space or world space

  // locally calculate basis vector, or, change scene graph to store its last-calculated world basis, and read from
  // that here  take lock  modify the descriptor to change position, orientation, velocity  release lock
  ASSERT(shared_data);
  ASSERT(thread_launched);
  spin_until_up_to_date();
  shared_data->descriptor = descriptor;
  shared_data->projection = projection;
  shared_data->camera = camera;
  shared_data->requested_tick += 1;
}

void Particle_Emitter::spin_until_up_to_date() const
{
  ASSERT(shared_data);
  if (shared_data->requested_tick != 0)
  {
    while (shared_data->requested_tick != shared_data->completed_update)
    {
      // spin
    }
  }
  return;
}

std::unique_ptr<Particle_Physics_Method> Particle_Emitter::construct_physics_method(Particle_Emitter_Descriptor d)
{
  if (d.physics_descriptor.type == simple)
  {
    return std::make_unique<Simple_Particle_Physics>();
  }
  else if (d.physics_descriptor.type == wind)
  {
    return std::make_unique<Wind_Particle_Physics>();
  }
}

std::unique_ptr<Particle_Emission_Method> Particle_Emitter::construct_emission_method(Particle_Emitter_Descriptor d)
{
  if (d.emission_descriptor.type == stream)
  {
    return std::make_unique<Particle_Stream_Emission>();
  }
  else if (d.emission_descriptor.type == explosion)
  {
    return std::make_unique<Particle_Explosion_Emission>();
  }
}

void Particle_Emitter::thread(std::shared_ptr<Physics_Shared_Data> shared_data)
{
  ASSERT(shared_data);
  std::unique_ptr<Particle_Emission_Method> emission = construct_emission_method(shared_data->descriptor);
  std::unique_ptr<Particle_Physics_Method> physics = construct_physics_method(shared_data->descriptor);
  Particle_Emission_Type emission_type = shared_data->descriptor.emission_descriptor.type;
  Particle_Physics_Type physics_type = shared_data->descriptor.physics_descriptor.type;
  while (!shared_data->request_thread_exit)
  {
    if (shared_data->requested_tick == shared_data->completed_update)
    {
      SDL_Delay(1);
      continue;
    }

    if (emission_type != shared_data->descriptor.emission_descriptor.type)
      emission = construct_emission_method(shared_data->descriptor);
    if (physics_type != shared_data->descriptor.physics_descriptor.type)
      physics = construct_physics_method(shared_data->descriptor);

    const float32 time = shared_data->completed_update * dt;
    vec3 pos = shared_data->descriptor.position;
    vec3 vel = shared_data->descriptor.velocity;
    quat o = shared_data->descriptor.orientation;
    emission->update(&shared_data->particles, &shared_data->descriptor.emission_descriptor, pos, vel, o, time, dt);
    physics->step(&shared_data->particles, &shared_data->descriptor.physics_descriptor, time, dt);
    // delete expired particles:

    for (uint i = 0; i < shared_data->particles.particles.size(); ++i)
    {
      shared_data->particles.particles[i].time_left_to_live -= dt;
      if (shared_data->particles.particles[i].time_left_to_live <= 0.0f)
      {
        shared_data->particles.particles[i] = shared_data->particles.particles.back();
        shared_data->particles.particles.pop_back();
        --i;
      }
    }
    shared_data->particles.compute_attributes(shared_data->projection, shared_data->camera);
    shared_data->completed_update += 1;
  }
  // possible problem: if requested_update doesnt match completed_update when the thread exits
  // does this cause an issue anywhere?
}

// interface to retrieve current state for rendering

bool Particle_Emitter::prepare_instance(std::vector<Render_Instance> *accumulator)
{
  ASSERT(shared_data);
  ASSERT(accumulator);
  spin_until_up_to_date();
  if (mesh_index.i != NODE_NULL && material_index.i != NODE_NULL)
  {
    return shared_data->particles.prepare_instance(accumulator);
  }
  return false;
}

void Particle_Emitter::clear()
{
  ASSERT(shared_data);
  spin_until_up_to_date();
  shared_data->particles.particles.clear();
}

Particle_Emitter &Particle_Emitter::operator=(const Particle_Emitter &rhs)
{
  rhs.spin_until_up_to_date();
  spin_until_up_to_date();
  descriptor = rhs.descriptor;
  construct_physics_method(descriptor);
  construct_emission_method(descriptor);
  shared_data->descriptor = descriptor;
  mesh_index = rhs.mesh_index;
  material_index = rhs.material_index;
  return *this;
}

Particle_Emitter &Particle_Emitter::operator=(Particle_Emitter &&rhs)
{
  ASSERT(rhs.shared_data->request_thread_exit == false);
  ASSERT(shared_data->request_thread_exit == false);
  rhs.spin_until_up_to_date();
  spin_until_up_to_date();
  shared_data->request_thread_exit = true;
  t.join();
  t = std::move(rhs.t);
  shared_data = std::move(rhs.shared_data);
  descriptor = rhs.descriptor;
  mesh_index = rhs.mesh_index;
  material_index = rhs.material_index;
  return *this;
}

void Simple_Particle_Physics::step(
    Particle_Array *p, const Particle_Physics_Method_Descriptor *d, float32 time, float32 dt)
{
  ASSERT(p);
  ASSERT(d);
  for (auto &particle : p->particles)
  {
    particle.velocity += dt * d->gravity;
    particle.position += dt * particle.velocity;
  }
}

void Wind_Particle_Physics::step(Particle_Array *p, const Particle_Physics_Method_Descriptor *d, float32 t, float32 dt)
{
  ASSERT(p);
  ASSERT(d);
  for (auto &particle : p->particles)
  {
    vec3 wind = d->intensity * d->direction;

    particle.velocity += dt * (d->gravity + wind);
    particle.position += dt * particle.velocity;
  }
}

void Particle_Explosion_Emission::update(Particle_Array *p, const Particle_Emission_Method_Descriptor *d, vec3 pos,
    vec3 vel, quat o, float32 time, float32 dt)
{
  ASSERT(p);
  ASSERT(d);
  // todo: particle explosion
  Particle new_particle;
  p->particles.push_back(new_particle);
}

void Particle_Stream_Emission::update(Particle_Array *p, const Particle_Emission_Method_Descriptor *d, vec3 pos,
    vec3 vel, quat o, float32 time, float32 dt)
{
  ASSERT(p);
  ASSERT(d);
  const uint32 particles_before_tick = (uint32)floor(d->particles_per_second * time);
  const uint32 particles_after_tick = (uint32)floor(d->particles_per_second * (time + dt));
  const uint32 spawns = particles_after_tick - particles_before_tick;

  for (uint32 i = 0; i < spawns; ++i)
  {

    if (!(p->particles.size() < MAX_INSTANCE_COUNT))
    {
      return;
    }
    Particle new_particle;

    vec3 pos_variance = random_within(d->initial_position_variance);
    pos_variance = pos_variance - 0.5f * d->initial_position_variance;
    new_particle.position = pos + pos_variance;

    vec3 vel_variance = random_within(d->initial_velocity_variance);
    vel_variance = vel_variance - 0.5f * d->initial_velocity_variance;
    if (d->inherit_velocity)
      new_particle.velocity = o * (vel + d->initial_velocity + vel_variance);
    else
      new_particle.velocity = o * (d->initial_velocity + vel_variance);

    vec3 av_variance = random_within(d->initial_angular_velocity_variance);
    av_variance = av_variance - 0.5f * d->initial_angular_velocity_variance;
    new_particle.angular_velocity = d->initial_angular_velocity + av_variance;

    //// first generated orientation - use to orient within a cone or an entire unit sphere
    //// these are args to random_3D_unit_vector: azimuth_min, azimuth_max, altitude_min, altitude_max
    // glm::vec4 randomized_orientation_axis = vec4(0.f, two_pi<float32>(), -1.f, 1.f);
    // float32 randomized_orientation_angle_variance = 0.f;

    //// post-spawn - use to orient the model relative to the emitter:
    // glm::vec3 intitial_orientation_axis = glm::vec3(0, 0, 1);1
    // float32 initial_orientation_angle = 0.0f;

    const vec4 &ov = d->randomized_orientation_axis;
    vec3 orientation_vector = random_3D_unit_vector(ov.x, ov.y, ov.z, ov.w);
    float32 oav = random_between(0.f, d->randomized_orientation_angle_variance);
    oav = oav - 0.5f * d->randomized_orientation_angle_variance;
    quat first_orientation = angleAxis(oav, orientation_vector);
    quat second_orientation = angleAxis(d->initial_orientation_angle, d->intitial_orientation_axis);
    new_particle.orientation = o * second_orientation * first_orientation;

    vec3 scale_variance = random_within(d->initial_scale_variance);
    scale_variance = scale_variance - 0.5f * d->initial_scale_variance;
    new_particle.scale = d->initial_scale + scale_variance;

    float32 time_to_live_varaince = random_between(0.f, d->time_to_live_variance);
    time_to_live_varaince = time_to_live_varaince - 0.5f * d->time_to_live_variance;
    new_particle.time_to_live = d->time_to_live + time_to_live_varaince;
    new_particle.time_left_to_live = new_particle.time_to_live;
    p->particles.push_back(new_particle);
  }
}

void Particle_Array::init()
{
  if (initialized)
    return;

  glGenBuffers(1, &instance_mvp_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_mvp_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(mat4), (void *)0, GL_DYNAMIC_DRAW);
  glGenBuffers(1, &instance_model_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_model_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(mat4), (void *)0, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &instance_attribute0_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute0_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(vec4), (void *)0, GL_DYNAMIC_DRAW);
  glGenBuffers(1, &instance_attribute1_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute1_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(vec4), (void *)0, GL_DYNAMIC_DRAW);
  glGenBuffers(1, &instance_attribute2_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute2_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(vec4), (void *)0, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  initialized = true;
}

void Particle_Array::destroy()
{
  glDeleteBuffers(1, &instance_mvp_buffer);
  glDeleteBuffers(1, &instance_model_buffer);
  glDeleteBuffers(1, &instance_attribute0_buffer);
  glDeleteBuffers(1, &instance_attribute1_buffer);
  glDeleteBuffers(1, &instance_attribute2_buffer);
  initialized = false;
}

Particle_Array::Particle_Array(Particle_Array &&rhs) {}

void Particle_Array::compute_attributes(mat4 projection, mat4 camera)
{
  MVP_Matrices.clear();
  Model_Matrices.clear();
  attributes0.clear();
  attributes1.clear();
  attributes2.clear();
  // set_message("compute_attributes projection:", s(projection), 1.0f);
  // set_message("compute_attributes camera:", s(camera), 1.0f);
  for (auto &i : particles)
  {
    const mat4 R = toMat4(i.orientation);
    const mat4 S = scale(i.scale);
    const mat4 T = translate(i.position);
    const mat4 model = T * R * S;
    const mat4 MVP = projection * camera * model;
    MVP_Matrices.push_back(MVP);
    Model_Matrices.push_back(model);
    attributes0.push_back(i.attribute0);
    attributes1.push_back(i.attribute1);
    attributes2.push_back(i.attribute2);
  }
}

bool Particle_Array::prepare_instance(std::vector<Render_Instance> *accumulator)
{
  if (!initialized)
  {
    set_message("Uninitialized particle array", "", 1.0f);
    return false;
  }

  Render_Instance result;
  uint32 num_instances = MVP_Matrices.size();
  if (num_instances == 0)
    return false;

  ASSERT(num_instances <= MAX_INSTANCE_COUNT);
  set_message("Particle count:", s(num_instances), 1.0f);

  glBindBuffer(GL_ARRAY_BUFFER, instance_mvp_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(mat4), &MVP_Matrices[0][0][0]);
  glBindBuffer(GL_ARRAY_BUFFER, instance_model_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(mat4), &Model_Matrices[0][0][0]);

  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute0_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(vec4), &attributes0[0]);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute1_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(vec4), &attributes1[0]);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute2_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(vec4), &attributes2[0]);

  result.mvp_buffer = instance_mvp_buffer;
  result.model_buffer = instance_model_buffer;
  result.attribute0_buffer = instance_attribute0_buffer;
  result.attribute1_buffer = instance_attribute1_buffer;
  result.attribute2_buffer = instance_attribute2_buffer;
  result.size = num_instances;
  accumulator->push_back(result);
  return true;
}
Particle_Array &Particle_Array::operator=(Particle_Array &&rhs)
{
  particles = std::move(rhs.particles);
  return *this;
}
Particle_Array &Particle_Array::operator=(Particle_Array &rhs)
{
  // todo: particle_array copy
  return *this;
}