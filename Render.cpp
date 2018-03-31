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
#include "SDL_Imgui_State.h"
#include "Shader.h"
#include "Third_party/imgui/imgui.h"
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

#define DIFFUSE_TARGET GL_COLOR_ATTACHMENT0
#define DEPTH_TARGET GL_COLOR_ATTACHMENT1
#define NORMAL_TARGET GL_COLOR_ATTACHMENT2
#define POSITION_TARGET GL_COLOR_ATTACHMENT3

std::unordered_map<std::string, std::weak_ptr<Mesh_Handle>> MESH_CACHE;

static std::unordered_map<std::string, std::weak_ptr<Texture_Handle>>
    TEXTURE2D_CACHE;

static std::unordered_map<std::string, std::weak_ptr<Texture_Handle>>
    TEXTURECUBEMAP_CACHE;

void check_FBO_status();

Framebuffer_Handle::~Framebuffer_Handle() { glDeleteFramebuffers(1, &fbo); }
Framebuffer::Framebuffer() {}
void Framebuffer::init()
{
  if (!fbo)
  {
    fbo = make_shared<Framebuffer_Handle>();
    glGenFramebuffers(1, &fbo->fbo);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);

  for (uint32 i = 0; i < color_attachments.size(); ++i)
  {
    Texture_Descriptor test;
    ASSERT(color_attachments[i].t.name !=
           test.name); // texture must be named, or .load assumes null
    color_attachments[i].load();
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
        color_attachments[i].get_handle(), 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
  }
  check_FBO_status();
  depth = make_shared<Renderbuffer_Handle>();
  if (depth_enabled)
  {
    glGenRenderbuffers(1, &depth->rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depth->rbo);

    glRenderbufferStorage(
        GL_RENDERBUFFER, depth_format, depth_size.x, depth_size.y);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth->rbo);
    depth->format = depth_format;
    depth->size = depth_size;
  }
  check_FBO_status();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void Framebuffer::bind()
{
  ASSERT(fbo);
  ASSERT(fbo->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
  check_FBO_status();
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
    gaussian_blur_shader = Shader("passthrough.vert", "gaussian_blur.frag");

  intermediate_fbo.color_attachments = {Texture(td)};
  intermediate_fbo.init();

  target.color_attachments = {Texture(td)};
  target.init();

  aspect_ratio_factor = (float32)td.size.y / (float32)td.size.x;

  initialized = true;
}
void Gaussian_Blur::draw(
    Renderer *renderer, Texture *src, float32 radius, uint32 iterations)
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
  gaussian_blur_shader.set_uniform(
      "transform", Renderer::ortho_projection(*dst_size));
  src->bind(0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->quad.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(),
      GL_UNSIGNED_INT, (void *)0);

  gaus_scale.y = radius / dst_size->x;
  gaus_scale.x = 0;
  gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo->fbo);

  intermediate_fbo.color_attachments[0].bind(0);

  glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(),
      GL_UNSIGNED_INT, (void *)0);

  iterations -= 1;

  for (uint i = 0; i < iterations; ++i)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, intermediate_fbo.fbo->fbo);
    vec2 gaus_scale = vec2(aspect_ratio_factor * radius / dst_size->x, 0.0);
    gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
    target.color_attachments[0].bind(0);
    glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(),
        GL_UNSIGNED_INT, (void *)0);
    gaus_scale.y = radius / dst_size->x;
    gaus_scale.x = 0;
    gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo->fbo);
    intermediate_fbo.color_attachments[0].bind(0);
    glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(),
        GL_UNSIGNED_INT, (void *)0);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glEnable(GL_DEPTH_TEST);
}

High_Pass_Filter::High_Pass_Filter() {}

void High_Pass_Filter::init(ivec2 size, GLenum format)
{
  if (!initialized)
    high_pass_shader = Shader("passthrough.vert", "high_pass_filter.frag");

  target.color_attachments = {Texture(name, size, format)};
  target.init();

  initialized = true;
}

void High_Pass_Filter::draw(Renderer *renderer, Texture *src)
{
  ASSERT(initialized);
  ASSERT(renderer);
  ASSERT(src);
  ASSERT(src->get_handle());
  ASSERT(target.fbo);
  ASSERT(target.color_attachments.size() > 0);

  ivec2 *dst_size = &target.color_attachments[0].t.size;

  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo->fbo);
  glViewport(0, 0, dst_size->x, dst_size->y);
  glDisable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindVertexArray(renderer->quad.get_vao());
  high_pass_shader.use();
  high_pass_shader.set_uniform(
      "transform", Renderer::ortho_projection(*dst_size));
  src->bind(0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->quad.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(),
      GL_UNSIGNED_INT, (void *)0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glEnable(GL_DEPTH_TEST);
}

Bloom_Shader::Bloom_Shader() {}
void Bloom_Shader::init(ivec2 size, GLenum format)
{
  Texture_Descriptor td;
  td.name = name + s(" Bloom Shader's blur texture");
  td.format = format;
  td.size = size;
  td.wrap_s = GL_CLAMP_TO_EDGE;
  td.wrap_t = GL_CLAMP_TO_EDGE;

  high_pass.init(size, format);

  blur.init(td);

  target.color_attachments = {
      Texture(name + s(" Bloom Shader's target texture"), size, format)};
  target.init();

  initialized = true;
}
void Bloom_Shader::draw(Renderer *renderer, Texture *src, Framebuffer *dst)
{
  ASSERT(initialized);
  ASSERT(renderer);

  // high pass draws src onto its own target with the result
  high_pass.draw(renderer, src);

  // blur takes the high pass as src, draws to its own target with the result
  blur.draw(
      renderer, &high_pass.target.color_attachments[0], radius, iterations);

  // this bloom_shader::draw takes blur result as src, and draws it onto dst

  // FINAL_OUTPUT_TEXTURE = &blur.target.color_attachments[0].texture->texture;
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  ASSERT(dst);
  ASSERT(dst->fbo);
  ASSERT(dst->color_attachments.size() > 0);

  dst->bind();
  blur.target.color_attachments[0].bind(0);
  ivec2 viewport_size = dst->color_attachments[0].t.size;
  glViewport(0, 0, viewport_size.x, viewport_size.y);
  glDisable(GL_DEPTH_TEST);
  glBindVertexArray(renderer->quad.get_vao());
  renderer->passthrough.use();
  renderer->passthrough.set_uniform(
      "transform", renderer->ortho_projection(viewport_size));
  renderer->passthrough.set_uniform("time", (float32)get_real_time());
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->quad.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(),
      GL_UNSIGNED_INT, (void *)0);

  glBindTexture(GL_TEXTURE_2D, 0);

  glDisable(GL_BLEND);
}

Renderer::~Renderer()
{
  glDeleteBuffers(1, &instance_mvp_buffer);
  glDeleteBuffers(1, &instance_model_buffer);
}

void check_and_clear_expired_textures()
{
  auto it = TEXTURE2D_CACHE.begin();
  while (it != TEXTURE2D_CACHE.end())
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

  SDL_Surface *surface = SDL_CreateRGBSurface(
      0, width, height, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
  if (surface)
  {
    std::string name = s("screen_", i, ".png");
    set_message("Saving Screenshot: ", " With name: " + name);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(
        0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
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

Texture::Texture(Texture_Descriptor &td)
{
  t = td;
  load();
}

Texture::Texture(std::string name, glm::ivec2 size, GLenum format,
    GLenum minification_filter, GLenum magnification_filter, GLenum wrap_s,
    GLenum wrap_t, glm::vec4 border_color)
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
  set_message("Deleting texture: ", s(texture));
  glDeleteTextures(1, &texture);
  texture = 0;
}
uint32 Texture_Handle::get_imgui_handle()
{
  if (format == GL_SRGB8_ALPHA8 || format == GL_SRGB || format == GL_RGBA16F ||
      format == GL_RGBA32F || format == GL_RG16F || format == GL_RG32F)
  {
    return ((uint32)texture | 0xf0000000);
  }
  return (uint32)texture;
}
Texture::Texture(std::string path, bool premul)
{
  t.name = path; // note this will possibly be modified within .load()
  t.cache_as_unique = false;
  t.process_premultiply = premul;
  load();
}

void Texture::load()
{
  if (initialized && !texture)
  { // file doesnt exist
#ifndef DYNAMIC_TEXTURE_RELOADING
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
    bool requires_reallocation = !((texture) && (texture->size == t.size) &&
                                   (texture->format == t.format));
    if (!requires_reallocation)
      return;

    if (!texture)
    {
      static uint32 i = 0;
      ++i;
      std::string unique_key = s("Generated texture ID:", i, " Name:", t.name);
      TEXTURE2D_CACHE[unique_key] = texture = make_shared<Texture_Handle>();
      glGenTextures(1, &texture->texture);
      texture->filename = t.name;
    }

    glBindTexture(GL_TEXTURE_2D, texture->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, t.format, t.size.x, t.size.y, 0, GL_RGBA,
        GL_FLOAT, 0);
    texture->size = t.size;
    texture->format = t.format;
    initialized = true;
    check_set_parameters();
    return;
  }

  if (!initialized && !is_a_color)
  {
    t.name = fix_filename(t.name);

    if (t.name.find_last_of("/") == t.name.npos)
    { // no directory included in name, so use base path
      t.name = BASE_TEXTURE_PATH + t.name; // also color( hits this
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

  texture = std::make_shared<Texture_Handle>();
  TEXTURE2D_CACHE[t.name] = texture;
  int32 width, height, n;

  // todo: other software procedural texture generators here?
  if (t.name.substr(0, 6) == "color(")
  {
    t.size = ivec2(1, 1);
    t.format = GL_RGBA16F; // could detect and support other formats in here
    t.magnification_filter = GL_NEAREST;
    t.minification_filter = GL_NEAREST;

    vec4 color = string_to_float4_color(t.name);
    glGenTextures(1, &texture->texture);
    glBindTexture(GL_TEXTURE_2D, texture->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, t.format, t.size.x, t.size.y, 0, GL_RGBA,
        GL_FLOAT, &color);

    glTexParameterfv(
        GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &t.border_color[0]);
    glBindTexture(GL_TEXTURE_2D, 0);

    texture->size = t.size;
    texture->filename = t.name;
    texture->format = t.format;
    texture->border_color = t.border_color;
    initialized = true;
    return;
  }

  auto *data = stbi_load(t.name.c_str(), &width, &height, &n, 4);
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
      initialized = true;
      return;
    }
#endif
  }

  set_message("Texture load cache miss. Texture from disk: ", t.name, 1.0);
  struct stat attr;
  stat(t.name.c_str(), &attr);
  texture.get()->file_mod_t = attr.st_mtime;

  if (t.process_premultiply)
  {
    for (int32 i = 0; i < width * height; ++i)
    {
      uint32 pixel = data[i];
      uint8 r = (uint8)(0x000000FF & pixel);
      uint8 g = (uint8)((0x0000FF00 & pixel) >> 8);
      uint8 b = (uint8)((0x00FF0000 & pixel) >> 16);
      uint8 a = (uint8)((0xFF000000 & pixel) >> 24);

      r = (uint8)round(r * ((float)a / 255));
      g = (uint8)round(g * ((float)a / 255));
      b = (uint8)round(b * ((float)a / 255));

      data[i] = (24 << a) | (16 << b) | (8 << g) | r;
    }
    t.format = GL_RGBA; // right?
  }

  t.size = ivec2(width, height);
  glGenTextures(1, &texture->texture);
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, t.format, t.size.x, t.size.y, 0, GL_RGBA,
      GL_UNSIGNED_BYTE, data);
  glTexParameterf(
      GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, MAX_ANISOTROPY);

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
  t.minification_filter = GL_LINEAR;
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
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t.magnification_filter);
  }
  if (t.minification_filter != texture->minification_filter)
  {
    texture->minification_filter = t.minification_filter;
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t.minification_filter);
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
    glTexParameterfv(
        GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &t.border_color[0]);
  }
  if (t.anisotropic_filtering != texture->anisotropic_filtering)
  {
    texture->anisotropic_filtering = t.anisotropic_filtering;
    glTexParameterf(
        GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, MAX_ANISOTROPY);
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

GLuint Texture::get_imgui_handle() { return texture->get_imgui_handle(); }

bool Texture::has_img_file_extension(std::string name)
{
  uint32 size = name.size();

  if (size < 3)
    return false;

  std::string end = name.substr(size - 4, 4);
  if (end == ".jpg" || end == ".png")
  {
    return true;
  }
  return false;
}

Mesh::Mesh() {}
Mesh::Mesh(Mesh_Primitive p, std::string mesh_name) : name(mesh_name)
{
  unique_identifier = to_string(p);
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
    // todo: Mesh JSON: but we could hash it for a uid and cache it
    // to support json restore
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

std::shared_ptr<Mesh_Handle> Mesh::upload_data(const Mesh_Data &mesh_data)
{
  std::shared_ptr<Mesh_Handle> mesh = std::make_shared<Mesh_Handle>();
  mesh->data = mesh_data;

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
  glBufferData(
      GL_ARRAY_BUFFER, buffer_size, &mesh_data.positions[0], GL_STATIC_DRAW);

  // normals
  buffer_size = mesh_data.normals.size() *
                sizeof(decltype(mesh_data.normals)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->normal_buffer);
  glBufferData(
      GL_ARRAY_BUFFER, buffer_size, &mesh_data.normals[0], GL_STATIC_DRAW);

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
  glBufferData(
      GL_ARRAY_BUFFER, buffer_size, &mesh_data.tangents[0], GL_STATIC_DRAW);

  // bitangents
  buffer_size = mesh_data.bitangents.size() *
                sizeof(decltype(mesh_data.bitangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->bitangents_buffer);
  glBufferData(
      GL_ARRAY_BUFFER, buffer_size, &mesh_data.bitangents[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  return mesh;
}

Material::Material() {}
Material::Material(Material_Descriptor &m)
{
  ASSERT(m.normal.name != "");
  load(m);
}
Material::Material(aiMaterial *ai_material, std::string working_directory,
    Material_Descriptor *material_override)
{
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
    m.albedo.name = working_directory + m.albedo.name;
  if (specular_n)
    m.specular.name = working_directory + m.specular.name;
  if (emissive_n)
    m.emissive.name = working_directory + m.emissive.name;
  if (normal_n)
    m.normal.name = working_directory + m.normal.name;
  if (roughness_n)
    m.roughness.name = working_directory + m.roughness.name;

  if (material_override)
  {
    Material_Descriptor defaults;
    if (material_override->albedo.name != defaults.albedo.name)
      m.albedo = material_override->albedo;
    if (material_override->roughness.name != defaults.roughness.name)
      m.roughness = material_override->roughness;
    if (material_override->specular.name != defaults.specular.name)
      m.specular = material_override->specular;
    if (material_override->metalness.name != defaults.metalness.name)
      m.metalness = material_override->metalness;
    if (material_override->tangent.name != defaults.tangent.name)
      m.tangent = material_override->tangent;
    if (material_override->normal.name != defaults.normal.name)
      m.normal = material_override->normal;
    if (material_override->ambient_occlusion.name !=
        defaults.ambient_occlusion.name)
      m.ambient_occlusion = material_override->ambient_occlusion;
    if (material_override->emissive.name != defaults.emissive.name)
      m.emissive = material_override->emissive;

    m.vertex_shader = material_override->vertex_shader;
    m.frag_shader = material_override->frag_shader;
    m.backface_culling = material_override->backface_culling;
    m.uv_scale = material_override->uv_scale;
    m.uses_transparency = material_override->uses_transparency;
    m.casts_shadows = material_override->casts_shadows;
    m.environment = material_override->environment;
    m.albedo_alpha_override = material_override->albedo_alpha_override;
  }

  if (m.normal.name == "")
    m.normal = "color(0.5,0.5,1.0)";
  load(m);
}
void Material::load(Material_Descriptor &mat)
{
  // the formats set here may be overriden in the texture constructor
  // based on if its a generated color or not

  m = mat;

  m.albedo.format = GL_SRGB8_ALPHA8;
  albedo = Texture(m.albedo);

  // specular_color = Texture(m.specular);
  m.normal.format = GL_RGBA;
  normal = Texture(m.normal);

  m.emissive.format = GL_SRGB8_ALPHA8;
  emissive = Texture(m.emissive);

  m.roughness.format = GL_SRGB8_ALPHA8;
  roughness = Texture(m.roughness);

  m.environment.format = GL_SRGB8;
  environment = Cubemap(m.environment);

  shader = Shader(m.vertex_shader, m.frag_shader);
}
void Material::bind(Shader *shader)
{
  // set_message("Material::bind()");
  if (m.backface_culling)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  shader->set_uniform("discard_on_alpha", m.discard_on_alpha);

  // set_message("Material::bind(): binding albedo:");
  albedo.bind(Texture_Location::albedo);
  shader->set_uniform("texture0_mod", m.albedo.mod);

  // set_message("Material::bind(): binding normal:");
  normal.bind(Texture_Location::normal);

  // set_message("Material::bind(): binding emissive:");
  emissive.bind(Texture_Location::emissive);
  shader->set_uniform("texture3_mod", m.emissive.mod);

  // set_message("Material::bind(): binding roughness:");
  roughness.bind(Texture_Location::roughness);
  shader->set_uniform("texture4_mod", m.roughness.mod);

  environment.bind(Texture_Location::environment);
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
  glActiveTexture(GL_TEXTURE0 + Texture_Location::environment);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
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
  bool b8 = brightness == rhs.brightness;
  return b1 & b2 & b3 & b4 & b5 & b6 & b7 & b8;
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
Render_Entity::Render_Entity(
    Mesh *mesh, Material *material, mat4 world_to_model)
    : mesh(mesh), material(material), transformation(world_to_model)
{
  ASSERT(mesh);
  ASSERT(material);
}

Renderer::Renderer(SDL_Window *window, ivec2 window_size,std::string name)
{
  set_message("Renderer constructor");
  this->name = name;
  this->window = window;
  this->window_size = window_size;
  SDL_DisplayMode current;
  SDL_GetCurrentDisplayMode(0, &current);
  target_frame_time = 1.0f / (float32)current.refresh_rate;
  set_vfov(vfov);

  bloom.name = name;


  set_message("Initializing renderer...");

  Mesh_Data quad_data = load_mesh_plane();
  quad_data.unique_identifier = "RENDERER's PLANE";
  quad = Mesh(quad_data, "RENDERER's PLANE");
  temporalaa = Shader("passthrough.vert", "TemporalAA.frag");
  passthrough = Shader("passthrough.vert", "passthrough.frag");
  variance_shadow_map = Shader("passthrough.vert", "variance_shadow_map.frag");
  gamma_correction = Shader("passthrough.vert", "gamma_correction.frag");
  fxaa = Shader("passthrough.vert", "fxaa.frag");


  Texture_Descriptor uvtd;
  uvtd.name = "uvgrid.png";
  uv_map_grid = uvtd;

  set_message("Initializing instance buffers");
  const GLuint mat4_size = sizeof(GLfloat) * 4 * 4;
  const GLuint instance_buffer_size = MAX_INSTANCE_COUNT * mat4_size;

  glGenBuffers(1, &instance_mvp_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_mvp_buffer);
  glBufferData(
      GL_ARRAY_BUFFER, instance_buffer_size, (void *)0, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &instance_model_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_model_buffer);
  glBufferData(
      GL_ARRAY_BUFFER, instance_buffer_size, (void *)0, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  set_message("Renderer init finished");
  init_render_targets();
  FRAME_TIMER.start();
}

void Renderer::set_uniform_lights(Shader &shader)
{
  for (uint32 i = 0; i < lights.light_count; ++i)
  {
    Light_Uniform_Value_Cache *value_cache =
        &shader.program->light_values_cache[i];
    const Light_Uniform_Location_Cache *location_cache =
        &shader.program->light_locations_cache[i];
    const Light *new_light = &lights.lights[i];

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
    vec3 new_irradiance = new_light->color * new_light->brightness;
    if (value_cache->irradiance != new_irradiance)
    {
      value_cache->irradiance = new_irradiance;
      glUniform3fv(location_cache->irradiance, 1, &new_irradiance[0]);
    }

    if (value_cache->attenuation != new_light->attenuation)
    {
      value_cache->attenuation = new_light->attenuation;
      glUniform3fv(location_cache->attenuation, 1, &new_light->attenuation[0]);
    }

    const vec3 new_ambient =
        new_light->brightness * new_light->ambient * new_light->color;
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
    glUniform3fv(
        shader.program->additional_ambient_location, 1, &new_ambient[0]);
  }
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
      spotlight_shadow_maps[i].blur.target.color_attachments[0].bind(
          Texture_Location::s0 + i);

    const mat4 offset = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0,
        0.5, 0.0, 0.5, 0.5, 0.5, 1.0);
    const mat4 shadow_map_transform = offset * shadow_map->projection_camera;

    const Light_Uniform_Location_Cache *location_cache =
        &shader.program->light_locations_cache[i];
    Light_Uniform_Value_Cache *value_cache =
        &shader.program->light_values_cache[i];

    float32 new_max_variance = lights.lights[i].max_variance;
    if (value_cache->max_variance != new_max_variance)
    {
      value_cache->max_variance = new_max_variance;
      glUniform1f(location_cache->max_variance, new_max_variance);
    }
    if (value_cache->shadow_map_transform != shadow_map_transform)
    {
      value_cache->shadow_map_transform = shadow_map_transform;
      glUniformMatrix4fv(location_cache->shadow_map_transform, 1, GL_FALSE,
          &shadow_map_transform[0][0]);
    }
    bool casts_shadows = lights.lights[i].casts_shadows;
    if (value_cache->shadow_map_enabled != casts_shadows)
    {
      value_cache->shadow_map_enabled = casts_shadows;
      glUniform1i(location_cache->shadow_map_enabled, casts_shadows);
    }
  }
}

mat4 Renderer::ortho_projection(ivec2 &dst_size)
{
  return ortho(0.0f, (float32)dst_size.x, 0.0f, (float32)dst_size.y, 0.1f,
             100.0f) *
         translate(vec3(vec2(0.5f * dst_size.x, 0.5f * dst_size.y), -1)) *
         scale(vec3(dst_size, 1));
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
    shadow_map->init(shadow_map_size);
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

    const mat4 camera =
        glm::lookAt(light->position, light->direction, vec3(0, 0, 1));
    const mat4 projection = glm::perspective(light->shadow_fov, 1.0f,
        light->shadow_near_plane, light->shadow_far_plane);
    shadow_map->projection_camera = projection * camera;
    for (Render_Entity &entity : render_entities)
    {
      if (entity.material->m.casts_shadows)
      {
        ASSERT(entity.mesh);
        int vao = entity.mesh->get_vao();
        glBindVertexArray(vao);
        variance_shadow_map.use();
        variance_shadow_map.set_uniform(
            "transform", shadow_map->projection_camera * entity.transformation);
        glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
        glDrawElements(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(),
            GL_UNSIGNED_INT, nullptr);
      }
    }

    shadow_map->blur.draw(this, &shadow_map->pre_blur.color_attachments[0],
        light->shadow_blur_radius, light->shadow_blur_iterations);

    // glGenerateMipmap(GL_TEXTURE_2D);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// shader must use passthrough.vert, and must use texture1, texture2 ...
// texturen for input texture sampler names
void run_pixel_shader(Shader *shader, vector<Texture *> *src_textures,
    Framebuffer *dst, bool clear_dst)
{
  ASSERT(shader);
  ASSERT(shader->vs == "passthrough.vert");

  static Mesh quad = Mesh(load_mesh_plane(), "run_pixel_shader_quad");

  if (dst)
  {
    ASSERT(dst->fbo);
    ASSERT(dst->color_attachments.size());
    glBindFramebuffer(GL_FRAMEBUFFER, dst->fbo->fbo);
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
  glBindVertexArray(quad.get_vao());
  shader->use();
  shader->set_uniform("transform", Renderer::ortho_projection(viewport_size));
  shader->set_uniform("time", (float32)get_real_time());
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer());
  glDrawElements(
      GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  for (uint32 i = 0; i < src_textures->size(); ++i)
  {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

void imgui_light_array(Light_Array &lights)
{
  static bool open = false;
  const uint32 initial_height = 50;
  const uint32 height_per_inactive_light = 25;
  const uint32 max_height = 600;
  uint32 width = 243;

  uint32 height =
      initial_height + height_per_inactive_light * lights.light_count;

  ImGui::Begin("lighting adjustment", &open, ImGuiWindowFlags_NoResize);
  ImGui::SetWindowSize(ImVec2((float)width, (float)height));
  if (ImGui::Button("Push Light"))
  {
    lights.light_count++;

    if (lights.light_count > MAX_LIGHTS)
    {
      lights.light_count = MAX_LIGHTS;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Pop Light"))
  {
    if (!lights.light_count == 0)
    {
      lights.light_count--;
    }
  }
  ImGui::SameLine();
  bool want_collapse = false;
  if (ImGui::Button("Collapse"))
  {
    want_collapse = true;
  }
  ImGui::BeginChild("ScrollingRegion");
  for (uint32 i = 0; i < lights.light_count; ++i)
  {
    ImGui::PushID(s(i).c_str());
    ImGui::PushItemWidth(150);
    if (want_collapse)
    {
      ImGui::SetNextTreeNodeOpen(false);
    }
    if (!ImGui::CollapsingHeader(s("Light ", i).c_str()))
    {
      ImGui::PopID();
      continue;
    }
    width = 270;
    height = max_height;
    Light *light = &lights.lights[i];
    ImGui::Indent(5);
    ImGui::LabelText("Option", "Setting %u", i);
    ImGui::ColorPicker3("Irradiance", &light->color[0],
        ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_RGB);
    ImGui::DragFloat3("Position", &light->position[0]);
    ImGui::DragFloat("Brightness", &light->brightness);
    if (light->brightness == 0)
      light->brightness = 0.00000001f;
    ImGui::DragFloat3("Attenuation", &light->attenuation[0], 0.01f);
    ImGui::DragFloat("Ambient", &light->ambient);
    std::string current_type = s(light->type);
    if (ImGui::BeginCombo("Light Type", current_type.c_str()))
    {
      const uint light_type_count = 3;
      for (int n = 0; n < light_type_count; n++)
      {
        std::string list_type_n = s(Light_Type(n));
        bool is_selected = (current_type == list_type_n);
        if (ImGui::Selectable(list_type_n.c_str(), is_selected))
          light->type = Light_Type(n);
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    if (light->type == Light_Type::parallel)
    {
      ImGui::DragFloat3("Direction", &light->direction[0]);
    }
    if (light->type == Light_Type::omnidirectional)
    {
    }
    if (light->type == Light_Type::spot)
    {
      ImGui::DragFloat3("Direction", &light->direction[0]);
      ImGui::DragFloat("Focus", &light->cone_angle, 0.0001f, 0.0000001, 0.5);
      ImGui::Checkbox("Casts Shadows", &light->casts_shadows);
      if (light->casts_shadows)
      {
        if (ImGui::TreeNode("Shadow settings"))
        {
          width = 320;
          ImGui::InputInt(
              "Blur Iterations", &light->shadow_blur_iterations, 1, 0);
          ImGui::DragFloat(
              "Blur Radius", &light->shadow_blur_radius, 0.1f, 0.00001f, 0.0f);
          ImGui::DragFloat(
              "Near Plane", &light->shadow_near_plane, 1.0f, 0.00001f);
          ImGui::DragFloat("Far Plane", &light->shadow_far_plane, 1.0f, 1.0f);
          ImGui::InputFloat(
              "Max Variance", &light->max_variance, 0.0000001f, 0.0001f, 12);
          ImGui::DragFloat("FoV", &light->shadow_fov, 1.0f, 0.0000001f, 90.f);
          ImGui::TreePop();
          if (light->shadow_blur_iterations < 0)
            light->shadow_blur_iterations = 0;
          if (light->max_variance < 0)
            light->max_variance = 0;
          if (light->shadow_blur_radius < 0)
            light->shadow_blur_radius = 0;
        }
      }
    }
    ImGui::Unindent(5.f);
    ImGui::PopItemWidth();
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::SetWindowSize(ImVec2((float32)width, (float32)height));
  ImGui::End();
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
  for (Render_Entity &entity : render_entities)
  {
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    // set_message(s("drawing entity with name:", entity.name));
    // set_message(s("binding vao:", vao));
    glBindVertexArray(vao);
    Shader &shader = entity.material->shader;
    shader.use();

    entity.material->bind(&shader);
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->m.uv_scale);
    shader.set_uniform("normal_uv_scale", entity.material->m.normal_uv_scale);
    shader.set_uniform("MVP", projection * camera * entity.transformation);
    shader.set_uniform("Model", entity.transformation);
    shader.set_uniform("alpha_albedo_override", -1.0f); //-1 is disabled
#if SHOW_UV_TEST_GRID
    uv_map_grid.bind(10);
    shader.set_uniform("texture10_mod", uv_map_grid.t.mod);
#endif
    set_uniform_lights(shader);
    // set_message("SETTING SHADOW MAPS:");
    set_uniform_shadowmaps(shader);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    // set_message(s("drawing elements..."));
    glDrawElements(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(),
        GL_UNSIGNED_INT, nullptr);
    entity.material->unbind_textures();
  }
}

void Renderer::instance_pass(float32 time)
{
  for (auto &entity : render_instances)
  {
    ASSERT(0); // disables vertex attribs, so non instanced draw calls will need
               // to re-enable and re-point
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

    glBindBuffer(GL_ARRAY_BUFFER, instance_mvp_buffer);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0, buffer_size, &entity.MVP_Matrices[0][0][0]);
    glBindBuffer(GL_ARRAY_BUFFER, instance_model_buffer);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0, buffer_size, &entity.Model_Matrices[0][0][0]);

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
    glBindBuffer(GL_ARRAY_BUFFER, instance_mvp_buffer);
    glVertexAttribPointer(loc1, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(0));
    glVertexAttribPointer(
        loc2, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(sizeof(GLfloat) * 4));
    glVertexAttribPointer(
        loc3, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(sizeof(GLfloat) * 8));
    glVertexAttribPointer(
        loc4, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(sizeof(GLfloat) * 12));
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
    glBindBuffer(GL_ARRAY_BUFFER, instance_model_buffer);
    glVertexAttribPointer(loc_1, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(0));
    glVertexAttribPointer(
        loc_2, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(sizeof(GLfloat) * 4));
    glVertexAttribPointer(
        loc_3, 4, GL_FLOAT, GL_FALSE, mat4_size, (void *)(sizeof(GLfloat) * 8));
    glVertexAttribPointer(loc_4, 4, GL_FLOAT, GL_FALSE, mat4_size,
        (void *)(sizeof(GLfloat) * 12));
    glVertexAttribDivisor(loc_1, 1);
    glVertexAttribDivisor(loc_2, 1);
    glVertexAttribDivisor(loc_3, 1);
    glVertexAttribDivisor(loc_4, 1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    glDrawElementsInstanced(GL_TRIANGLES,
        entity.mesh->get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0,
        num_instances);

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

void Renderer::translucent_pass(float32 time)
{
  glDisable(GL_CULL_FACE);
  // glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (Render_Entity &entity : translucent_entities)
  {
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    glBindVertexArray(vao);
    Shader &shader = entity.material->shader;
    shader.use();
    entity.material->bind(&shader);
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->m.uv_scale);
    shader.set_uniform("MVP", projection * camera * entity.transformation);
    shader.set_uniform("Model", entity.transformation);
    shader.set_uniform("discard_on_alpha", false);
    shader.set_uniform(
        "alpha_albedo_override", entity.material->m.albedo_alpha_override);

#if SHOW_UV_TEST_GRID
    uv_map_grid.bind(10);
    shader.set_uniform("texture10_mod", uv_map_grid.t.mod);
#endif

    set_uniform_lights(shader);
    set_uniform_shadowmaps(shader);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    glDrawElements(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(),
        GL_UNSIGNED_INT, nullptr);
  }
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

void imgui_fxaa(Shader *fxaa)
{
  static float EDGE_THRESHOLD_MIN = 0.0312;
  static float EDGE_THRESHOLD_MAX = 0.125;
  static float SUBPIXEL_QUALITY = 0.75f;
  static int ITERATIONS = 8;
  static int QUALITY = 1;

  static bool open = true;
  ImGui::Begin("fxaa adjustment", &open);
  ImGui::SetWindowSize(ImVec2(300, 160));
  ImGui::DragFloat("EDGE_MIN", &EDGE_THRESHOLD_MIN, 0.001f);
  ImGui::DragFloat("EDGE_MAX", &EDGE_THRESHOLD_MAX, 0.001f);
  ImGui::DragFloat("SUBPIXEL", &SUBPIXEL_QUALITY, 0.001f);
  ImGui::DragInt("ITERATIONS", &ITERATIONS, 0.1f);
  ImGui::DragInt("QUALITY", &QUALITY, 0.1f);

  fxaa->set_uniform("EDGE_THRESHOLD_MIN", EDGE_THRESHOLD_MIN);
  fxaa->set_uniform("EDGE_THRESHOLD_MAX", EDGE_THRESHOLD_MAX);
  fxaa->set_uniform("SUBPIXEL_QUALITY", SUBPIXEL_QUALITY);
  fxaa->set_uniform("ITERATIONS", ITERATIONS);
  fxaa->set_uniform("QUALITY", QUALITY);

  ImGui::End();
}

void Renderer::render(float64 state_time)
{ /*
   set_message("sin(time) [-1,1]:", s(sin(state_time)), 1.0f);
   set_message("sin(time) [ 0,1]:", s(0.5f + 0.5f*sin(state_time)), 1.0f);*/
#if DYNAMIC_FRAMERATE_TARGET
  dynamic_framerate_target();
#endif
#if DYNAMIC_TEXTURE_RELOADING
  check_and_clear_expired_textures();
#endif
#if SHOW_UV_TEST_GRID
  uv_map_grid.t.mod =
      vec4(1, 1, 1, clamp((float32)pow(sin(state_time), .25f), 0.0f, 1.0f));
#endif

  static bool show_renderer_window = true;
  if (show_renderer_window)
  {
    ImGui::Begin("Renderer", &show_renderer_window);
    ImGui::BeginChild("ScrollingRegion");

    uint32 height = 60;
    float width = 200;
    const uint32 height_per_header = 25;
    const uint32 height_per_treenode = 15;

    if (ImGui::CollapsingHeader("GPU Textures"))
    { // todo improve texture names for generated textures
      height += height_per_header;
      ImGui::Indent(5);
      std::vector<std::shared_ptr<Texture_Handle>> handles;

      for (auto &tex : TEXTURE2D_CACHE)
      {
        auto ptr = tex.second.lock();
        if (ptr)
          handles.push_back(ptr);
      }
      std::sort(handles.begin(), handles.end(),
          [](std::shared_ptr<Texture_Handle> a,
              std::shared_ptr<Texture_Handle> b) {
            return a->peek_filename().compare(b->peek_filename()) < 0;
          });
      for (std::shared_ptr<Texture_Handle> &ptr : handles)
      {
        uint32 id = (uint32) & (*ptr);
        ImGui::PushID(s(id).c_str());
        height += height_per_treenode;
        width = 400;
        if (ImGui::TreeNode(ptr->peek_filename().c_str()))
        {
          height = 600;
          width = 800;
          ImGui::Text(s("Heap Address:", (uint32)ptr.get()).c_str());
          ImGui::Text(s("Ptr Refcount:", (uint32)ptr.use_count()).c_str());
          ImGui::Text(s("OpenGL handle:", ptr->texture).c_str());
          const char *format = texture_format_to_string(ptr->format);

          ImGui::Text(s("Format:", format).c_str());
          ImGui::Text(s("Size:", ptr->size.x, "x", ptr->size.y).c_str());

          ImGui::Image((ImTextureID)ptr->get_imgui_handle(), ImVec2(256, 256),
              ImVec2(0, 1), ImVec2(1, 0));
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
      ImGui::Unindent(5.f);
    }

    ImGui::EndChild();

    ImGui::SetWindowSize(ImVec2(width, height));
    ImGui::End();
  }

  float32 time = (float32)get_real_time();
  float64 t = (time - state_time) / dt;

  set_message("FRAME_START", "");
  set_message("BUILDING SHADOW MAPS START", "");
  build_shadow_maps();
  set_message("OPAQUE PASS START", "");
  opaque_pass(time);

  // instance_pass(time);
  translucent_pass(time);
#if POSTPROCESSING
  //bloom.draw(this, &draw_target.color_attachments[0], &draw_target);
#else

#endif

  glDisable(GL_DEPTH_TEST);
  // todo fix txaa
  if (use_txaa)
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
    glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(),
        GL_UNSIGNED_INT, (void *)0);
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
    glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(),
        GL_UNSIGNED_INT, (void *)0);

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
    glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(),
        GL_UNSIGNED_INT, (void *)0);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  // do fxaa or passthrough if disabled
  draw_target_fxaa.bind();
  if (use_fxaa)
  {
    fxaa.use();
    imgui_fxaa(&fxaa);
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
  glDrawElements(
      GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  // draw draw_target_post_fxaa to screen
  glViewport(0, 0, window_size.x, window_size.y);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_FRAMEBUFFER_SRGB);
  gamma_correction.use();
  gamma_correction.set_uniform("transform", ortho_projection(window_size));
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  draw_target_fxaa.color_attachments[0].bind(0);


  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer()); 
  glDrawElements(
      GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  FRAME_TIMER.stop();
  SWAP_TIMER.start();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glBindVertexArray(0);

  frame_count += 1;
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
  camera_position = pos;
  camera = glm::lookAt(pos, p, {0, 0, 1});
}
void Renderer::set_camera_gaze(vec3 pos, vec3 p)
{
  camera_position = pos;
  camera = glm::lookAt(pos, p, {0, 0, 1});
}

void Renderer::set_vfov(float32 vfov)
{
  const float32 aspect = (float32)window_size.x / (float32)window_size.y;
  const float32 znear = 0.1f;
  const float32 zfar = 1000;
  projection = glm::perspective(radians(vfov), aspect, znear, zfar);
}

void Renderer::set_render_entities(vector<Render_Entity> *new_entities)
{
  previous_render_entities = move(render_entities);
  render_entities.clear();
  translucent_entities.clear();

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
      // todo: ASSERT(0); // test this, the furthest objects should be first
    }
    else
    {
      render_entities.push_back((*new_entities)[i.first]);
    }
  }
}

void Renderer::set_lights(Light_Array new_lights) { lights = new_lights; }

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

void Renderer::init_render_targets()
{
  set_message("init_render_targets()");

  size = ivec2(render_scale * window_size.x, render_scale * window_size.y);
  bloom.init(size, FRAMEBUFFER_FORMAT);

  Texture_Descriptor td;
  td.name = name + " Renderer::draw_target.color[0]";
  td.size = size;
  td.format = FRAMEBUFFER_FORMAT;
  draw_target.color_attachments = { Texture(td) };
  draw_target.depth_enabled = true;
  draw_target.depth_size = size;
  draw_target.init();

  td.name = name + " Renderer::previous_frame.color[0]";
  previous_draw_target.color_attachments = { Texture(td) };
  previous_draw_target.init();

  // full render scaled, clamped and encoded srgb
  Texture_Descriptor srgb8;
  srgb8.name = name + " Renderer::draw_target_srgb8.color[0]";
  srgb8.size = size;
  srgb8.format = GL_SRGB;
  draw_target_srgb8.color_attachments = { Texture(srgb8) };
  draw_target_srgb8.encode_srgb_on_draw = true;
  draw_target_srgb8.init();

  // full render scaled, fxaa or passthrough target
  Texture_Descriptor fxaa;
  fxaa.name = name + " Renderer::draw_target_post_fxaa.color[0]";
  fxaa.size = size;
  fxaa.format = GL_SRGB;
  draw_target_fxaa.color_attachments = { Texture(fxaa) };
  draw_target_fxaa.encode_srgb_on_draw = true;
  draw_target_fxaa.init();
}

void Renderer::dynamic_framerate_target()
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
          std::to_string(swap_avg) + " " + std::to_string(target_frame_time));
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

  mat4 result = glm::translate(vec3(translation / vec2(size), 0));
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
  pre_blur.depth_enabled = true;
  pre_blur.depth_size = size;
  pre_blur.depth_format = GL_DEPTH_COMPONENT32;

  if (!initialized)
  {
    Texture_Descriptor pre_blur_td;
    pre_blur_td.name = "Spotlight Shadow Map pre_blur[0]";
    pre_blur_td.size = size;
    pre_blur_td.format = format;
    pre_blur.color_attachments = {Texture(pre_blur_td)};
    pre_blur.init();
  }

  ASSERT(pre_blur.color_attachments.size() == 1);

  Texture_Descriptor td;
  td.name = "Spotlight Shadow Map";
  td.size = size;
  td.format = format;
  td.border_color = glm::vec4(999999, 999999, 0, 0);
  td.wrap_s = GL_CLAMP_TO_BORDER;
  td.wrap_t = GL_CLAMP_TO_BORDER;
  blur.init(td);

  initialized = true;
}

Renderbuffer_Handle::~Renderbuffer_Handle() { glDeleteRenderbuffers(1, &rbo); }

Cubemap::Cubemap() {}

void Cubemap::bind(GLuint texture_unit)
{
  if (handle)
  {
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
  }
}

// filenames ordered: right left top bottom back front

Cubemap_Descriptor::Cubemap_Descriptor() {}

Cubemap_Descriptor::Cubemap_Descriptor(std::string directory)
{
  directory = BASE_TEXTURE_PATH + directory;
  // yellow means original side name
  // red is where it should appear

  // todo: cubemap preprocess: revert the cubemap file changes from default when
  // implemented:  necessary processing:

  // note: in order to import a new set of cubemap files, you must do the
  // following in an  image editor first  rotate bottom.jpg 180 degrees  rotate
  // front.jpg 180 degrees  rotate left.jpg 90 clockwise  rotate right.jpg 90
  // anticlockwise

  // opengl left = right.jpg
  // opengl right = left.jpg
  // opengl top = back.jpg
  // opengl front = top.jpg
  // opengl bottom = front.jpg
  // opengl back = bottom.jpg
  //

  // opengl right
  faces[0] = directory + "/left.jpg";

  // opengl left
  faces[1] = directory + "/right.jpg";

  // opengl top
  faces[2] = directory + "/back.jpg";

  // opengl bottom
  faces[3] = directory + "/front.jpg";

  // opengl back
  faces[4] = directory + "/bottom.jpg";

  // opengl front
  faces[5] = directory + "/top.jpg";

  // opengl z-forward space is right left top bottom back front
  // faces[0] = directory + "/right.jpg";
  // faces[1] = directory + "/left.jpg";
  // faces[2] = directory + "/top.jpg";
  // faces[3] = directory + "/bottom.jpg";
  // faces[4] = directory + "/back.jpg";
  // faces[5] = directory + "/front.jpg";
}

Cubemap::Cubemap(Cubemap_Descriptor d)
{
  descriptor = d;
  std::string key = d.faces[0] + d.faces[1] + d.faces[2] + d.faces[3] +
                    d.faces[4] + d.faces[5];
  handle = TEXTURECUBEMAP_CACHE[key].lock();
  if (handle)
  {
    return;
  }
  handle = std::make_shared<Texture_Handle>();
  TEXTURECUBEMAP_CACHE[key] = handle;

  for (uint32 i = 0; i < 6; ++i)
  { // opengl z-forward space is right left top bottom back front
    int32 width, height, n;
    auto *data = stbi_load(d.faces[i].c_str(), &width, &height, &n, 4);
    if (!data)
    {
      // retry next frame
      set_message("Warning: missing Cubemap Texture:" + d.faces[i]);
      handle = nullptr;
      return;
    }
    set_message(
        "Cubemap load cache miss. Texture from disk: ", d.faces[i], 1.0);
    if (descriptor.process_premultiply)
    {
      for (int32 i = 0; i < width * height; ++i)
      {
        uint32 pixel = data[i];
        uint8 r = (uint8)(0x000000FF & pixel);
        uint8 g = (uint8)((0x0000FF00 & pixel) >> 8);
        uint8 b = (uint8)((0x00FF0000 & pixel) >> 16);
        uint8 a = (uint8)((0xFF000000 & pixel) >> 24);

        r = (uint8)round(r * ((float)a / 255));
        g = (uint8)round(g * ((float)a / 255));
        b = (uint8)round(b * ((float)a / 255));

        data[i] = (24 << a) | (16 << b) | (8 << g) | r;
      }
    }

    if (i == 0)
    {
      glGenTextures(1, &handle->texture);
      glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
    }
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, d.format, width, height,
        0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}
