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
#include "Scene_Graph.h"
#include "UI.h"
#include "State.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#include <filesystem>

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

#define DIFFUSE_TARGET GL_COLOR_ATTACHMENT0
#define DEPTH_TARGET GL_COLOR_ATTACHMENT1
#define NORMAL_TARGET GL_COLOR_ATTACHMENT2
#define POSITION_TARGET GL_COLOR_ATTACHMENT3

const vec4 Conductor_Reflectivity::iron = vec4(0.56, 0.57, 0.58, 1.0f);
const vec4 Conductor_Reflectivity::copper = vec4(0.95, 0.64, 0.54, 1.0f);
const vec4 Conductor_Reflectivity::gold = vec4(1.0, 0.71, 0.29, 1.0f);
const vec4 Conductor_Reflectivity::aluminum = vec4(0.91, 0.92, 0.92, 1.0f);
const vec4 Conductor_Reflectivity::silver = vec4(0.95, 0.93, 0.88, 1.0f);
const uint32 ENV_MAP_MIP_LEVELS = 7;
static unordered_map<string, weak_ptr<Mesh_Handle>> MESH_CACHE;
static unordered_map<string, weak_ptr<Texture_Handle>> TEXTURE2D_CACHE;
static unordered_map<string, weak_ptr<Texture_Handle>> TEXTURECUBEMAP_CACHE;
shared_ptr<Texture_Handle> COPIED_TEXTURE_HANDLE;
extern std::unordered_map<std::string, std::weak_ptr<Shader_Handle>> SHADER_CACHE;

// if we overwrite an opengl object in a state thread that isnt the main thread
// we need to defer deletion of the resources, we can do it anywhere in the main thread
// so theyre just cleared at the top of render()

std::vector<Mesh_Handle> DEFERRED_MESH_DELETIONS;
std::vector<Texture_Handle> DEFERRED_TEXTURE_DELETIONS;

// all unsuccessful texture binds will bind white instead of default black
// this means we need default values for the mod value of the textures
Texture WHITE_TEXTURE;
const vec4 DEFAULT_ALBEDO = vec4(1);
const vec4 DEFAULT_NORMAL = vec4(0.5, 0.5, 1.0, 0.0);
const vec4 DEFAULT_ROUGHNESS = vec4(0.3);
const vec4 DEFAULT_METALNESS = vec4(0);
const vec4 DEFAULT_EMISSIVE = vec4(0);
const vec4 DEFAULT_DISPLACEMENT = vec4(0);
const vec4 DEFAULT_AMBIENT_OCCLUSION = vec4(1);
const std::string DEFAULT_VERTEX_SHADER = "vertex_shader.vert";
const std::string DEFAULT_FRAG_SHADER = "fragment_shader.frag";

void check_FBO_status(GLuint fbo);
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
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  if (!fbo)
  {
    fbo = make_shared<Framebuffer_Handle>();
    // glGenFramebuffers(1, &fbo->fbo);
    glCreateFramebuffers(1, &fbo->fbo);
  }
  draw_buffers.clear();
  for (uint32 i = 0; i < color_attachments.size(); ++i)
  {
    glNamedFramebufferTexture(fbo->fbo, GL_COLOR_ATTACHMENT0 + i, color_attachments[i].get_handle(), 0);

    ASSERT(color_attachments[i].texture->size != ivec2(0));
    draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
  }
  GLint current_fbo;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
  glDrawBuffers(uint32(draw_buffers.size()), &draw_buffers[0]);
  glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
  if (depth_enabled)
  {
    if (use_renderbuffer_depth && depth == nullptr)
    {
      depth = make_shared<Renderbuffer_Handle>();
      glGenRenderbuffers(1, &depth->rbo);
      glBindRenderbuffer(GL_RENDERBUFFER, depth->rbo);
      // glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,depth_size.x,depth_size.y);
      glNamedRenderbufferStorage(depth->rbo, depth_format, depth_size.x, depth_size.y);
      // glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_COMPONENT,GL_RENDERBUFFER,depth->rbo);
      glNamedFramebufferRenderbuffer(fbo->fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth->rbo);
      depth->format = depth_format;
      depth->size = depth_size;
    }
    else
    {
      if (!depth_texture.texture)
      {
        depth_texture.t.size = depth_size;
        depth_texture.t.format = depth_format;

        if (depth_texture.t.name == "default")
          depth_texture.t.name = "some unnamed depth texture";
        depth_texture.t.source = "generate";
        depth_texture.load();
        ASSERT(depth_texture.get_handle());
        glNamedFramebufferTexture(fbo->fbo, GL_DEPTH_ATTACHMENT, depth_texture.get_handle(), 0);
      }
    }
  }
  check_FBO_status(fbo->fbo);
}

void Framebuffer::bind_for_drawing_dst()
{
  ASSERT(fbo);
  ASSERT(fbo->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
  // check_FBO_status();
}

Gaussian_Blur::Gaussian_Blur() {}
void Gaussian_Blur::init(Texture_Descriptor &td)
{
  ASSERT(td.source == "generate");
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
  gaussian_blur_shader.set_uniform("transform", fullscreen_quad());
  src->bind_for_sampling_at(0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->quad.get_indices_buffer());
  glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  gaus_scale.y = radius / dst_size->x;
  gaus_scale.x = 0;
  gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo->fbo);

  intermediate_fbo.color_attachments[0].bind_for_sampling_at(0);

  glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);

  iterations -= 1;

  for (uint i = 0; i < iterations; ++i)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, intermediate_fbo.fbo->fbo);
    vec2 gaus_scale = vec2(aspect_ratio_factor * radius / dst_size->x, 0.0);
    gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
    target.color_attachments[0].bind_for_sampling_at(0);
    glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);
    gaus_scale.y = radius / dst_size->x;
    gaus_scale.x = 0;
    gaussian_blur_shader.set_uniform("gauss_axis_scale", gaus_scale);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo->fbo);
    intermediate_fbo.color_attachments[0].bind_for_sampling_at(0);
    glDrawElements(GL_TRIANGLES, renderer->quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glEnable(GL_DEPTH_TEST);
}

Renderer::~Renderer() {}
//
// void check_and_clear_expired_textures()
//{
//  auto it = TEXTURE2D_CACHE.begin();
//  while (it != TEXTURE2D_CACHE.end())
//  {
//    auto texture_handle = *it;
//    const string *path = &texture_handle.first;
//    struct stat attr;
//    stat(path->c_str(), &attr);
//    time_t t = attr.st_mtime;
//
//    auto ptr = texture_handle.second.lock();
//    if (ptr) // texture could exist but not yet be loaded
//    {
//      time_t t1 = ptr.get()->file_mod_t;
//      if (t1 != t)
//      {
//        it = TEXTURE2D_CACHE.erase(it);
//        continue;
//      }
//    }
//    ++it;
//  }
//}

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

int32 save_texture(Texture *texture, std::string filename, uint32 level)
{
  const GLenum format = texture->texture->internalformat;
  uint32 width = texture->texture->size.x;
  uint32 height = texture->texture->size.y;

  if (is_float_format(texture->texture->internalformat))
  {
    uint32 comp = 4;
    std::vector<float32> data((uint32)(width * height * comp));
    uint32 size = uint32(data.size()) * (uint32)sizeof(float32);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureImage(texture->texture->texture, level, GL_RGBA, GL_FLOAT, size, &data[0]);

    int32 result = stbi_write_hdr(s(filename, ".hdr").c_str(), width, height, comp, &data[0]);
    return result;
  }
  else
  {
    uint32 comp = 4;
    std::vector<uint8> data((uint32)(width * height * comp));
    uint32 size = uint32(data.size()) * (uint32)sizeof(uint8);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureImage(texture->texture->texture, level, GL_RGBA, GL_UNSIGNED_BYTE, size, &data[0]);

    int32 result = stbi_write_png(s(texture->t.name, ".png").c_str(), width, height, comp, &data[0], 0);
    return result;
  }
}

int32 save_texture_type(GLuint texture, ivec2 size, std::string filename, std::string type, uint32 level)
{
  uint32 width = size.x;
  uint32 height = size.y;

  if (type == "hdr")
  {
    uint32 comp = 4;
    std::vector<float32> data(width * height * comp);
    uint32 size = data.size() * sizeof(float32);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureImage(texture, level, GL_RGBA, GL_FLOAT, size, &data[0]);

    int32 result = stbi_write_hdr(s(filename, ".hdr").c_str(), width, height, comp, &data[0]);
    return result;
  }
  if (type == "png")
  {
    uint32 comp = 4;
    std::vector<uint8> data(width * height * comp);
    uint32 size = data.size() * sizeof(uint8);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureImage(texture, level, GL_RGBA, GL_UNSIGNED_BYTE, size, &data[0]);

    int32 result = stbi_write_png(s(filename, ".png").c_str(), width, height, comp, &data[0], 0);
    return result;
  }
  return 0;
}

int32 save_texture_type(Texture *texture, std::string filename, std::string type, uint32 level)
{
  const GLenum format = texture->texture->internalformat;
  uint32 width = texture->texture->size.x;
  uint32 height = texture->texture->size.y;

  if (type == "hdr")
  {
    uint32 comp = 4;
    std::vector<float32> data(width * height * comp);
    uint32 size = uint32(data.size()) * sizeof(float32);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureImage(texture->texture->texture, level, GL_RGBA, GL_FLOAT, size, &data[0]);

    int32 result = stbi_write_hdr(s(filename, ".hdr").c_str(), width, height, comp, &data[0]);
    return result;
  }
  if (type == "png")
  {
    uint32 comp = 4;
    std::vector<uint8> data(width * height * comp);
    uint32 size = uint32(data.size()) * sizeof(uint8);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureImage(texture->texture->texture, level, GL_RGBA, GL_UNSIGNED_BYTE, size, &data[0]);

    int32 result = stbi_write_png(s(filename, ".png").c_str(), width, height, comp, &data[0], 0);
    return result;
  }
  return 0;
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

Texture::Texture(std::shared_ptr<Texture_Handle> texture)
{
  t.name = s("Created from copied handle of: ", texture->filename);
  t.source = texture->filename;
  t.size = texture->size;
  t.format = texture->internalformat;
  t.key = s(t.source, ",", t.format);
  t.wrap_s = texture->wrap_s;
  t.wrap_t = texture->wrap_t;
  t.minification_filter = texture->minification_filter;
  t.magnification_filter = texture->magnification_filter;
  t.levels = texture->levels;
  this->texture = texture;
}

Texture::Texture(Texture_Descriptor &td)
{
  t = td;
  t.key = s(t.source, ",", t.format);
}

Texture::Texture(const char *file_path)
{
  t.source = file_path;
  t.key = s(t.source, ",", t.format);
  t.format = GL_SRGB8_ALPHA8;
}

bool Texture::is_ready()
{
  return texture && texture->texture;
}

Texture::Texture(string name, ivec2 size, uint8 levels, GLenum internalformat, GLenum minification_filter,
    GLenum magnification_filter, GLenum wrap_s, GLenum wrap_t, vec4 border_color)
{
  this->t.name = name;
  this->t.source = "generate";
  this->t.size = size;
  this->t.levels = levels;
  this->t.format = internalformat;
  this->t.minification_filter = minification_filter;
  this->t.magnification_filter = magnification_filter;
  this->t.wrap_t = wrap_t;
  this->t.wrap_s = wrap_s;
  this->t.border_color = border_color;
  load();
}
Texture_Handle::~Texture_Handle()
{
  if (std::this_thread::get_id() != MAIN_THREAD_ID)
  {
    DEFERRED_TEXTURE_DELETIONS.push_back(*this);
    return;
  }
  glDeleteTextures(1, &texture);
}

void Texture_Handle::generate_ibl_mipmaps(float32 time)
{

  if (ibl_mipmaps_generated || (!(time > (this->time + 2 * dt))))
  {
    return;
  }
  this->time = time;
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

  const mat4 projection = perspective(half_pi<float>(), 1.0f, 0.01f, 10.0f);
  const vec3 origin = vec3(0);
  const vec3 posx = vec3(1.0f, 0.0f, 0.0f);
  const vec3 negx = vec3(-1.0f, 0.0f, 0.0f);
  const vec3 posy = vec3(0.0f, 1.0f, 0.0f);
  const vec3 negy = vec3(0.0f, -1.0f, 0.0f);
  const vec3 posz = vec3(0.0f, 0.0f, 1.0f);
  const vec3 negz = vec3(0.0f, 0.0f, -1.0f);
  const mat4 cameras[] = {lookAt(origin, posx, negy), lookAt(origin, negx, negy), lookAt(origin, posy, posz),
      lookAt(origin, negy, negz), lookAt(origin, posz, negy), lookAt(origin, negz, negy)};

  if (tiley == 0 && tilex == 0)
  {
    string label = filename + "ibl mipmapped";
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &ibl_texture_target);
    glTextureStorage2D(ibl_texture_target, ENV_MAP_MIP_LEVELS, GL_RGB16F, size.x, size.y);

    glTextureParameteri(ibl_texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(ibl_texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(ibl_texture_target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(ibl_texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(ibl_texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &ibl_fbo);
    // glCreateRenderbuffers(1, &ibl_rbo);

    ibl_source = texture;

    // uncomment to see progress:
    // texture = ibl_texture_target;
  }
  GLint current_fbo;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_fbo);

  glBindFramebuffer(GL_FRAMEBUFFER, ibl_fbo);
  specular_filter.use();
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_CUBE_MAP, ibl_source);
  ASSERT(ibl_source);

  float angle = radians(0.f);
  mat4 rot = toMat4(quat(1, 0, 0, angle));
  specular_filter.set_uniform("projection", projection);
  specular_filter.set_uniform("rotation", rot);

  glEnable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glFrontFace(GL_CCW);
  glCullFace(GL_FRONT);
  glDepthFunc(GL_LESS);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_SCISSOR_TEST);

  for (uint32 mip_level = 0; mip_level < ENV_MAP_MIP_LEVELS; ++mip_level)
  {
    uint32 width = (uint32)floor(size.x * pow(0.5, mip_level));
    uint32 height = (uint32)floor(size.y * pow(0.5, mip_level));
    uint32 xf = (uint32)floor(width / ibl_tile_max);
    uint32 x = tilex * xf;
    uint32 draw_width = (uint32)ceil(float32(width) / ibl_tile_max);

    uint32 yf = (uint32)floor(height / ibl_tile_max);
    uint32 y = tiley * yf;
    uint32 draw_height = (uint32)ceil(float32(height) / ibl_tile_max);

    glViewport(0, 0, width, height);

    // jank af, without these magic numbers its calculated exact
    // but its not pixel perfect at lower mip levels...
    // even with a scissor size the same as the viewport
    glScissor(x - 15, y - 15, draw_width + 33, draw_height + 33);
    float roughness = (float)mip_level / (float)(ENV_MAP_MIP_LEVELS - 1);
    specular_filter.set_uniform("roughness", roughness);
    specular_filter.set_uniform("size", float32(size.x));
    for (unsigned int i = 0; i < 6; ++i)
    {
      specular_filter.set_uniform("camera", cameras[i]);
      glFramebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, ibl_texture_target, mip_level);
      // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      cube.draw();
    }
  }

  glDisable(GL_SCISSOR_TEST);
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glViewport(0, 0, 0, 0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);

  bool x_at_end = (tilex == ibl_tile_max);
  bool y_at_end = (tiley == ibl_tile_max);
  if (x_at_end)
  {
    tiley = tiley + 1;
    tilex = 0;
  }
  else
  {
    tilex += 1;
  }

  if (x_at_end && y_at_end)
  {
    ibl_mipmaps_generated = true;
    texture = ibl_texture_target;
    glDeleteTextures(1, &ibl_source);
  }
}

void Texture::load()
{
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);

  if (texture)
  {
    if (texture->texture == 0)
    {
      static float64 last = get_real_time();
      float64 time = get_real_time();
      if (!(time > last + 0.05))
      {
        return;
      }
      last = time;

      if (texture->upload_sync != 0)
      {
        GLint ready = 0;
        glGetSynciv(texture->upload_sync, GL_SYNC_STATUS, 1, NULL, &ready);

        if (ready == GL_SIGNALED)
        {
          t.size = texture->size;
          glCreateTextures(GL_TEXTURE_2D, 1, &texture->texture);
          glBindBuffer(GL_PIXEL_UNPACK_BUFFER, texture->uploading_pbo);

          glTextureStorage2D(texture->texture, t.levels, texture->internalformat, texture->size.x, texture->size.y);
          glTextureSubImage2D(
              texture->texture, 0, 0, 0, texture->size.x, texture->size.y, GL_RGBA, texture->datatype, 0);
          glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
          // glDeleteBuffers(1, &texture->uploading_pbo); ??
          set_message("SYNC FINISHED FOR:", s(t.name, " ", t.source), 1.0f);
          glDeleteSync(texture->upload_sync);
          texture->upload_sync = 0;
          glGenerateTextureMipmap(texture->texture);
          check_set_parameters();
          texture->transfer_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
          return;
        }
      }
    }
    return;
  }

  if (t.name == "")
  {
    ASSERT(0);
  }

  if (t.source == "")
  {
    ASSERT(0);
  }

  if (t.source == "generate")
  {
    if (!texture)
    {
      texture = make_shared<Texture_Handle>();
      static uint32 i = 0;
      ++i;
      string key = s("Generated texture ID:", i, " Name:", t.name, ",", t.format);
      t.key = key;
      TEXTURE2D_CACHE[key] = texture;
      ASSERT(t.name != "");
      glCreateTextures(GL_TEXTURE_2D, 1, &texture->texture);
      glTextureStorage2D(texture->texture, t.levels, t.format, t.size.x, t.size.y);
      glObjectLabel(GL_TEXTURE, texture->texture, -1, t.name.c_str());
      texture->filename = t.name;
      texture->size = t.size;
      texture->levels = t.levels;
      texture->internalformat = t.format;
      ASSERT(texture->internalformat != 0);

      check_set_parameters();
      glGenerateTextureMipmap(texture->texture);
    }
    return;
  }
  auto ptr = TEXTURE2D_CACHE[t.key].lock();
  if (ptr)
  {
    texture = ptr;
    ASSERT(texture->size != ivec2(0));

    return;
  }

  if (t.source == "default" || t.source == "white")
  {
    texture = make_shared<Texture_Handle>();
    texture->filename = t.name;

    TEXTURE2D_CACHE[t.key] = texture;
    texture->internalformat = GL_SRGB8_ALPHA8;
    t.size = texture->size = ivec2(1);
    texture->datatype = GL_UNSIGNED_BYTE;
    uint8 arr[4] = {255, 255, 255, 255};
    glCreateTextures(GL_TEXTURE_2D, 1, &texture->texture);
    glTextureStorage2D(texture->texture, 1, GL_RGBA8, 1, 1);
    glTextureSubImage2D(texture->texture, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &arr[0]);
    return;
  }

  Image_Data imgdata;
  if (IMAGE_LOADER.load(t.source, &imgdata, t.format))
  {
    if (!imgdata.data)
    {
      texture = make_shared<Texture_Handle>();
      texture->filename = t.name + " - FILE MISSING";
      TEXTURE2D_CACHE[t.key] = texture;
      texture->internalformat = GL_SRGB8_ALPHA8;
      t.size = texture->size = ivec2(1);
      texture->datatype = GL_UNSIGNED_BYTE;
      texture->levels = t.levels;
      uint8 arr[4] = {255, 255, 255, 255};
      glCreateTextures(GL_TEXTURE_2D, 1, &texture->texture);
      glTextureStorage2D(texture->texture, 1, GL_SRGB8_ALPHA8, 1, 1);
      glTextureSubImage2D(texture->texture, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &arr[0]);
      return;
    }
    texture = make_shared<Texture_Handle>();
    TEXTURE2D_CACHE[t.key] = texture;

    struct stat attr;
    stat(t.source.c_str(), &attr);
    texture.get()->file_mod_t = attr.st_mtime;

    if (t.name == "default")
    {
      t.name = t.source;
    }

    t.size = texture->size = ivec2(imgdata.x, imgdata.y);
    t.levels = 1 + floor(glm::log2(float32(glm::max(texture->size.x, texture->size.y))));
    texture->levels = t.levels;
    texture->filename = t.name;

    texture->internalformat = t.format;
    texture->border_color = t.border_color;

    set_message(s("Texture load cache miss. Texture from disk: ", t.name,
                    "\nInternal_Format: ", texture_format_to_string(texture->internalformat)),
        "", 3.f);
    PERF_TIMER.start();
    glCreateBuffers(1, &texture->uploading_pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, texture->uploading_pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, imgdata.data_size, imgdata.data, GL_STATIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    PERF_TIMER.stop();
    texture->datatype = imgdata.data_type;

    stbi_image_free(imgdata.data);
    set_message("PERF:", PERF_TIMER.string_report(), 55.0f);
    set_message("SYNC STARTED FOR:", s(t.name, " ", t.source), 1.0f);
    texture->upload_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  }
}

void Texture::check_set_parameters()
{

  if (t.magnification_filter != texture->magnification_filter)
  {
    texture->magnification_filter = t.magnification_filter;
    glTextureParameteri(texture->texture, GL_TEXTURE_MAG_FILTER, t.magnification_filter);
  }
  if (t.minification_filter != texture->minification_filter)
  {
    texture->minification_filter = t.minification_filter;
    glTextureParameteri(texture->texture, GL_TEXTURE_MIN_FILTER, t.minification_filter);
  }
  if (t.wrap_t != texture->wrap_t)
  {
    texture->wrap_t = t.wrap_t;
    glTextureParameteri(texture->texture, GL_TEXTURE_WRAP_T, t.wrap_t);
  }
  if (t.wrap_s != texture->wrap_s)
  {
    texture->wrap_s = t.wrap_s;
    glTextureParameteri(texture->texture, GL_TEXTURE_WRAP_S, t.wrap_s);
  }
  if (t.border_color != texture->border_color)
  {
    texture->border_color = t.border_color;
    glTextureParameterfv(texture->texture, GL_TEXTURE_BORDER_COLOR, &t.border_color[0]);
  }
}

bool Texture::bind_for_sampling_at(GLuint binding)
{
  load();
  if (!texture)
  {
    WHITE_TEXTURE.bind_for_sampling_at(binding);
    return false;
  }
  if (texture->texture == 0)
  {
    WHITE_TEXTURE.bind_for_sampling_at(binding);
    return false;
  }

  if (texture->transfer_sync != 0)
  {
    GLint ready = 0;
    glGetSynciv(texture->transfer_sync, GL_SYNC_STATUS, 1, NULL, &ready);

    if (ready == GL_SIGNALED)
    {
      texture->transfer_sync = 0;
      glDeleteSync(texture->transfer_sync);
    }
    else
    {
      WHITE_TEXTURE.bind_for_sampling_at(binding);
      return false;
    }
  }
  glActiveTexture(GL_TEXTURE0 + binding);
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  // glBindTextureUnit(binding, texture->texture);
  check_set_parameters();
  return true;
}
GLuint Texture::get_handle()
{
  load();
  return texture ? texture->texture : 0;
}

Mesh::Mesh() {}

Mesh::Mesh(const Mesh_Descriptor &d) : Mesh(&d) {}
Mesh::Mesh(const Mesh_Descriptor *d)
{
  name = d->name;

  static uint64 count = 0;
  count += d->mesh_data.positions.size();
  set_message("Mesh(Mesh_Descriptor) ASSERT perf warning - hashing data", s(name, " ", count), 1.0f);
  std::string unique_identifier = d->mesh_data.build_unique_identifier();

  shared_ptr<Mesh_Handle> ptr = MESH_CACHE[unique_identifier].lock();
  if (ptr)
  {
    mesh = ptr;
    return;
  }
  mesh = make_shared<Mesh_Handle>();
  mesh->descriptor = *d;
  mesh->descriptor.unique_identifier = unique_identifier;
  MESH_CACHE[unique_identifier] = mesh;
}

void Mesh::load()
{
  if (!mesh->vao)
  {
    mesh->upload_data();
  }
}

void Mesh::draw()
{
  load();
  mesh->enable_assign_attributes(); // also binds vao
  if (!mesh->vao)
  {
    set_message("warning: draw called on mesh with no vao", "", 5.f);
    return;
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_buffer);
  glDrawElements(GL_TRIANGLES, mesh->indices_buffer_size, GL_UNSIGNED_INT, nullptr);
}

void Mesh_Handle::enable_assign_attributes()
{
  if (!vao)
    return;
  glBindVertexArray(vao);

  if (position_buffer)
  {
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  if (normal_buffer)
  {
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  if (uv_buffer)
  {
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
  }

  if (tangents_buffer)
  {
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, tangents_buffer);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  if (bitangents_buffer)
  {
    glEnableVertexAttribArray(4);
    glBindBuffer(GL_ARRAY_BUFFER, bitangents_buffer);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  if (bone_indices_buffer)
  {
    glEnableVertexAttribArray(5);
    glBindBuffer(GL_ARRAY_BUFFER, bone_indices_buffer);
    glVertexAttribIPointer(5, 4, GL_UNSIGNED_INT, GL_FALSE, 0);

    glEnableVertexAttribArray(6);
    glBindBuffer(GL_ARRAY_BUFFER, bone_weight_buffer);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 0, 0);

    ASSERT(6 < GL_MAX_VERTEX_ATTRIBS);
    // glEnableVertexAttribArray(5);
    // glVertexAttribPointer(5, 4, GL_INT, GL_FALSE, stride, 0);

    // glEnableVertexAttribArray(6);
    // glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void *)(4 * sizeof(int32)));

    /*





    When an array indexing expression, including struct field member accesses, results in an opaque types, the standard
has special requirements on those array indices. Under GLSL version 3.30, Sampler arrays (the only opaque type 3.30
provides) can be declared, but they can only be accessed by compile-time integral Constant Expressions. So you cannot
loop over an array of samplers, no matter what the array initializer, offset and comparison expressions are.

Under GLSL 4.00 and above, array indices leading to an opaque value can be accessed by non-compile-time constants, but
these index values must be dynamically uniform. The value of those indices must be the same value, in the same execution
order, regardless of any non-uniform parameter values, for all shader invocations in the invocation group.

For example, in 4.00, it is legal to loop over an array of samplers, so long as the loop index is based on constants and
uniforms. So this is legal:




    */
  }
}

void Mesh_Handle::upload_data()
{

  if (descriptor.mesh_data.positions.size() == 0)
    return;

  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  ASSERT(vao == 0);
  Mesh_Data &mesh_data = descriptor.mesh_data;

  if (sizeof(decltype(mesh_data.indices)::value_type) != sizeof(uint32))
  {
    set_message("Mesh::upload_data error: render() assumes index type to be 32 bits");
    ASSERT(0);
  }

  indices_buffer_size = mesh_data.indices.size();
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  set_message("uploading mesh data...", s("created vao: ", vao), 1);
  glGenBuffers(1, &position_buffer);
  glGenBuffers(1, &normal_buffer);
  glGenBuffers(1, &indices_buffer);
  glGenBuffers(1, &uv_buffer);
  glGenBuffers(1, &tangents_buffer);
  glGenBuffers(1, &bitangents_buffer);

  uint32 positions_buffer_size = uint32(mesh_data.positions.size());
  uint32 normal_buffer_size = uint32(mesh_data.normals.size());
  uint32 uv_buffer_size = uint32(mesh_data.texture_coordinates.size());
  uint32 tangents_size = uint32(mesh_data.tangents.size());
  uint32 bitangents_size = uint32(mesh_data.bitangents.size());
  uint32 indices_buffer_size = uint32(mesh_data.indices.size());
  ASSERT(all_equal(positions_buffer_size, normal_buffer_size, uv_buffer_size, tangents_size, bitangents_size));

  // positions
  uint32 buffer_size = (uint32)mesh_data.positions.size() * (uint32)sizeof(decltype(mesh_data.positions)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.positions[0], GL_STATIC_DRAW);

  // normals
  buffer_size = (uint32)mesh_data.normals.size() * (uint32)sizeof(decltype(mesh_data.normals)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.normals[0], GL_STATIC_DRAW);

  // texture_coordinates
  buffer_size = (uint32)mesh_data.texture_coordinates.size() *
                (uint32)sizeof(decltype(mesh_data.texture_coordinates)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.texture_coordinates[0], GL_STATIC_DRAW);

  // indices
  buffer_size = (uint32)mesh_data.indices.size() * (uint32)sizeof(decltype(mesh_data.indices)::value_type);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer_size, &mesh_data.indices[0], GL_STATIC_DRAW);

  // tangents
  buffer_size = (uint32)mesh_data.tangents.size() * (uint32)sizeof(decltype(mesh_data.tangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, tangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.tangents[0], GL_STATIC_DRAW);

  // bitangents
  buffer_size = (uint32)mesh_data.bitangents.size() * (uint32)sizeof(decltype(mesh_data.bitangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, bitangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.bitangents[0], GL_STATIC_DRAW);

  if (mesh_data.bone_weights.size())
  {

    size_t vertex_count = mesh_data.bone_weights.size();

    ASSERT(vertex_count == mesh_data.positions.size());

    buffer_size = vertex_count * sizeof(Vertex_Bone_Data);

    // sponge repack this for cache

    std::vector<uvec4> temp1;
    std::vector<vec4> temp2;
    temp1.resize(vertex_count);
    temp2.resize(vertex_count);

    uint32 max_index = 0;
    uint32 min_index = 100000;

    for (size_t i = 0; i < vertex_count; ++i)
    {
      uvec4 *a = (uvec4 *)&mesh_data.bone_weights[i].indices[0];
      temp1[i] = *a;

      // temp1[i] = ivec4(5);

      max_index = max(max_index, a->r);
      max_index = max(max_index, a->g);
      max_index = max(max_index, a->b);
      max_index = max(max_index, a->a);

      min_index = min(min_index, a->r);
      min_index = min(min_index, a->g);
      min_index = min(min_index, a->b);
      min_index = min(min_index, a->a);

      float32 *b = &mesh_data.bone_weights[i].weights[0];
      vec4 weight_v = vec4(*b, *(b + 1), *(b + 2), *(b + 3));

      float32 sum = weight_v.x + weight_v.y + weight_v.z + weight_v.w;
      ASSERT(sum < 1.1f || sum > -0.1f);

      // the shader says its reading values out of range of max bones..............
      // yet we upload them right here

      temp2[i] = weight_v;
    }

    int a = sizeof(uvec4);
    int b = sizeof(vec4);

    if (max_index > MAX_BONES || min_index < 0)
    {
      ASSERT(0);
    }

    glGenBuffers(1, &bone_indices_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, bone_indices_buffer);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(uint32) * 4, &(temp1[0]), GL_STATIC_DRAW);

    glGenBuffers(1, &bone_weight_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, bone_weight_buffer);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(float32) * 4, &(temp2[0]), GL_STATIC_DRAW);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

Material::Material() {}
Material::Material(Material_Descriptor &m)
{
  descriptor = m;

  static const Material_Descriptor default_md;
  descriptor.albedo.format = GL_SRGB8_ALPHA8;
  descriptor.normal.format = GL_RGBA8;
  descriptor.emissive.format = GL_SRGB8_ALPHA8;
  descriptor.roughness.format = GL_R8;
  descriptor.metalness.format = GL_R8;
  descriptor.ambient_occlusion.format = GL_R8;
  descriptor.displacement.format = GL_R8;

  albedo = Texture(descriptor.albedo);
  normal = Texture(descriptor.normal);
  emissive = Texture(descriptor.emissive);
  roughness = Texture(descriptor.roughness);
  metalness = Texture(descriptor.metalness);
  ambient_occlusion = Texture(descriptor.ambient_occlusion);
  displacement = Texture(descriptor.displacement);

  if (normal.t.source == "default")
  {
    normal.t.mod = DEFAULT_NORMAL;
    descriptor.normal.mod = DEFAULT_NORMAL;
  }

  if (roughness.t.source == "default")
  {
    roughness.t.mod = DEFAULT_ROUGHNESS;
    descriptor.roughness.mod = DEFAULT_ROUGHNESS;
  }

  if (metalness.t.source == "default")
  {
    metalness.t.mod = DEFAULT_METALNESS;
    descriptor.metalness.mod = DEFAULT_METALNESS;
  }

  if (emissive.t.source == "default")
  {
    emissive.t.mod = DEFAULT_EMISSIVE;
    descriptor.emissive.mod = DEFAULT_EMISSIVE;
  }

  if (displacement.t.source == "default")
  {
    displacement.t.mod = DEFAULT_DISPLACEMENT;
    descriptor.displacement.mod = DEFAULT_DISPLACEMENT;
  }

  if (descriptor.vertex_shader == "")
  {
    descriptor.vertex_shader = DEFAULT_VERTEX_SHADER;
  }

  if (descriptor.frag_shader == "")
  {
    descriptor.frag_shader = DEFAULT_FRAG_SHADER;
  }
  shader = Shader(descriptor.vertex_shader, descriptor.frag_shader);
}

void Material::load()
{

  static const Material_Descriptor default_md;

  descriptor.albedo.format = GL_SRGB8_ALPHA8;
  descriptor.normal.format = GL_RGBA8;
  descriptor.emissive.format = GL_SRGB8_ALPHA8;
  descriptor.roughness.format = GL_R8;
  descriptor.metalness.format = GL_R8;
  descriptor.ambient_occlusion.format = GL_R8;
  descriptor.displacement.format = GL_R8;

  Texture newalbedo(descriptor.albedo);
  newalbedo.load();
  albedo = newalbedo;

  Texture newnormal(descriptor.normal);
  newnormal.load();
  normal = newnormal;

  Texture newemissive(descriptor.emissive);
  newemissive.load();
  emissive = newemissive;

  Texture newroughness(descriptor.roughness);
  newroughness.load();
  roughness = newroughness;

  Texture newmetalness(descriptor.metalness);
  newmetalness.load();
  metalness = newmetalness;

  Texture newambient_occlusion(descriptor.ambient_occlusion);
  newambient_occlusion.load();
  ambient_occlusion = newambient_occlusion;

  Texture newdisplacement(descriptor.displacement);
  newdisplacement.load();
  displacement = newdisplacement;

  if (normal.t.source == "default")
  {
    normal.t.mod = DEFAULT_NORMAL;
    descriptor.normal.mod = DEFAULT_NORMAL;
  }

  if (roughness.t.source == "default")
  {
    roughness.t.mod = DEFAULT_ROUGHNESS;
    descriptor.roughness.mod = DEFAULT_ROUGHNESS;
  }

  if (metalness.t.source == "default")
  {
    metalness.t.mod = DEFAULT_METALNESS;
    descriptor.metalness.mod = DEFAULT_METALNESS;
  }

  if (emissive.t.source == "default")
  {
    emissive.t.mod = DEFAULT_EMISSIVE;
    descriptor.emissive.mod = DEFAULT_EMISSIVE;
  }

  if (displacement.t.source == "default")
  {
    displacement.t.mod = DEFAULT_DISPLACEMENT;
    descriptor.displacement.mod = DEFAULT_DISPLACEMENT;
  }

  if (descriptor.vertex_shader == "")
  {
    descriptor.vertex_shader = DEFAULT_VERTEX_SHADER;
  }

  if (descriptor.frag_shader == "")
  {
    descriptor.frag_shader = DEFAULT_FRAG_SHADER;
  }
  descriptor.derivative_offset = default_md.derivative_offset;
  shader = Shader(descriptor.vertex_shader, descriptor.frag_shader);
  reload_from_descriptor = false;
}
void Material::bind()
{
  if (reload_from_descriptor)
    load();

  if (descriptor.backface_culling)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  shader.use();
  shader.set_uniform("discard_on_alpha", descriptor.discard_on_alpha);
  shader.set_uniform("include_ao_in_uv_scale", descriptor.include_ao_in_uv_scale);
  shader.set_uniform("discard_threshold", descriptor.discard_threshold);
  shader.set_uniform("dielectric_reflectivity", descriptor.dielectric_reflectivity);
  shader.set_uniform("index_of_refraction", descriptor.ior);

  // if (shader.vs == "displacement.vert")
  {
    shader.set_uniform("derivative_offset", descriptor.derivative_offset);
    // ASSERT(descriptor.derivative_offset != 0.0);
  }

  bool success = albedo.bind_for_sampling_at(Texture_Location::albedo);
  shader.set_uniform("texture0_mod", albedo.t.mod);

  success = emissive.bind_for_sampling_at(Texture_Location::emissive);
  shader.set_uniform("texture1_mod", emissive.t.mod);
  if (!success)
  {
    shader.set_uniform("texture1_mod", DEFAULT_EMISSIVE);
  }

  success = roughness.bind_for_sampling_at(Texture_Location::roughness);
  shader.set_uniform("texture2_mod", roughness.t.mod);
  if (!success)
  {
    shader.set_uniform("texture2_mod", DEFAULT_ROUGHNESS);
  }

  success = normal.bind_for_sampling_at(Texture_Location::normal);
  shader.set_uniform("texture3_mod", normal.t.mod);
  if (!success)
  {
    shader.set_uniform("texture3_mod", DEFAULT_NORMAL);
  }

  success = metalness.bind_for_sampling_at(Texture_Location::metalness);
  shader.set_uniform("texture4_mod", metalness.t.mod * metalness.t.mod);
  if (!success)
  {
    shader.set_uniform("texture4_mod", DEFAULT_METALNESS);
  }

  success = displacement.bind_for_sampling_at(Texture_Location::displacement);
  shader.set_uniform("texture11_mod", displacement.t.mod);
  if (!success)
  {
    shader.set_uniform("texture11_mod", DEFAULT_DISPLACEMENT);
  }

  success = ambient_occlusion.bind_for_sampling_at(Texture_Location::ambient_occlusion);
  shader.set_uniform("texture5_mod", ambient_occlusion.t.mod);

  shader.set_uniform("multiply_albedo_by_a", descriptor.multiply_albedo_by_alpha);
  shader.set_uniform("multiply_result_by_a", descriptor.multiply_result_by_alpha);
  shader.set_uniform("multiply_pixelalpha_by_moda", descriptor.multiply_pixelalpha_by_moda);

  for (auto &uniform : descriptor.uniform_set.float32_uniforms)
  {
    shader.set_uniform(uniform.first.c_str(), uniform.second);
  }
  for (auto &uniform : descriptor.uniform_set.int32_uniforms)
  {
    shader.set_uniform(uniform.first.c_str(), uniform.second);
  }
  for (auto &uniform : descriptor.uniform_set.uint32_uniforms)
  {
    shader.set_uniform(uniform.first.c_str(), uniform.second);
  }
  for (auto &uniform : descriptor.uniform_set.bool_uniforms)
  {
    shader.set_uniform(uniform.first.c_str(), uniform.second);
  }
  for (auto &uniform : descriptor.uniform_set.vec2_uniforms)
  {
    shader.set_uniform(uniform.first.c_str(), uniform.second);
  }
  for (auto &uniform : descriptor.uniform_set.vec3_uniforms)
  {
    shader.set_uniform(uniform.first.c_str(), uniform.second);
  }
  for (auto &uniform : descriptor.uniform_set.vec4_uniforms)
  {
    shader.set_uniform(uniform.first.c_str(), uniform.second);
  }
  for (auto &uniform : descriptor.uniform_set.mat4_uniforms)
  {
    shader.set_uniform(uniform.first.c_str(), uniform.second);
  }
  for (auto &uniform : descriptor.uniform_set.texture_uniforms)
  {
    uniform.second.bind_for_sampling_at(uniform.first);
  }
}

Render_Entity::Render_Entity(Array_String n, Mesh *mesh, Material *material, mat4 world_to_model, Node_Index node_index)
    : mesh(mesh), material(material), transformation(world_to_model), name(n), node(node_index)
{
  ASSERT(mesh);
  ASSERT(material);
}
Render_Entity::Render_Entity(Array_String n, Mesh *mesh, Material *material, Skeletal_Animation_State *animation,
    mat4 world_to_model, Node_Index node_index)
    : mesh(mesh), material(material), animation(animation), transformation(world_to_model), name(n), node(node_index)
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
  Mesh_Descriptor md(plane, "mesh primitive plane");
  quad = Mesh(md);

  cube = Mesh({Mesh_Primitive::cube, "mesh primitive cube"});

  Texture_Descriptor uvtd;
  uvtd.name = "uvgrid.png";
  uv_map_grid = uvtd;

  const ivec2 brdf_lut_size = ivec2(512, 512);
  Shader brdf_lut_generator = Shader("passthrough.vert", "brdf_lut_generator.frag");
  brdf_integration_lut = Texture("brdf_lut", brdf_lut_size, 1, GL_RG16F, GL_LINEAR);
  Framebuffer lut_fbo;
  lut_fbo.color_attachments[0] = brdf_integration_lut;
  lut_fbo.init();
  lut_fbo.bind_for_drawing_dst();
  auto mat = fullscreen_quad();
  glViewport(0, 0, brdf_lut_size.x, brdf_lut_size.y);
  brdf_lut_generator.set_uniform("transform", mat);
  set_message("drawing brdf lut..", "", 1.0f);
  quad.draw();
  save_texture(&brdf_integration_lut, "brdflut");

  Texture_Descriptor white_td;

  WHITE_TEXTURE.t.key = "default";
  bind_white_to_all_textures();
  set_message("Renderer init finished");
  init_render_targets();

  FRAME_TIMER.start();
}

void Renderer::bind_white_to_all_textures()
{
  for (uint32 i = 0; i <= Texture_Location::displacement; ++i)
  {
    WHITE_TEXTURE.bind_for_sampling_at(Texture_Location(i));
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
      spotlight_shadow_maps[i].blur.target.color_attachments[0].bind_for_sampling_at(Texture_Location::s0 + i);

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

void blit_attachment(Framebuffer &src, Framebuffer &dst)
{
  glBlitNamedFramebuffer(src.fbo->fbo, dst.fbo->fbo, 0, 0, src.color_attachments[0].t.size.x,
      src.color_attachments[0].t.size.y, 0, 0, dst.color_attachments[0].t.size.x, dst.color_attachments[0].t.size.y,
      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

mat4 fullscreen_quad()
{
  return scale(vec3(2, 2, 1));
}

void draw_texture_handle_info(shared_ptr<Texture_Handle> ptr)
{
  if (!ptr.get())
  {
    ImGui::Text("No texture");
    return;
  }
  uint32 addr = (uint32)ptr.get();
  std::stringstream ss;
  ss << std::hex << addr;
  std::string str = ss.str();
  ImGui::Text(s("Heap Address:", str).c_str());
  ImGui::SameLine();
  if (ImGui::Button("Copy Handle"))
  {
    COPIED_TEXTURE_HANDLE = ptr;
  }
  ImGui::Text(s("Ptr Refcount:", (uint32)ptr.use_count()).c_str());
  ImGui::Text(s("OpenGL handle:", ptr->texture).c_str());

  const char *min_filter_result = filter_format_to_string(ptr->minification_filter);
  const char *mag_filter_result = filter_format_to_string(ptr->magnification_filter);
  bool min_good = strcmp(min_filter_result, filter_format_to_string(GL_LINEAR_MIPMAP_LINEAR)) == 0;
  bool mag_good = strcmp(mag_filter_result, filter_format_to_string(GL_LINEAR)) == 0;
  ImGui::Text("Minification filter:");
  ImGui::SameLine();
  if (min_good)
  {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), min_filter_result);
  }
  else
  {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), min_filter_result);
  }

  ImGui::Text("Magnification filter:");
  ImGui::SameLine();
  if (mag_good)
  {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), mag_filter_result);
  }
  else
  {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), mag_filter_result);
  }

  ImGui::Text("Format:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1, 0, 1, 1), texture_format_to_string(ptr->internalformat));

  if (ptr->is_cubemap)
  {
    ImGui::Text("Type:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 0, 1, 1), "Cubemap");
  }
  else
  {
    ImGui::Text("Type:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 0, 1, 1), "Texture2D");
  }
  ImGui::Text(s("Size:", ptr->size.x, "x", ptr->size.y).c_str());
  Imgui_Texture_Descriptor descriptor;
  descriptor.ptr = ptr;
  GLenum format = descriptor.ptr->get_format();

  ImGui::Checkbox("Use alpha in thumbnail", &ptr->imgui_use_alpha);
  ImGui::SetNextItemWidth(200);
  ImGui::DragFloat("Thumbnail Size", &ptr->imgui_size_scale, 0.02f);
  uint32 mip_levels = ptr->levels;
  ImGui::Text(s("Mipmap levels: ", mip_levels).c_str());
  ImGui::SetNextItemWidth(200);
  ImGui::SliderFloat("Mipmap LOD", &ptr->imgui_mipmap_setting, 0.f, float32(mip_levels), "%.2f");
  descriptor.mip_lod_to_draw = ptr->imgui_mipmap_setting;
  descriptor.size = ptr->imgui_size_scale * vec2(256);
  descriptor.y_invert = false;
  descriptor.use_alpha = ptr->imgui_use_alpha;
  put_imgui_texture(&descriptor);

  if (ImGui::TreeNode("List Mipmaps"))
  {
    for (uint32 i = 0; i < mip_levels; ++i)
    {
      Imgui_Texture_Descriptor d = descriptor;
      d.mip_lod_to_draw = (float)i;
      d.is_mipmap_list_command = true;
      put_imgui_texture(&d);
    }
    ImGui::TreePop();
  }
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
    ImGui::SameLine();
    if (ImGui::Button("Reload Shaders"))
    {
      for (std::pair<const std::string, std::weak_ptr<Shader_Handle>> &handle : SHADER_CACHE)
      {
        std::string key = handle.first;
        auto ptr = SHADER_CACHE[key].lock();
        if (!ptr)
          continue;

        std::string vs = ptr->vs;
        std::string fs = ptr->fs;
        GLuint program = load_shader(vs, fs);

        if (!program)
          continue;
        Shader_Handle blank(0);
        *ptr = blank;
        ptr->program = program;
        ptr->vs = vs;
        ptr->fs = fs;
        glUseProgram(program);
        ptr->set_location_cache();
      }
    }

    if (ImGui::Button("Tonemapping"))
    {
      show_tonemap = !show_tonemap;
    }
    ImGui::Checkbox("Depth Prepass", &do_depth_prepass);

    static float32 tempscale = render_scale;
    ImGui::SliderFloat("scale", &tempscale, 0.05f, 2.f);
    static float32 tempfov = vfov;
    ImGui::SliderFloat("fov", &tempfov, 0.f, 360.f);
    set_render_scale(tempscale);
    set_vfov(tempfov);
    if (show_tonemap)
    {
      ImGui::Begin("Tonemapping", &show_tonemap);
      ImGui::DragFloat("Exposure", &exposure, 0.0001f, 0.f, 20.f, "%.3f", 2.5f);
      ImGui::ColorPicker3("Exposure Color", &exposure_color[0]);
      ImGui::InputInt("tonemapper", &selected_tonemap_function);

      ImGui::End();
    }
    if (ImGui::CollapsingHeader("Copied Texture"))
    {
      draw_texture_handle_info(COPIED_TEXTURE_HANDLE);
    }
    if (ImGui::CollapsingHeader("GPU Textures"))
    {
      ImGui::Indent(5);
      vector<Imgui_Texture_Descriptor> imgui_texture_array;
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
            if (a.ptr->peek_filename() == b.ptr->peek_filename())
            {
              return a.ptr->texture < b.ptr->texture;
            }

            return a.ptr->peek_filename().compare(b.ptr->peek_filename()) < 0;
          });
      for (uint32 i = 0; i < imgui_texture_array.size(); ++i)
      {
        Imgui_Texture_Descriptor *itd = &imgui_texture_array[i];
        shared_ptr<Texture_Handle> ptr = itd->ptr;

        const char *label = ptr->peek_filename().c_str();
        ImGui::PushID(s(ptr->texture, label).c_str());
        bool popped = false;
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        ImGuiID id = window->GetID(label);
        bool node_is_open = ImGui::TreeNodeBehaviorIsOpen(id, ImGuiTreeNodeFlags_Framed);
        if (node_is_open)
        {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 255, 0, 1));
        }
        else
        {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 1));
        }
        if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Framed))
        {
          ImGui::PopStyleColor();
          popped = true;
          float32 indent_width = 25;
          ImGui::Indent(indent_width);
          draw_texture_handle_info(ptr);
          ImGui::TreePop();
          ImGui::Unindent(indent_width);
        }
        if (!popped)
          ImGui::PopStyleColor();
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
  Mesh *previous_mesh = nullptr;
  Shader *previous_shader = nullptr;
  int32 previous_light_index = -1;
  uint32 mesh_saves = 0;

  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    spotlight_shadow_maps[i].enabled = false;
  }
  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    if (!(i < lights.light_count))
      break;

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
    ivec2 shadow_map_size = CONFIG.shadow_map_scale * vec2(light->shadow_map_resolution);
    shadow_map->init(shadow_map_size);
    shadow_map->blur.target.color_attachments[0].bind_for_sampling_at(0);

    glTextureParameteri(
        shadow_map->blur.target.color_attachments[0].texture->texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glViewport(0, 0, shadow_map_size.x, shadow_map_size.y);
    glEnable(GL_CULL_FACE);
    // glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
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
        if (entity.material->displacement.t.source != "default")
        {
          if (previous_shader != &variance_shadow_map)
          {
            variance_shadow_map_displacement.use();
            previous_shader = &variance_shadow_map_displacement;
          }
          bool displacement_success =
              entity.material->displacement.bind_for_sampling_at(Texture_Location::displacement);
          variance_shadow_map_displacement.set_uniform(
              "transform", shadow_map->projection_camera * entity.transformation);
          // sponge: jank code specific to the world sim thing:
          bool is_ground = entity.name == "ground";
          variance_shadow_map_displacement.set_uniform("ground", is_ground);
          variance_shadow_map_displacement.set_uniform("displacement_map_size", uint32(HEIGHTMAP_RESOLUTION));
          // variance_shadow_map_displacement.set_uniform("texture11_mod",
          // entity.material->descriptor.displacement.mod);
          // entity.material->displacement.bind_for_sampling_at(displacement);

          if (entity.mesh != previous_mesh)
          {
            entity.mesh->load();
            entity.mesh->mesh->enable_assign_attributes(); // also binds vao
            if (!entity.mesh->mesh->vao)
            {
              set_message("warning: draw called on mesh with no vao", "", 5.f);
              continue;
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->mesh->indices_buffer);
            previous_mesh = entity.mesh;
          }
          else
          {
            mesh_saves += 1;
          }
          glDrawElements(GL_TRIANGLES, entity.mesh->mesh->indices_buffer_size, GL_UNSIGNED_INT, nullptr);
        }
        else
        {
          if (previous_shader != &variance_shadow_map)
          {
            variance_shadow_map.use();
            previous_shader = &variance_shadow_map;
          }
          variance_shadow_map.set_uniform("transform", shadow_map->projection_camera * entity.transformation);
          ASSERT(entity.mesh);
          if (entity.mesh != previous_mesh)
          {
            entity.mesh->load();
            entity.mesh->mesh->enable_assign_attributes();
            if (!entity.mesh->mesh->vao)
            {
              set_message("warning: draw called on mesh with no vao", "", 5.f);
              continue;
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->mesh->indices_buffer);
            previous_mesh = entity.mesh;
          }
          else
          {
            mesh_saves += 1;
          }
          glDrawElements(GL_TRIANGLES, entity.mesh->mesh->indices_buffer_size, GL_UNSIGNED_INT, nullptr);
        }
      }
    }

    shadow_map->blur.draw(
        this, &shadow_map->pre_blur.color_attachments[0], light->shadow_blur_radius, light->shadow_blur_iterations);

    shadow_map->blur.target.color_attachments[0].bind_for_sampling_at(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  set_message("shadow map mesh bind saves:", s(mesh_saves), 1.0f);
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

  static Mesh_Descriptor d(plane, "run_pixel_shader_quad");
  static Mesh quad = Mesh(d);

  if (dst)
  {
    ASSERT(dst->fbo);
    ASSERT(dst->color_attachments.size());
    dst->bind_for_drawing_dst();
  }
  else
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

  for (uint32 i = 0; i < src_textures->size(); ++i)
  {
    (*src_textures)[i]->bind_for_sampling_at(i);
  }
  ivec2 viewport_size = dst->color_attachments[0].t.size;
  glViewport(0, 0, viewport_size.x, viewport_size.y);

  if (clear_dst)
  {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE); /*
   glFrontFace(GL_CCW);
   glCullFace(GL_BACK);*/

  // glBindVertexArray(quad.get_vao());
  shader->use();
  shader->set_uniform("transform", fullscreen_quad());
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

void Renderer::opaque_pass(float64 time)
{

  // set_message("opaque_pass()");

  glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  // todo: depth pre-pass

  // set_message("opaque_pass projection:", s(projection), 1.0f);
  // set_message("opaque_pass camera:", s(camera), 1.0f);
  bind_white_to_all_textures();
  environment.bind(Texture_Location::environment, Texture_Location::irradiance, float32(time), render_target_size);
  brdf_integration_lut.bind_for_sampling_at(brdf_ibl_lut);
  glViewport(0, 0, render_target_size.x, render_target_size.y);

  if (do_depth_prepass)
  {

    glDrawBuffer(GL_NONE);
    // depth pass
    for (Render_Entity &entity : render_entities)
    {
      if (entity.material->descriptor.wireframe)
      {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      }
      entity.material->bind();
      Shader &shader = entity.material->shader;
      shader.set_uniform("time", (float32)time);
      shader.set_uniform("txaa_jitter", txaa_jitter);
      shader.set_uniform("camera_position", camera_position);
      shader.set_uniform("MVP", projection * camera * entity.transformation);
      shader.set_uniform("Model", entity.transformation);
      shader.set_uniform("alpha_albedo_override", -1.0f); //-1 is disabledX
      entity.mesh->draw();
      if (entity.material->descriptor.wireframe)
      {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      }
    }
    glDrawBuffers((uint32)draw_target.draw_buffers.size(), &draw_target.draw_buffers[0]);
    glDepthFunc(GL_EQUAL);
  }

  Material *previous_material = nullptr;
  Mesh *previous_mesh = nullptr;

  uint32 mat_saves = 0;
  uint32 mesh_saves = 0;
  environment.bind(Texture_Location::environment, Texture_Location::irradiance, float32(time), render_target_size);
  brdf_integration_lut.bind_for_sampling_at(brdf_ibl_lut);
  for (Render_Entity &entity : render_entities)
  {
    ASSERT(entity.mesh);
    Shader &shader = entity.material->shader;

    if ( entity.material != previous_material)
    {
      entity.material->bind();
      previous_material = entity.material;
      shader.set_uniform("time", (float32)time);
      shader.set_uniform("txaa_jitter", txaa_jitter);
      shader.set_uniform("camera_position", camera_position);
      shader.set_uniform("uv_scale", entity.material->descriptor.uv_scale);
      shader.set_uniform("normal_uv_scale", entity.material->descriptor.normal_uv_scale);
      lights.bind(shader);
      set_uniform_shadowmaps(shader);
      if (entity.material->descriptor.depth_test)
      {
        glEnable(GL_DEPTH_TEST);
      }
      else
      {
        glDisable(GL_DEPTH_TEST);
      }

      if (entity.material->descriptor.depth_mask)
      {
        glDepthMask(GL_TRUE);
      }
      else
      {
        glDepthMask(GL_FALSE);
      }
      if (entity.material->descriptor.wireframe)
      {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      }
      else
      {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      }
    }
    else
    {
      mat_saves += 1;

    }

    if (entity.material->shader.vs == "skeletal_animation.vert")
    {
      ASSERT(entity.animation);
      shader.set_uniform("VP", projection * camera);
    }
    else
    {
      shader.set_uniform("MVP", projection * camera * entity.transformation);
    }
    shader.set_uniform("Model", entity.transformation);

    if (entity.animation)
    {
      ASSERT(shader.vs == "skeletal_animation.vert");
      Skeletal_Animation_State *animation = entity.animation;
      std::vector<Bone> *bones = &entity.mesh->mesh->descriptor.mesh_specific_bones;
      ASSERT(bones->size() < MAX_BONES);
      for (size_t i = 0; i < bones->size(); ++i)
      {
        Bone *to_bind = &(*bones)[i];
        mat4 *pose = &animation->final_bone_transforms[to_bind->name];
        //*pose = mat4(1);
        std::string bone_uniform_name = s("bones", "[", i, "]");
        shader.set_uniform(bone_uniform_name.c_str(), *pose);
      }
    }
    else
    {
      if (shader.vs == "skeletal_animation.vert")
      {
        static mat4 identity[MAX_BONES] = {mat4(1)};
        shader.set_uniform_array("bones[0]", &identity[0], MAX_BONES);
      }
    }

    // environment.bind(Texture_Location::environment, Texture_Location::irradiance, time, render_target_size);

    if (entity.mesh != previous_mesh)
    {
      entity.mesh->load();
      entity.mesh->mesh->enable_assign_attributes(); // also binds vao
      if (!entity.mesh->mesh->vao)
      {
        set_message("warning: draw called on mesh with no vao", "", 5.f);
        continue;
      }
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->mesh->indices_buffer);
      previous_mesh = entity.mesh;
    }
    else
    {
      mesh_saves += 1;
    }
    glDrawElements(GL_TRIANGLES, entity.mesh->mesh->indices_buffer_size, GL_UNSIGNED_INT, nullptr);
  }

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDepthFunc(GL_LESS);
  bind_white_to_all_textures();

  set_message("material bind saves:", s(mat_saves), 1.0f);
  set_message("mesh bind saves:", s(mesh_saves), 1.0f);
}

void Renderer::instance_pass(float64 time)
{
  glDisable(GL_BLEND);
  vec3 forward_v = normalize(camera_gaze - camera_position);
  vec3 right_v = normalize(cross(forward_v, {0, 0, 1}));
  vec3 up_v = -normalize(cross(forward_v, right_v));

  // set_message("forward_v", s(forward_v), 1.0f);
  // set_message("right_v", s(right_v), 1.0f);
  // set_message("up_v", s(up_v), 1.0f);

  if (!use_txaa)
  {
    previous_draw_target.color_attachments[0].t.wrap_s = GL_CLAMP_TO_EDGE;
    previous_draw_target.color_attachments[0].t.wrap_t = GL_CLAMP_TO_EDGE;
    previous_draw_target.color_attachments[0].bind_for_sampling_at(0); // will set the wraps
    previous_draw_target.bind_for_drawing_dst();
    glViewport(0, 0, render_target_size.x, render_target_size.y);
    passthrough.use();
    draw_target.color_attachments[0].bind_for_sampling_at(0);
    passthrough.set_uniform("transform", fullscreen_quad());
    quad.draw();
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.get_indices_buffer());
    // glDrawElements(GL_TRIANGLES, quad.get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0);
    glBindTexture(GL_TEXTURE_2D, 0);
    draw_target.bind_for_drawing_dst();
  }
  glActiveTexture(GL_TEXTURE0 + Texture_Location::refraction);
  glBindTexture(GL_TEXTURE_2D, previous_draw_target.color_attachments[0].get_handle());

  environment.bind(Texture_Location::environment, Texture_Location::irradiance, float32(time), render_target_size);
  for (Render_Instance &entity : render_instances)
  {
    entity.material->bind();
    ASSERT(entity.mesh);
    Shader *shader = &entity.material->shader;
    ASSERT(shader->vs == "instance.vert");

    shader->set_uniform("time", (float32)time);
    shader->set_uniform("txaa_jitter", txaa_jitter);
    shader->set_uniform("camera_position", camera_position);
    shader->set_uniform("uv_scale", entity.material->descriptor.uv_scale);
    shader->set_uniform("normal_uv_scale", entity.material->descriptor.normal_uv_scale);
    shader->set_uniform("viewport_size", vec2(render_target_size));
    shader->set_uniform("aspect_ratio", render_target_size.x / render_target_size.y);
    shader->set_uniform("camera_forward", forward_v);
    shader->set_uniform("camera_right", right_v);
    shader->set_uniform("camera_up", up_v);
    shader->set_uniform("project", projection);
    shader->set_uniform("use_billboarding", entity.use_billboarding);
    lights.bind(*shader);
    set_uniform_shadowmaps(*shader);
    entity.mesh->load();

    glBindVertexArray(entity.mesh->get_vao());
    ASSERT(glGetAttribLocation(shader->program->program, "instanced_MVP") == 5);
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

    GLint instanced_model_location = glGetAttribLocation(shader->program->program, "instanced_model");
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

    int32 loc_billboard_pos = glGetAttribLocation(shader->program->program, "billboard_position");
    if (loc_billboard_pos != -1)
    {
      ASSERT(loc_billboard_pos == 13);
      glEnableVertexAttribArray(13);
      glBindBuffer(GL_ARRAY_BUFFER, entity.instance_billboard_location_buffer);
      glVertexAttribPointer(13, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)(0));
      glVertexAttribDivisor(13, 1);
    }

    if (entity.use_attribute0)
    {
      int32 loc0 = glGetAttribLocation(shader->program->program, "attribute0");
      if (loc0 != -1)
      {
        ASSERT(loc0 == 14);
        glEnableVertexAttribArray(14);
        glBindBuffer(GL_ARRAY_BUFFER, entity.attribute0_buffer);
        glVertexAttribPointer(14, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)(0));
        glVertexAttribDivisor(14, 1);
      }
    }

    if (entity.use_attribute1)
    {
      int32 loc1 = glGetAttribLocation(shader->program->program, "attribute1");
      if (loc1 != -1)
      {
        ASSERT(loc1 == 15);
        glEnableVertexAttribArray(15);
        glBindBuffer(GL_ARRAY_BUFFER, entity.attribute1_buffer);
        glVertexAttribPointer(15, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)(0));
        glVertexAttribDivisor(15, 1);
      }
    }
    if (entity.use_attribute2)
    {
      int32 loc2 = glGetAttribLocation(shader->program->program, "attribute2");
      if (loc2 != -1)
      {
        ASSERT(loc2 == 16);
        glEnableVertexAttribArray(16);
        glBindBuffer(GL_ARRAY_BUFFER, entity.attribute2_buffer);
        glVertexAttribPointer(16, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)(0));
        glVertexAttribDivisor(16, 1);
      }
    }
    if (entity.use_attribute3)
    {
      int32 loc2 = glGetAttribLocation(shader->program->program, "attribute2");
      if (loc2 != -1)
      {
        ASSERT(loc2 == 17);
        glEnableVertexAttribArray(17);
        glBindBuffer(GL_ARRAY_BUFFER, entity.attribute3_buffer);
        glVertexAttribPointer(17, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)(0));
        glVertexAttribDivisor(17, 1);
      }
    }

    if (entity.material->descriptor.require_self_depth)
    {
      // depth info for this object's back faces
      self_object_depth.bind_for_drawing_dst();
      glCullFace(GL_FRONT);
      glDrawBuffer(GL_NONE);
      glClear(GL_DEPTH_BUFFER_BIT);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
      glDrawElementsInstanced(
          GL_TRIANGLES, entity.mesh->get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0, entity.size);
      self_object_depth.depth_texture.bind_for_sampling_at(Texture_Location::depth_of_self);
      draw_target.bind_for_drawing_dst();
    }

    if (entity.material->descriptor.depth_test)
    {
      glEnable(GL_DEPTH_TEST);
    }
    else
    {
      glDisable(GL_DEPTH_TEST);
    }

    if (entity.material->descriptor.depth_mask)
    {
      glDepthMask(GL_TRUE);
    }
    else
    {
      glDepthMask(GL_FALSE);
    }
    if (entity.material->descriptor.blendmode == Material_Blend_Mode::blend)
    {
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (entity.material->descriptor.blendmode == Material_Blend_Mode::add)
    {
      glBlendFunc(GL_ONE, GL_ONE);
    }
    if (entity.material->descriptor.wireframe)
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (entity.material->descriptor.translucent_pass)
    {
      glEnable(GL_BLEND);
    }
    else
    {
      glDisable(GL_BLEND);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());
    glDrawElementsInstanced(
        GL_TRIANGLES, entity.mesh->get_indices_buffer_size(), GL_UNSIGNED_INT, (void *)0, entity.size);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    bind_white_to_all_textures();

    glDisableVertexAttribArray(6);
    glDisableVertexAttribArray(7);
    glDisableVertexAttribArray(8);
    glDisableVertexAttribArray(9);
    glDisableVertexAttribArray(10);
    glDisableVertexAttribArray(11);
    glDisableVertexAttribArray(12);
    glDisableVertexAttribArray(13);
    if (entity.use_attribute0)
    {
      glDisableVertexAttribArray(14);
    }
    if (entity.use_attribute1)
    {
      glDisableVertexAttribArray(15);
    }
    if (entity.use_attribute2)
    {
      glDisableVertexAttribArray(16);
    }
    if (entity.use_attribute3)
    {
      glDisableVertexAttribArray(17);
    }
  }

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

// pseudocode for gaussian blur for textured roughness translucent object surfaces
// such as foggy glass windows

// possible to jitter sample based on roughness and average refraction map instead - slower

//
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

void Renderer::translucent_pass(float64 time)
{
  brdf_integration_lut.bind_for_sampling_at(brdf_ibl_lut);
  environment.bind(Texture_Location::environment, Texture_Location::irradiance, float32(time), render_target_size);

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);

  vec3 forward_v = normalize(camera_gaze - camera_position);
  vec3 right_v = normalize(cross(forward_v, {0, 0, 1}));
  vec3 up_v = -normalize(cross(forward_v, right_v));

  glNamedFramebufferDrawBuffers(translucent_sample_source.fbo->fbo, translucent_sample_source.draw_buffers.size(),
      &translucent_sample_source.draw_buffers[0]);
  glBlitNamedFramebuffer(draw_target.fbo->fbo, translucent_sample_source.fbo->fbo, 0, 0, render_target_size.x,
      render_target_size.y, 0, 0, render_target_size.x, render_target_size.y, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
      GL_NEAREST);

  bind_white_to_all_textures();
  draw_target.bind_for_drawing_dst();
  translucent_sample_source.color_attachments[0].bind_for_sampling_at(Texture_Location::refraction);
  translucent_sample_source.depth_texture.bind_for_sampling_at(Texture_Location::depth_of_scene);
  for (Render_Entity &entity : translucent_entities)
  {
    if (CONFIG.render_simple)
    { // not implemented here
      ASSERT(0);
    }
    ASSERT(entity.mesh);
    entity.material->bind();
    Shader *shader = &entity.material->shader;
    shader->set_uniform("camera_forward", forward_v);
    shader->set_uniform("camera_right", right_v);
    shader->set_uniform("camera_up", up_v);
    shader->set_uniform("time", (float32)time);
    shader->set_uniform("txaa_jitter", txaa_jitter);
    shader->set_uniform("camera_position", camera_position);
    shader->set_uniform("uv_scale", entity.material->descriptor.uv_scale);
    shader->set_uniform("MVP", projection * camera * entity.transformation);
    shader->set_uniform("Model", entity.transformation);
    shader->set_uniform("discard_on_alpha", false);
    shader->set_uniform("uv_scale", entity.material->descriptor.uv_scale);
    shader->set_uniform("normal_uv_scale", entity.material->descriptor.normal_uv_scale);
    shader->set_uniform("viewport_size", vec2(render_target_size));
    shader->set_uniform("aspect_ratio", render_target_size.x / render_target_size.y);

    lights.bind(*shader);
    set_uniform_shadowmaps(*shader);

    if (entity.material->descriptor.wireframe)
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glDepthMask(GL_FALSE);
    if (entity.material->descriptor.require_self_depth)
    { // depth info for this object's back faces
      glDepthMask(GL_TRUE);
      self_object_depth.bind_for_drawing_dst();
      glCullFace(GL_FRONT);
      glDrawBuffer(GL_NONE);
      glClear(GL_DEPTH_BUFFER_BIT);
      entity.mesh->draw();
      self_object_depth.depth_texture.bind_for_sampling_at(Texture_Location::depth_of_self);
      draw_target.bind_for_drawing_dst();
      glCullFace(GL_BACK);
      glDepthMask(GL_FALSE);
    }
    // depth of closest front face will be in the main rendering call below
    // depth of the world will be in translucent sample source - dont need to render to it
    // depth of closest back face is in self_object_depth

    // the translucent sample source does NOT have the depth info for other translucent objects, even their
    // front faces...

    // if the depth of the front face is close to the back face we can blend towards green

    draw_target.bind_for_drawing_dst();
    translucent_sample_source.color_attachments[0].bind_for_sampling_at(Texture_Location::refraction);
    translucent_sample_source.depth_texture.bind_for_sampling_at(Texture_Location::depth_of_scene);
    glNamedFramebufferDrawBuffers(draw_target.fbo->fbo, draw_target.draw_buffers.size(), &draw_target.draw_buffers[0]);
    glCullFace(GL_BACK);
    if (entity.material->descriptor.depth_test)
    {
      glEnable(GL_DEPTH_TEST);
    }
    else
    {
      glDisable(GL_DEPTH_TEST);
    }

    if (entity.material->descriptor.depth_mask)
    {
      glDepthMask(GL_TRUE);
    }
    else
    {
      glDepthMask(GL_FALSE);
    }
    if (entity.material->descriptor.blendmode == Material_Blend_Mode::blend)
    {
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (entity.material->descriptor.blendmode == Material_Blend_Mode::add)
    {
      glBlendFunc(GL_ONE, GL_ONE);
    }

    glEnable(GL_BLEND);
    entity.mesh->draw();

    if (true)
    { // this will copy back the result into the scene so there can be multi layers of refraction-enabled objects
      // might be too slow
      glBlitNamedFramebuffer(draw_target.fbo->fbo, translucent_sample_source.fbo->fbo, 0, 0, render_target_size.x,
          render_target_size.y, 0, 0, render_target_size.x, render_target_size.y,
          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
  }
  self_object_depth.bind_for_drawing_dst();
  glClear(GL_DEPTH_BUFFER_BIT);
  draw_target.bind_for_drawing_dst();
  bind_white_to_all_textures();
  glActiveTexture(GL_TEXTURE0 + Texture_Location::refraction);
  glBindTexture(GL_TEXTURE_2D, 0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

void Renderer::postprocess_pass(float64 time)
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
  bool need_to_allocate_textures = bloom_target.texture == nullptr;
  const float32 scale_relative_to_1080p = this->render_target_size.x / 1920.f;
  const int32 min_width = (int32)(80.f * scale_relative_to_1080p);
  vector<ivec2> resolutions = {render_target_size};
  while (resolutions.back().x / 2 > min_width)
  {
    resolutions.push_back(resolutions.back() / 2);
  }

  const uint32 levels = uint32(resolutions.size());
  const uint32 dst_mip_level_count = uint32(resolutions.size()) - 1;

  if (need_to_allocate_textures)
  {
    bloom_intermediate =
        Texture(name + s("'s Bloom Intermediate"), render_target_size, levels, GL_RGB16F, GL_LINEAR_MIPMAP_LINEAR);
    bloom_target = Texture(name + s("'s Bloom Target"), render_target_size, levels, GL_RGB16F, GL_LINEAR_MIPMAP_LINEAR);
    bloom_result = Texture(name + "'s bloom_result", render_target_size, 1, GL_RGB16F, GL_LINEAR, GL_LINEAR,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    // glTextureParameteri(bloom_intermediate.texture->texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // glTextureParameteri(bloom_target.texture->texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // for (uint32 i = 0; i < resolutions.size(); ++i)
    //{
    //  ivec2 resolution = resolutions[i];
    //  bloom_intermediate.bind(0);
    //  glTextureStorage2D(bloom_intermediate.texture->texture, 6, bloom_intermediate.texture->internalformat,
    //      bloom_intermediate.texture->size.x, bloom_intermediate.texture->size.y);
    //  glTexImage2D(GL_TEXTURE_2D, i, GL_RGB16F, resolution.x, resolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);
    //  bloom_target.bind(0);
    //  glTexImage2D(GL_TEXTURE_2D, i, GL_RGB16F, resolution.x, resolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);
    //}
  }
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);
  glDisable(GL_DEPTH_TEST);

  // apply high pass filter
  bloom_fbo.color_attachments[0] = bloom_target;
  bloom_fbo.init();
  bloom_fbo.bind_for_drawing_dst();
  high_pass_shader.use();
  high_pass_shader.set_uniform("transform", fullscreen_quad());
  glViewport(0, 0, render_target_size.x, render_target_size.y);
  draw_target.color_attachments[0].bind_for_sampling_at(0);
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
      ImGui::Text(s("mip_count:", dst_mip_level_count).c_str());
      ImGui::End();
    }
  }

  float32 original = blur_radius;
  // in a loop
  // blur: src:bloom_target, dst:intermediate (x), src:intermediate, dst:target (y)
  for (uint32 i = 0; i < dst_mip_level_count; ++i)
  {
    ivec2 resolution = resolutions[i + uint32(1)];
    gaussian_blur_15x.use();
    glViewport(0, 0, resolution.x, resolution.y);

    bloom_target.bind_for_sampling_at(0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_intermediate.get_handle(), i + 1);
    float32 aspect_ratio_factor = (float32)resolution.y / (float32)resolution.x;
    vec2 gaus_scale = vec2(aspect_ratio_factor * blur_radius, 0.0);
    gaussian_blur_15x.set_uniform("gauss_axis_scale", gaus_scale);
    gaussian_blur_15x.set_uniform("lod", i);
    gaussian_blur_15x.set_uniform("transform", fullscreen_quad());
    quad.draw();

    gaus_scale = vec2(0.0f, blur_radius);
    gaussian_blur_15x.set_uniform("gauss_axis_scale", gaus_scale);
    bloom_intermediate.bind_for_sampling_at(0);
    gaussian_blur_15x.set_uniform("lod", i + 1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_target.get_handle(), i + 1);
    quad.draw();

    blur_radius += (blur_factor * blur_radius);
  }
  blur_radius = original;

  // bloom target is now the screen high pass filtered, lod0 no blur, increasing bluriness on each mip level below that
  // now we need to mix the lods and put them in the bloom_result texture

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_result.get_handle(), 0);
  bloom_mix.use();
  bloom_target.bind_for_sampling_at(0);
  glViewport(0, 0, bloom_result.t.size.x, bloom_result.t.size.y);
  bloom_mix.set_uniform("transform", fullscreen_quad());
  bloom_mix.set_uniform("mip_count", levels);
  quad.draw();
  // all of these bloom textures should now be averaged and drawn into a texture at 1/4 the source resolution

  // add to draw_target
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  draw_target.bind_for_drawing_dst();
  bloom_result.bind_for_sampling_at(0);
  glViewport(0, 0, render_target_size.x, render_target_size.y);
  passthrough.use();
  passthrough.set_uniform("transform", fullscreen_quad());
  if (enabled)
    quad.draw();
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_BLEND);
}

void Renderer::skybox_pass(float64 time)
{

  vec3 scalev = vec3(4000);
  mat4 T = translate(camera_position);
  T = mat4(1);
  mat4 S = scale(scalev);
  mat4 transformation = T * S;

  environment.bind(Texture_Location::environment, Texture_Location::irradiance, time, render_target_size);
  glViewport(0, 0, render_target_size.x, render_target_size.y);
  glBindVertexArray(cube.get_vao());
  skybox.use();
  skybox.set_uniform("time", (float32)time);
  skybox.set_uniform("txaa_jitter", txaa_jitter);
  skybox.set_uniform("camera_position", camera_position);
  skybox.set_uniform("MVP", projection * camera * transformation);
  skybox.set_uniform("Model", transformation);

  glCullFace(GL_FRONT);
  // glDisable(GL_CULL_FACE);
  cube.draw();
  // glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  glActiveTexture(GL_TEXTURE0 + Texture_Location::environment);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::irradiance);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Renderer::copy_to_primary_framebuffer_and_txaa(float64 time)
{
  txaa_jitter = get_next_TXAA_sample();
  // TODO: implement motion vector vertex attribute

  // 1: blend draw_target with previous_draw_target, store in draw_target_srgb8
  // 2: copy draw_target to previous_draw_target
  // 3: run fxaa on draw_target_srgb8, drawing to screen

  tonemapping_target_srgb8.bind_for_drawing_dst();
  glViewport(0, 0, render_target_size.x, render_target_size.y);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  temporalaa.use();

  // blend draw_target with previous_draw_target, store in draw_target_srgb8
  if (previous_color_target_missing)
  {
    draw_target.color_attachments[0].bind_for_sampling_at(0);
    draw_target.color_attachments[0].bind_for_sampling_at(1);
  }
  else
  {
    draw_target.color_attachments[0].bind_for_sampling_at(0);
    previous_draw_target.color_attachments[0].bind_for_sampling_at(1);
  }

  temporalaa.set_uniform("transform", fullscreen_quad());
  quad.draw();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, 0);

  // copy draw_target to previous_draw_target
  previous_draw_target.bind_for_drawing_dst();
  passthrough.use();
  draw_target.color_attachments[0].bind_for_sampling_at(0);
  passthrough.set_uniform("transform", fullscreen_quad());
  quad.draw();

  previous_color_target_missing = false;
  glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::use_fxaa_shader(float64 state_time)
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
  fxaa.set_uniform("transform", fullscreen_quad());
  fxaa.set_uniform("inverseScreenSize", vec2(1.0f) / vec2(window_size));
  fxaa.set_uniform("time", (float32)state_time);
}

void Renderer::render(float64 state_time)
{
  DEFERRED_MESH_DELETIONS.clear();
  DEFERRED_TEXTURE_DELETIONS.clear();
  // glEnable(GL_FRAMEBUFFER_SRGB);
  ASSERT(name != "Unnamed Renderer");
// set_message("sin(time) [-1,1]:", s(sin(state_time)), 1.0f);
// set_message("sin(time) [ 0,1]:", s(0.5f + 0.5f * sin(state_time)), 1.0f);
#if DYNAMIC_FRAMERATE_TARGET
  dynamic_framerate_target();
#endif
#if DYNAMIC_TEXTURE_RELOADING
  check_and_clear_expired_textures();
#endif
  if (imgui_this_tick)
  {
    draw_imgui();
  }

  float32 time = (float32)get_real_time();
  float64 t = (time - state_time) / dt;
  // glEnable(GL_FRAMEBUFFER_SRGB);
  // set_message("FRAME_START", "");
  // set_message("BUILDING SHADOW MAPS START", "");
  if (!CONFIG.render_simple)
  {
    build_shadow_maps();
  }
  // set_message("OPAQUE PASS START", "");
  draw_target.bind_for_drawing_dst();

  opaque_pass(time);
  skybox_pass(time);
  instance_pass(time);
  translucent_pass(time);
  postprocess_pass(time);

  // all pixel data is now in draw_target in 16f linear space

  glDisable(GL_DEPTH_TEST);

  if (use_txaa && previous_camera == camera)
  {
    copy_to_primary_framebuffer_and_txaa(state_time);
    previous_camera = camera;
  }
  else
  {
    ///////////////////////////////////////
    // DRAWING TO TONEMAPPER FRAMEBUFFER
    ////////////////////////////////////////
    glEnable(GL_FRAMEBUFFER_SRGB);
    glViewport(0, 0, render_target_size.x, render_target_size.y);
    tonemapping_target_srgb8.bind_for_drawing_dst();
    tonemapping.use();
    tonemapping.set_uniform("transform", fullscreen_quad());
    tonemapping.set_uniform("function_select", selected_tonemap_function);
    tonemapping.set_uniform("texture0_mod", vec4(exposure * exposure_color, 1));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_target.color_attachments[0].bind_for_sampling_at(0);
    quad.draw();
  }

  if (use_fxaa)
  {
    use_fxaa_shader(state_time);
  }
  else
  {
    passthrough.use();
    passthrough.set_uniform("transform", fullscreen_quad());
  }

  ///////////////////////////////////////
  // DRAWING TO FXAA FRAMEBUFFER
  ////////////////////////////////////////
  glEnable(GL_FRAMEBUFFER_SRGB);
  fxaa_target_srgb8.bind_for_drawing_dst();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  tonemapping_target_srgb8.color_attachments[0].bind_for_sampling_at(0);
  quad.draw();

  ///////////////////////////////////////
  // DRAWING TO DEFAULT FRAMEBUFFER
  ////////////////////////////////////////
  glEnable(GL_FRAMEBUFFER_SRGB);
  glViewport(0, 0, window_size.x, window_size.y);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  passthrough.use();
  passthrough.set_uniform("transform", fullscreen_quad());
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  fxaa_target_srgb8.color_attachments[0].bind_for_sampling_at(0);
  quad.draw();

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
  this->window_size = window_size;
  set_vfov(vfov);
  init_render_targets();
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
  camera_gaze = p;
  camera = lookAt(pos, p, {0, 0, 1});
}
void Renderer::set_camera_gaze(vec3 pos, vec3 p)
{
  camera_position = pos;
  camera_gaze = p;
  camera = lookAt(pos, p, {0, 0, 1});
}

void Renderer::set_vfov(float32 vfov)
{
  aspect = (float32)window_size.x / (float32)window_size.y;
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
    bool is_transparent = entity->material->descriptor.translucent_pass;
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

  sort(render_entities.begin(), render_entities.end(),
      [](Render_Entity &left, Render_Entity &right) { 
      
      if (left.material == right.material)
      {
        return left.mesh < right.mesh;
      }      
      return left.material < right.material; });
}

/*Light diameter guideline for deferred rendering (not yet used)
Distance 	Constant 	Linear 	Quadratic
7 	1.0 	0.7 	1.8
13 	1.0 	0.35 	0.44
20 	1.0 	0.22 	0.20
32 	1.0 	0.14 	0.07
50 	1.0 	0.09 	0.032
65 	1.0 	0.07 	0.017
100 	1.0 	0.045 	0.0075
160 	1.0 	0.027 	0.0028
200 	1.0 	0.022 	0.0019
325 	1.0 	0.014 	0.0007
600 	1.0 	0.007 	0.0002
3250 	1.0 	0.0014 	0.000007

*/

void check_FBO_status(GLuint fbo)
{
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  auto result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  // auto result = glCheckNamedFramebufferStatus(fbo,GL_FRAMEBUFFER);
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
    case GL_FRAMEBUFFER_DEFAULT:
      str = "GL_FRAMEBUFFER_DEFAULT";
      break;
  }

  if (str != "GL_FRAMEBUFFER_COMPLETE" && str != "")
  {
    set_message("FBO_ERROR", str);
    ASSERT(0);
  }
}

void Renderer::init_render_targets()
{
  set_message("init_render_targets()");

  render_target_size = ivec2(render_scale * window_size.x, render_scale * window_size.y);

  // these are setup in the bloom function itself
  bloom_result = Texture();
  bloom_intermediate = Texture();
  bloom_target = Texture();
  bloom_fbo = Framebuffer();
  draw_target = Framebuffer();
  previous_draw_target = Framebuffer();
  translucent_sample_source = Framebuffer();
  self_object_depth = Framebuffer();
  tonemapping_target_srgb8 = Framebuffer();
  fxaa_target_srgb8 = Framebuffer();

  Texture_Descriptor td;
  td.source = "generate";
  td.name = name + " Renderer::draw_target.color[0]";
  td.size = render_target_size;
  td.levels = 1;
  td.format = FRAMEBUFFER_FORMAT;
  td.minification_filter = GL_LINEAR;
  draw_target.color_attachments[0] = Texture(td);
  draw_target.color_attachments[0].load();
  draw_target.depth_enabled = true;
  td.name = name + " Renderer::draw_target.depth_texture";
  draw_target.depth_texture.t.name = td.name;
  draw_target.use_renderbuffer_depth = false;
  draw_target.depth_size = render_target_size;
  draw_target.init();

  td.name = name + " Renderer::previous_frame.color[0]";
  previous_draw_target.color_attachments[0] = Texture(td);
  previous_draw_target.color_attachments[0].load();
  previous_draw_target.init();

  td.name = name + " Renderer::translucent_sample_source.color[0]";
  translucent_sample_source.color_attachments[0] = Texture(td);
  translucent_sample_source.color_attachments[0].t.wrap_s = GL_CLAMP_TO_EDGE;
  translucent_sample_source.color_attachments[0].t.wrap_t = GL_CLAMP_TO_EDGE;
  translucent_sample_source.color_attachments[0].load();
  translucent_sample_source.depth_enabled = true;
  translucent_sample_source.use_renderbuffer_depth = false;
  translucent_sample_source.depth_size = render_target_size;
  td.name = name + " Renderer::translucent_sample_source.depth_texture";
  translucent_sample_source.depth_texture.t.name = td.name;
  translucent_sample_source.init();

  td.name = name + "Renderer::self_object_depth";
  self_object_depth.color_attachments[0] = Texture(td);
  self_object_depth.depth_enabled = true;
  self_object_depth.use_renderbuffer_depth = false;
  self_object_depth.depth_size = render_target_size;
  self_object_depth.depth_texture.t.name = td.name + "'s depth texture";
  self_object_depth.init();

  // full render scaled, clamped and encoded srgb
  Texture_Descriptor srgb8;
  srgb8.source = "generate";
  srgb8.name = name + " Renderer::draw_target_srgb8.color[0]";
  srgb8.size = render_target_size;
  srgb8.levels = 1;
  srgb8.format = GL_SRGB8;
  srgb8.minification_filter = GL_LINEAR;
  tonemapping_target_srgb8.color_attachments[0] = Texture(srgb8);
  tonemapping_target_srgb8.color_attachments[0].load();
  tonemapping_target_srgb8.init();

  // full render scaled, fxaa or passthrough target
  Texture_Descriptor fxaa;
  fxaa.source = "generate";
  fxaa.name = name + " Renderer::draw_target_post_fxaa.color[0]";
  fxaa.size = render_target_size;
  fxaa.levels = 1;
  fxaa.format = GL_SRGB8;
  fxaa.minification_filter = GL_LINEAR;
  fxaa_target_srgb8.color_attachments[0] = Texture(fxaa);
  fxaa_target_srgb8.color_attachments[0].load();
  fxaa_target_srgb8.init();
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

  mat4 result = translate(vec3(translation / vec2(render_target_size), 0));
  jitter_switch = !jitter_switch;
  return result;
}

Mesh_Handle::~Mesh_Handle()
{
  if (std::this_thread::get_id() != MAIN_THREAD_ID)
  {
    DEFERRED_MESH_DELETIONS.push_back(*this);
    return;
  }
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
  pre_blur.depth_format = GL_DEPTH_COMPONENT32F;

  Texture_Descriptor pre_blur_td;
  pre_blur_td.source = "generate";
  pre_blur_td.name = "Spotlight Shadow Map pre_blur[0]";
  pre_blur_td.size = size;
  pre_blur_td.format = format;
  pre_blur_td.minification_filter = GL_LINEAR;
  pre_blur.color_attachments[0] = Texture(pre_blur_td);
  pre_blur.color_attachments[0].bind_for_sampling_at(0);
  pre_blur.init();

  ASSERT(pre_blur.color_attachments.size() == 1);

  Texture_Descriptor td;
  td.source = "generate";
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
  if (!handle)
  {
    if (is_equirectangular)
    {
      if (source.texture && source.texture->texture != 0)
      {
        if (!source.bind_for_sampling_at(0)) // attempting to bind the texture will check the transfer sync
        {
          return;
        }
        produce_cubemap_from_equirectangular_source();
      }
      else
      {
        source.load();
        return;
      }
    }
    else
    {
      produce_cubemap_from_texture_array();
      return;
    }
  }
  glActiveTexture(GL_TEXTURE0 + texture_unit);
  glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
}

void Cubemap::produce_cubemap_from_equirectangular_source()
{
  handle = make_shared<Texture_Handle>();
  std::string &filename = source.t.name;
  std::string label = filename + "cubemap";
  TEXTURECUBEMAP_CACHE[label] = handle;
  handle->filename = label;
  handle->is_cubemap = true;

  handle->internalformat = GL_RGB16F;
  size = CONFIG.use_low_quality_specular ? ivec2(1024, 1024) : ivec2(2048, 2048);
  size = ivec2(2048);
  handle->size = size;

  static Shader equi_to_cube("equi_to_cube.vert", "equi_to_cube.frag");
  static Mesh cube(Mesh_Descriptor(cube, "Cubemap(string equirectangular_filename)'s cube"));

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

  GLint current_fbo;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_fbo);

  // initialize targets and result cubemap
  GLuint fbo;
  GLuint rbo;
  glCreateFramebuffers(1, &fbo);
  glCreateRenderbuffers(1, &rbo);
  glNamedRenderbufferStorage(rbo, GL_DEPTH_COMPONENT24, size.x, size.y);
  glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

  glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &handle->texture);
  uint32 level_count = 1 + uint32(floor(glm::log2(float32(glm::max(size.x, size.y)))));
  glTextureStorage2D(handle->texture, level_count, GL_RGB16F, size.x, size.y);

  mat4 rot = toMat4(quat(1, 0, 0, radians(0.f)));
  glViewport(0, 0, size.x, size.y);
  equi_to_cube.use();
  equi_to_cube.set_uniform("projection", projection);
  equi_to_cube.set_uniform("rotation", rot);
  equi_to_cube.set_uniform("gamma_encoded", is_gamma_encoded);
  ASSERT(source.texture->texture != 0);
  source.bind_for_sampling_at(0);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glCullFace(GL_FRONT);

  glDisable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  // draw to cubemap target
  for (uint32 i = 0; i < 6; ++i)
  {
    equi_to_cube.set_uniform("camera", cameras[i]);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, handle->texture, 0);
    cube.draw();
  }
  glCullFace(GL_BACK);
  glTextureParameteri(handle->texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameteri(handle->texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTextureParameteri(handle->texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(handle->texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTextureParameteri(handle->texture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  handle->magnification_filter = GL_LINEAR;
  handle->minification_filter = GL_LINEAR_MIPMAP_LINEAR;
  handle->wrap_s = GL_CLAMP_TO_EDGE;
  handle->wrap_t = GL_CLAMP_TO_EDGE;
  glGenerateTextureMipmap(handle->texture);
  // cleanup targets
  glDeleteRenderbuffers(1, &rbo);
  glDeleteFramebuffers(1, &fbo);

  glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
  glClearColor(1.0, 0.0, 0.0, 1.0);
  glEnable(GL_DEPTH_TEST);
}

void Cubemap::produce_cubemap_from_texture_array()
{
  string key = sources[0].filename + sources[1].filename + sources[2].filename + sources[3].filename +
               sources[4].filename + sources[5].filename;
  handle = make_shared<Texture_Handle>();
  TEXTURECUBEMAP_CACHE[key] = handle;
  handle->is_cubemap = true;

  size = ivec2(source.texture->size.x, source.texture->size.y);
  glGenTextures(1, &handle->texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP, handle->texture);
  for (uint32 i = 0; i < 6; ++i)
  {
    Image2 &face = sources[i];
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

// opengl z-forward space is ordered: right left top bottom back front
// our coordinate space is +Z up, +X right, +Y forward
std::array<Image2, 6> load_cubemap_faces(array<string, 6> filenames)
{
  Image2 right = filenames[0];
  Image2 left = filenames[1];
  Image2 top = filenames[2];
  Image2 bottom = filenames[3];
  Image2 back = filenames[4];
  Image2 front = filenames[5];

  right.rotate90();
  right.rotate90();
  right.rotate90();
  left.rotate90();
  bottom.rotate90();
  bottom.rotate90();
  front.rotate90();
  front.rotate90();

  array<Image2, 6> result;
  result[0] = left;
  result[1] = right;
  result[2] = back;
  result[3] = front;
  result[4] = bottom;
  result[5] = top;
  return result;
}

Cubemap::Cubemap(string equirectangular_filename, bool gamma_encoded)
{
  is_equirectangular = true;
  std::shared_ptr<Texture_Handle> ptr = TEXTURECUBEMAP_CACHE[equirectangular_filename + "cubemap"].lock();
  if (ptr)
  {
    size = ptr->size;
    handle = ptr;
    return;
  }
  Texture_Descriptor d;
  d.source = equirectangular_filename;
  d.name = equirectangular_filename;
  d.format = GL_RGBA16F;
  source = d;
  is_gamma_encoded = gamma_encoded;
}

Cubemap::Cubemap(array<string, 6> filenames)
{
  string key = filenames[0] + filenames[1] + filenames[2] + filenames[3] + filenames[4] + filenames[5];
  std::shared_ptr<Texture_Handle> ptr = TEXTURECUBEMAP_CACHE[key].lock();
  if (ptr)
  {
    handle = ptr;
    return;
  }

  sources = load_cubemap_faces(filenames);
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

  radiance_is_gamma_encoded = !has_hdr_file_extension(m.radiance);

  if (m.source_is_equirectangular)
  {
    radiance = Cubemap(m.radiance, radiance_is_gamma_encoded);
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
    // irradiance_convolution();
  }
}

void Environment_Map::bind(GLuint radiance_texture_unit, GLuint irradiance_texture_unit, float32 time, vec2 size)
{
  irradiance.bind(irradiance_texture_unit);
  radiance.bind(radiance_texture_unit);
  if (radiance.handle)
    radiance.handle->generate_ibl_mipmaps(time);
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
    translucent_pass = override->translucent_pass;
    casts_shadows = override->casts_shadows;
    discard_on_alpha = override->discard_on_alpha;

    uniform_set = override->uniform_set;
  }
}

Particle_Emitter::Particle_Emitter(Particle_Emitter_Descriptor d, Mesh_Index m, Material_Index mat)
    : descriptor(d), mesh_index(m), material_index(mat)
{
  shared_data = std::make_unique<Physics_Shared_Data>();
  shared_data->descriptor = &descriptor;
  t = std::thread(thread, shared_data);
  t.detach();
  thread_launched = true;
}

Particle_Emitter::Particle_Emitter()
{
  shared_data = std::make_unique<Physics_Shared_Data>();
  shared_data->descriptor = &descriptor;
  t = std::thread(thread, shared_data);
  t.detach();
  thread_launched = true;
}

Particle_Emitter::Particle_Emitter(Particle_Emitter &rhs)
    : descriptor(rhs.descriptor), mesh_index(rhs.mesh_index), material_index(rhs.material_index)
{
  shared_data = std::make_unique<Physics_Shared_Data>();
  shared_data->descriptor = &descriptor;
  rhs.fence();
  shared_data->particles = rhs.shared_data->particles;
  t = std::thread(thread, shared_data);
  t.detach();
  thread_launched = true;
}

Particle_Emitter::Particle_Emitter(Particle_Emitter &&rhs)
{
  ASSERT(rhs.shared_data->request_thread_exit == false);
  rhs.fence();
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
  // todo: option to snap all particles to emitter space or world space

  // locally calculate basis vector, or, change scene graph to store its last-calculated world basis, and read from
  // that here  take lock  modify the descriptor to change position, orientation, velocity  release lock
  ASSERT(shared_data);
  ASSERT(thread_launched);

  fence();
  shared_data->descriptor = &descriptor;
  // ASSERT(shared_data->descriptor == &descriptor);
  shared_data->projection = projection;
  shared_data->camera = camera;
  shared_data->requested_tick += 1;
}

void Particle_Emitter::fence()
{
  ASSERT(shared_data);
  if (shared_data->requested_tick != 0)
  {
    while (shared_data->requested_tick != shared_data->completed_update)
    {
      // spin
      // Sleep(1);
    }
  }
  idle = shared_data->idle;
  active = shared_data->active;
  time_allocations = shared_data->time_allocations;
  attribute_times = shared_data->attribute_timer;

  per_static_octree_test = shared_data->per_static_octree_test;
  per_dynamic_octree_test = shared_data->per_dynamic_octree_test;

  static_collider_count_max = shared_data->static_collider_count_max;
  static_collider_count_sum = shared_data->static_collider_count_sum;
  static_collider_count_samples = shared_data->static_collider_count_samples;

  dynamic_collider_count_max = shared_data->dynamic_collider_count_max;
  dynamic_collider_count_sum = shared_data->dynamic_collider_count_sum;
  dynamic_collider_count_samples = shared_data->dynamic_collider_count_samples;
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
  return nullptr;
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
  return nullptr;
}

void Particle_Emitter::thread(std::shared_ptr<Physics_Shared_Data> shared_data)
{
  ASSERT(shared_data);
  std::unique_ptr<Particle_Emission_Method> emission = construct_emission_method(*shared_data->descriptor);
  std::unique_ptr<Particle_Physics_Method> physics = construct_physics_method(*shared_data->descriptor);
  Particle_Emission_Type emission_type = shared_data->descriptor->emission_descriptor.type;
  Particle_Physics_Type physics_type = shared_data->descriptor->physics_descriptor.type;
  while (!shared_data->request_thread_exit)
  {
    shared_data->idle.start();
    if (shared_data->requested_tick == shared_data->completed_update)
    {
      // instead of this we should wait on an event that is triggered by setting the new tick...
      // so that we can get to work immediately
      SDL_Delay(1);
      continue;
    }
    shared_data->idle.stop();

    // make the vertical slider bois in the renderer imgui window
    // to auto assign to every shader as general 'knob0' 'knob1'... etc
    // make the actual values global so that shader.use can see them and
    // literally all shaders auto-set those uniforms

    float64 last_attribute_calc_time_taken = clamp(shared_data->attribute_timer.get_last(), 0., float64(dt));
    float64 time_left_for_tick = glm::clamp(get_time_left_in_this_tick(), 0., float64(dt));
    float64 time_to_give = time_left_for_tick - last_attribute_calc_time_taken;
    shared_data->time_allocations.insert_time(time_to_give);
    shared_data->active.start();

    if (emission_type != shared_data->descriptor->emission_descriptor.type)
      emission = construct_emission_method(*shared_data->descriptor);
    if (physics_type != shared_data->descriptor->physics_descriptor.type)
      physics = construct_physics_method(*shared_data->descriptor);

    emission_type = shared_data->descriptor->emission_descriptor.type;
    physics_type = shared_data->descriptor->physics_descriptor.type;

    shared_data->descriptor->emission_descriptor.dynamic_octree = shared_data->descriptor->dynamic_octree;
    shared_data->descriptor->physics_descriptor.dynamic_octree = shared_data->descriptor->dynamic_octree;
    shared_data->descriptor->emission_descriptor.static_octree = shared_data->descriptor->static_octree;
    shared_data->descriptor->physics_descriptor.static_octree = shared_data->descriptor->static_octree;

    shared_data->descriptor->emission_descriptor.static_geometry_collision =
        shared_data->descriptor->static_geometry_collision;
    shared_data->descriptor->physics_descriptor.static_geometry_collision =
        shared_data->descriptor->static_geometry_collision;
    shared_data->descriptor->emission_descriptor.dynamic_geometry_collision =
        shared_data->descriptor->dynamic_geometry_collision;
    shared_data->descriptor->physics_descriptor.dynamic_geometry_collision =
        shared_data->descriptor->dynamic_geometry_collision;

    shared_data->descriptor->emission_descriptor.maximum_octree_probe_size =
        shared_data->descriptor->maximum_octree_probe_size;
    shared_data->descriptor->physics_descriptor.maximum_octree_probe_size =
        shared_data->descriptor->maximum_octree_probe_size;

    const float32 time = shared_data->completed_update * dt;
    vec3 pos = shared_data->descriptor->position;
    vec3 vel = shared_data->descriptor->velocity;
    quat o = shared_data->descriptor->orientation;
    emission->update(&shared_data->particles, &shared_data->descriptor->emission_descriptor, pos, vel, o, time, dt,
        shared_data.get());
    physics->step(&shared_data->particles, &shared_data->descriptor->physics_descriptor, time, dt, shared_data.get());
    // delete expired particles:
    shared_data->attribute_timer.start();
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
    shared_data->particles.compute_attributes(shared_data->projection, shared_data->camera, &shared_data->active);
    shared_data->attribute_timer.stop();
    shared_data->active.stop();
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
  fence();
  if (mesh_index != NODE_NULL && material_index != NODE_NULL)
  {
    return shared_data->particles.prepare_instance(accumulator);
  }
  return false;
}

void Particle_Emitter::clear()
{
  ASSERT(shared_data);
  fence();
  shared_data->particles.particles.clear();
}

Particle_Emitter &Particle_Emitter::operator=(Particle_Emitter &rhs)
{
  rhs.fence();
  fence();
  shared_data = std::make_unique<Physics_Shared_Data>();
  descriptor = rhs.descriptor;
  shared_data->descriptor = &descriptor;
  mesh_index = rhs.mesh_index;
  material_index = rhs.material_index;
  return *this;
}

Particle_Emitter &Particle_Emitter::operator=(Particle_Emitter &&rhs)
{
  ASSERT(rhs.shared_data->request_thread_exit == false);
  ASSERT(shared_data->request_thread_exit == false);
  rhs.fence();
  fence();
  shared_data->request_thread_exit = true;
  t.join();
  t = std::move(rhs.t);
  shared_data = std::move(rhs.shared_data);
  descriptor = rhs.descriptor;
  shared_data->descriptor = &descriptor;
  mesh_index = rhs.mesh_index;
  material_index = rhs.material_index;
  return *this;
}

void physics_billboard_step(Particle &particle, const Particle_Physics_Method_Descriptor *d, float32 current_time)
{
  bool lock_z = particle.billboard_lock_z;

  particle.billboard_angle += particle.billboard_rotation_velocity;
  float32 applying_angle =
      wrap_to_range(particle.billboard_angle + d->billboard_rotation_time_factor * current_time, 0, two_pi<float32>());
  particle.orientation = angleAxis(applying_angle, vec3(0, 0, 1));
}

void misc_particle_attribute_iteration_step(
    Particle &particle, const Particle_Physics_Method_Descriptor *d, float32 current_time)
{
  if (d->size_multiply_uniform_min != 1.f || d->size_multiply_uniform_max != 1.f)
  {
    particle.scale = random_between(d->size_multiply_uniform_min, d->size_multiply_uniform_max) * particle.scale;
  }
  if (d->size_multiply_min != vec3(1.f) || d->size_multiply_max != vec3(1.f))
  {
    particle.scale =
        (d->size_multiply_min + random_within(d->size_multiply_max - d->size_multiply_min)) * particle.scale;
  }

  if (d->die_when_size_smaller_than != vec3(0))
  {
    bool3 greater = glm::greaterThan(particle.scale, d->die_when_size_smaller_than);
    if (!any(greater))
    {
      particle.time_left_to_live = 0.f;
    }
  }
}

void simple_physics_step(Particle &particle, Particle_Physics_Method_Descriptor *d, float32 current_time)
{
  particle.position += (dt * particle.velocity);
  float32 len_v = length(particle.velocity);
  if (len_v > 100000)
  {
    particle.time_left_to_live = 0.f;
  }
  else
  {
//#define GODS_WAY
#ifdef GODS_WAY
    float32 density_of_air = 1.0f;
    float32 area_of_front = 1.0f;
    vec3 velocity_n = normalize(particle.velocity);
    float32 speedsq = len_v * len_v;
    float32 drag = d->drag * area_of_front * 0.5f * density_of_air * speedsq;
    particle.velocity = particle.velocity - dt * drag * velocity_n;
#endif

#if 1
    particle.velocity = particle.velocity * (1.0f - dt * len_v * 0.5f * d->drag);
    particle.velocity += dt * d->gravity;

#endif

#if 0
    vec3 vel_squared = particle.velocity*particle.velocity;
    

    if(particle.velocity.x < 0)
    {
      vel_squared.x = -vel_squared.x;
    }
    if (particle.velocity.y < 0)
    {
      vel_squared.y = -vel_squared.y;
    }
    if (particle.velocity.z < 0)
    {
      vel_squared.z = -vel_squared.z;
    }

    vec3 drag = dt * d->drag * 0.5f * vel_squared;
    particle.velocity = particle.velocity - drag; 
    particle.velocity += dt * d->gravity;
#endif
  }
  misc_particle_attribute_iteration_step(particle, d, current_time);
  if (particle.billboard)
  {
    physics_billboard_step(particle, d, current_time);
  }
}

void Simple_Particle_Physics::step(
    Particle_Array *p, Particle_Physics_Method_Descriptor *d, float32 time, float32 dt, Physics_Shared_Data *data)
{
  ASSERT(p);
  ASSERT(d);
  float32 current_time = (float32)get_real_time();
  for (auto &particle : p->particles)
  {
    if (d->abort_when_late)
    {
      if (data->active.get_current() > (0.7 * dt) || SPIRAL_OF_DEATH)
      {
        return;
      }
    }
    simple_physics_step(particle, d, current_time);
  }
}

void Wind_Particle_Physics::step(
    Particle_Array *p, Particle_Physics_Method_Descriptor *d, float32 t, float32 dt, Physics_Shared_Data *data)
{
  ASSERT(p);
  ASSERT(d);

  float32 current_time = (float32)get_real_time();
  //
  // float32 bounce_loss = 0.75;
  if (p->particles.size() > 0)
  {
    // bounce_loss = fract(42.353123f * p->particles[0].position.x * p->particles[0].position.y);
    // bounce_loss = lerp(d->bounce_min, d->bounce_max, rand);
  }

  bool want_out = false;
  for (Particle &particle : p->particles)
  {
    // bounce_loss = fract(42.353123f * particle.position.x * particle.position.y);
    // bounce_loss = lerp(d->bounce_min, d->bounce_max, bounce_loss);

    if (!want_out && d->abort_when_late)
    {
      if (data->active.get_current() > (0.75f * data->time_allocations.get_last()) || SPIRAL_OF_DEATH)
      {
        want_out = true;
        return; // sponge
      }
    }

    if (want_out)
    {
      if (particle.billboard)
      {
        // physics_billboard_step(particle, d, current_time);//sponge
      }
      continue;
    }

    // vec3 ray = (dt * particle.velocity) + (dt * d->gravity);

    AABB probe(vec3(0));

    // we are testing only the next position
    Particle unmodified_particle = particle;
    vec3 wind = dt * d->wind_intensity * d->direction;
    simple_physics_step(particle, d, current_time);

    // if the probe is too big it grabs too many triangles
    vec3 half_probe_scale = 0.5f * min(particle.scale, vec3(d->maximum_octree_probe_size));
    probe.min = particle.position - half_probe_scale;
    probe.max = particle.position + half_probe_scale;

    ASSERT(!isnan(particle.position.x));
    ASSERT(!isnan(particle.velocity.x));
    // vec3 min = probe.min;
    // vec3 max = probe.max;
    // push_aabb(probe, min + 1.01f * ray);
    // push_aabb(probe, max + 1.01f * ray);

    uint32 counter = 0;
    thread_local static std::vector<Triangle_Normal> colliders;
    colliders.clear();
    uint32 static_collider_count = 0;
    uint32 dynamic_collider_count = 0;
    if (d->static_octree && d->static_geometry_collision)
    {
      data->per_static_octree_test.start();
      d->static_octree->test_all(probe, &counter, &colliders);
      data->per_static_octree_test.stop();

      static_collider_count = (uint32)colliders.size();
      static_collider_count = counter;
      // if (static_collider_count > 1000)
      //{
      //  std::vector<Triangle_Normal> colliderstest;
      //  d->static_octree->test_all(probe, &counter, &colliderstest);
      //}

      data->static_collider_count_max = glm::max(data->static_collider_count_max, static_collider_count);
      data->static_collider_count_samples += 1;
      data->static_collider_count_sum = data->static_collider_count_sum + static_collider_count;
    }
    if (d->dynamic_octree && d->dynamic_geometry_collision)
    {
      data->per_static_octree_test.start();
      data->per_dynamic_octree_test.start();
      data->per_dynamic_octree_test.start();
      d->dynamic_octree->test_all(probe, &counter, &colliders);
      data->per_dynamic_octree_test.stop();

      dynamic_collider_count = (uint32)colliders.size() - static_collider_count;
      data->dynamic_collider_count_max = glm::max(data->dynamic_collider_count_max, dynamic_collider_count);
      data->dynamic_collider_count_samples += 1;
      data->dynamic_collider_count_sum += dynamic_collider_count;
    }
    if (colliders.size() == 0) //|| particle.last_frame_collided)
    {
      // if (colliders.size() == 0)
      //{
      // particle.last_frame_collided = false;
      // particle.last_frame_collision_normal = vec3(0);
      //}
      ASSERT(!isnan(particle.position.x));
      ASSERT(!isnan(particle.velocity.x));

      particle.velocity = particle.velocity + wind;
      continue;
    }

    // bool aabb_triangle_intersection(const AABB &aabb, const Triangle_Normal &triangle)

    vec3 approach_velocity = unmodified_particle.velocity;
    vec3 reflection_normal = vec3(0);
    vec3 collider_velocity = vec3(0);

    // jank way to 'identify' different objects instead of using an id
    // this was meant to cull multiple triangles of the same object
    // for the purpose of avoiding adding the velocity of a single (multitriangle) object more than once
    // however it is technically broken when there is more than one object with the same velocity
    // which is not common for moving objects, but stationary ones....

    // for now we are just using it to cull triangles we are approaching from behind
    std::vector<vec3> velocities_found;
    for (uint32 i = 0; i < colliders.size(); ++i)
    {
      // bool found = false;
      // for (uint32 j = 0; j < velocities_found.size(); ++j)
      //{
      //  if (velocities_found[j] == colliders[i].v)
      //  {
      //    found = true;
      //    break;
      //  }
      //}
      // if (!found)
      {
        vec3 approach_velocity = particle.velocity - colliders[i].v;
        if (dot(-approach_velocity, colliders[i].n) > 0.f)
        {
          velocities_found.push_back(colliders[i].v);
          reflection_normal = reflection_normal + colliders[i].n;
          collider_velocity = collider_velocity + colliders[i].v;
        }
      }
    }

    if (reflection_normal == vec3(0))
    {
      // all the colliders were backfaces
      continue;
    }
    reflection_normal = normalize(reflection_normal);

    // if we get here, our next position isn't valid, we intersected a triangle facing us
    bool moving = length(particle.velocity) > d->stiction_velocity;
    if (moving)
    {
      vec3 full_step_ray = particle.position - unmodified_particle.position;

      float32 largest_non_colliding_t = 0.f;
      float32 t = 0.5f; //% of dt we collide at
      float32 tt = 0.25f;
      bool is_colliding = true;
      AABB probe2(vec3(0));

      ////sponge:testing to see if the old particle was colliding:
      // colliders.clear();
      // if (d->static_geometry_collision && d->static_octree)
      //  d->static_octree->test_all(probe, &counter, &colliders);
      // if (d->dynamic_geometry_collision && d->dynamic_octree)
      //  d->dynamic_octree->test_all(probe, &counter, &colliders);

      // if(colliders.size() > 0)
      //{
      //  int a  = 3;
      //}

      for (uint32 i = 0; i < d->collision_binary_search_iterations; ++i)
      {
        // this probe is not completely correct if the size is increasing
        vec3 new_pos = unmodified_particle.position + t * full_step_ray;
        probe2.min = new_pos - half_probe_scale;
        probe2.max = new_pos + half_probe_scale;
        colliders.clear();

        if (d->static_geometry_collision && d->static_octree)
          d->static_octree->test_all(probe2, &counter, &colliders);
        if (d->dynamic_geometry_collision && d->dynamic_octree)
          d->dynamic_octree->test_all(probe2, &counter, &colliders);

        is_colliding = colliders.size() > 0;

        if (is_colliding)
        {
          t = t - tt;
        }
        else
        {
          t = t + tt;
          largest_non_colliding_t = t;
        }
        tt = 0.5f * tt;
      }
      float32 before_reflect_t = largest_non_colliding_t;
      float32 after_reflect_t = 1.0f - largest_non_colliding_t;

      // revert our state to before we stepped into colliding geometry
      particle = unmodified_particle;
      // time step forward until we hit the surface
      particle.position = unmodified_particle.position + (before_reflect_t * dt * unmodified_particle.velocity);
      particle.velocity += (before_reflect_t * dt * d->gravity);

      // instantaneous surface interaction
      particle.velocity = reflect(particle.velocity, reflection_normal);
      float32 ndotv = clamp(dot(reflection_normal, particle.velocity), 0.f, 1.f);
      vec3 bounce_loss = vec3(random_between(d->bounce_min, d->bounce_max));
      bounce_loss = mix(bounce_loss, d->friction, 1.0f - ndotv);
      particle.velocity = (bounce_loss * particle.velocity) + collider_velocity;
      particle.billboard_rotation_velocity = bounce_loss.z * particle.billboard_rotation_velocity;
      particle.angular_velocity = bounce_loss * particle.angular_velocity;

      // time step forward after interaction
      particle.position += (after_reflect_t * (dt)*particle.velocity);
      particle.velocity += (after_reflect_t * dt * d->gravity);

      // misc
      // particle.velocity *= d->drag; // needs to scale with velocity squared
      particle.velocity += wind;

      // scale change etc
      // this might enlarge the particle and make us intersect again...
      misc_particle_attribute_iteration_step(particle, d, current_time);
      if (particle.billboard)
      {
        physics_billboard_step(particle, d, current_time);
      }

#if 0
      // final step, if we are still inside something, lets just push ourselves
      // out in the direction of the normal
      float32 offset = 0.105f;
      for (uint32 i = 0; i < 1; ++i)
      {
        probe.min = particle.position - half_probe_scale;
        probe.max = particle.position + half_probe_scale;

        colliders.clear();
        if (d->static_octree && d->static_geometry_collision)
        {
          // data->per_static_octree_test.start();
          d->static_octree->test_all(probe, &counter, &colliders);
          // data->per_static_octree_test.stop();
        }
        if (d->dynamic_octree && d->dynamic_geometry_collision)
        {
          // data->per_dynamic_octree_test.start();
          d->dynamic_octree->test_all(probe, &counter, &colliders);
          // data->per_dynamic_octree_test.stop();
        }
        if (colliders.size() == 0)
          break;

        for (Triangle_Normal &collider : colliders)
        {
          if (dot(-approach_velocity, collider.n) > 0.f)
          {
            particle.position += offset * collider.n;

            //sponge
            particle.position = vec3(0,0,4);
            particle.velocity = vec3(0.f);
          }
        }
        // offset = 2.f * offset;

        // particle.position += dt * particle.velocity; no: we did it just above

        ASSERT(!isnan(particle.velocity.x));
        ASSERT(!isnan(particle.position.x));
      }
#endif
    }
    else
    {
      particle.velocity = vec3(0.f);
      particle.billboard_rotation_velocity = 0.f;
      particle.angular_velocity = vec3(0.f);
      simple_physics_step(particle, d, current_time);
      particle.velocity = vec3(0.f);
    }
  }
}
float32 radicalInverse_VdC(uint32 bits)
{
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float32(bits) * 2.3283064365386963e-10; // / 0x100000000
}
vec2 hammersley2d(uint32 i, uint32 N)
{
  return vec2(float32(i) / float32(N), radicalInverse_VdC(i));
}

vec3 hemisphere_sample_uniform(vec2 uv)
{
  float32 phi = uv.y * two_pi<float32>();
  float32 cos_theta = 1.0f - uv.x;
  float32 sin_theta = sqrt(1.0f - cos_theta * cos_theta);
  return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

vec3 hammersley_hemisphere(uint32 i, uint32 n)
{
  return hemisphere_sample_uniform(hammersley2d(i, n));
}

vec3 hammersley_sphere(uint32 i, uint32 n)
{
  uint32 extra_index = n % 2 != 0; // if n is odd, we do one extra on the top hemisphere
  uint32 half_n = n / 2;
  vec3 result;
  if (i < half_n)
    result = vec3(1, 1, -1) * hammersley_hemisphere(i, half_n); // bottom
  else
    result = hammersley_hemisphere(i - half_n, half_n + extra_index); // top

  return result;
}
Particle misc_particle_emitter_step(
    Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel, quat o, float32 time, float32 dt)
{
  Particle new_particle;
  new_particle.billboard = d->billboarding;
  new_particle.billboard_lock_z = d->billboard_lock_z;
  float32 extra_rotational_vel = random_between(0.f, d->initial_billboard_rotation_velocity_variance);
  new_particle.billboard_rotation_velocity = d->billboard_rotation_velocity + extra_rotational_vel;
  new_particle.billboard_angle = d->billboard_initial_angle;
  vec3 pos_variance = random_within(d->initial_position_variance);
  pos_variance = pos_variance - 0.5f * d->initial_position_variance;
  new_particle.position = pos + pos_variance;

  vec3 vel_variance = random_within(d->initial_velocity_variance);
  vel_variance = vel_variance - 0.5f * d->initial_velocity_variance;
  if (d->inherit_velocity != 0.f)
    new_particle.velocity = o * (d->inherit_velocity * vel + d->initial_velocity + vel_variance);
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

  vec3 extra_scale;
  std::normal_distribution<float32> dist(0.f, d->initial_extra_scale_variance.x + .00001);
  extra_scale.x = dist(generator);
  dist = std::normal_distribution<float32>(0.f, d->initial_extra_scale_variance.y + .00001);
  extra_scale.y = dist(generator);
  dist = std::normal_distribution<float32>(0.f, d->initial_extra_scale_variance.z + .00001);
  extra_scale.z = dist(generator);
  new_particle.scale = d->initial_scale + extra_scale;
  new_particle.scale =
      new_particle.scale + random_between(0.f, d->initial_extra_scale_uniform_variance) * d->initial_scale;

  new_particle.lifespan = d->minimum_time_to_live + random_between(0.f, d->extra_time_to_live_variance);

  new_particle.time_left_to_live = new_particle.lifespan;

  return new_particle;
}
void Particle_Explosion_Emission::update(Particle_Array *p, Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel,
    quat o, float32 time, float32 dt, Physics_Shared_Data *data)
{
  ASSERT(p);
  ASSERT(d);
  if (d->boom_t < dt)
  {
    return;
  }

  d->boom_t = d->boom_t - dt;

  if (!d->generate_particles)
  {
    return;
  }

  // std::vector<uint32> shuffled_indices;
  // if (d->low_discrepency_position_variance)
  //{
  //  shuffled_indices.resize(d->particle_count_per_tick);
  //  std::iota(shuffled_indices.begin(), shuffled_indices.end(), 0);
  //  std::shuffle(shuffled_indices.begin(), shuffled_indices.end(), std::mt19937{ std::random_device{}() });
  //}
  float32 epc = (float32)d->explosion_particle_count;
#ifndef NDEBUG
  epc = 0.1f * epc;
#endif

  for (uint32 i = 0; i < (uint32)epc; ++i)
  {
    Particle new_particle = misc_particle_emitter_step(d, pos, vel, o, time, dt);

    vec3 impulse_p = pos;

    if (d->impulse_center_offset_min != vec3(0) || d->impulse_center_offset_max != vec3(0))
    {
      impulse_p += random_between(d->impulse_center_offset_min, d->impulse_center_offset_max);
    }

    if (d->low_discrepency_position_variance)
    {
      vec3 hammersley_pos;
      if (d->hammersley_sphere)
      {
        hammersley_pos = hammersley_sphere(i, (uint32)epc);
      }
      else
      {
        hammersley_pos = hammersley_hemisphere(i, (uint32)epc);
      }
      // lets take the previously calculated random-offsetted position from the
      // misc emitter step and use its length as the distance from emitter origin
      float32 dist = length(new_particle.position - pos);
      dist = length(new_particle.position - impulse_p);
      // if (dist == 0)
      {
        dist = d->hammersley_radius;
      }
      new_particle.position = pos + dist * normalize(hammersley_pos);

      vec3 dir = normalize(new_particle.position - impulse_p);
      if (isnan(dir.x))
      {
        dir = vec3(0.f, 0.f, 0.0001f);
      }
      new_particle.velocity = new_particle.velocity + (d->power / glm::max(dist, 0.1f)) * dir;

      if (d->enforce_velocity_position_offset_match)
      {
        new_particle.velocity = length(new_particle.velocity) * (new_particle.position - pos);
      }
      ASSERT(!isnan(new_particle.velocity.x));
      p->particles.push_back(new_particle);
    }
    else
    {
      vec3 dir = normalize(new_particle.position - impulse_p);

      if (new_particle.position == impulse_p)
      {
        new_particle.position += 0.1f * random_3D_unit_vector();
      }
      if (isnan(dir.x))
      {
        dir = vec3(0.f, 0.f, 0.0001f);
      }

      float32 dist = length(new_particle.position - impulse_p);
      if (dist == 0)
      {
        dist = 0.01f;
      }

      new_particle.velocity = new_particle.velocity + (d->power / dist) * dir;
      if (d->enforce_velocity_position_offset_match)
      {
        new_particle.velocity = length(new_particle.velocity) * normalize((new_particle.position - pos));
      }
      ASSERT(!isnan(new_particle.velocity.x));
      ASSERT(!isnan(new_particle.position.x));
    }
    if (!d->allow_colliding_spawns)
    {
      AABB probe(vec3(0));
      vec3 half_probe_scale = 0.5f * min(new_particle.scale, vec3(d->maximum_octree_probe_size));
      probe.min = new_particle.position - half_probe_scale;
      probe.max = new_particle.position + half_probe_scale;
      thread_local static std::vector<Triangle_Normal> colliders;
      colliders.clear();
      uint32 counter = 0;
      if (d->static_octree && d->static_geometry_collision)
      {
        d->static_octree->test_all(probe, &counter, &colliders);
      }
      if (d->dynamic_octree && d->dynamic_geometry_collision)
      {
        d->dynamic_octree->test_all(probe, &counter, &colliders);
      }

      if (colliders.size() != 0)
      {
        new_particle.time_left_to_live = 0.f;
      }
    }
    p->particles.push_back(new_particle);
  }
}

void Particle_Stream_Emission::update(Particle_Array *p, Particle_Emission_Method_Descriptor *d, vec3 pos, vec3 vel,
    quat o, float32 time, float32 dt, Physics_Shared_Data *data)
{
  ASSERT(p);
  ASSERT(d);

  // do this better, accumulate time
  // they should also be placed smoothly along the delta position of the
  // emitter itself
  // based on time between visits and expected spawn time per individual particle
  // instead of all clumped at the current position

  // perhaps a simple way would to be loop n times per dt and do the same thing here
  // do this spawns loop "spawns per dt"+1 times per visit to this function
  float32 sps = d->particles_per_second;
#ifndef NDEBUG
  sps = 0.1f * sps;
#endif

  const uint32 particles_before_tick = (uint32)floor(sps * time);
  const uint32 particles_after_tick = (uint32)floor(sps * (time + dt));
  const uint32 spawns = particles_after_tick - particles_before_tick;

  if (!d->generate_particles)
  {
    return;
  }

  for (uint32 i = 0; i < spawns; ++i)
  {
    if (!(p->particles.size() < MAX_INSTANCE_COUNT))
    {
      return;
    }
    Particle new_particle = misc_particle_emitter_step(d, pos, vel, o, time, dt);
    if (!d->allow_colliding_spawns)
    {
      AABB probe(vec3(0));
      vec3 half_probe_scale = 0.5f * min(new_particle.scale, vec3(d->maximum_octree_probe_size));
      probe.min = new_particle.position - half_probe_scale;
      probe.max = new_particle.position + half_probe_scale;
      thread_local static std::vector<Triangle_Normal> colliders;
      colliders.clear();
      uint32 counter = 0;

      if (d->static_octree && d->static_geometry_collision)
      {
        d->static_octree->test_all(probe, &counter, &colliders);
      }
      if (d->dynamic_octree && d->dynamic_geometry_collision)
      {
        d->dynamic_octree->test_all(probe, &counter, &colliders);
      }

      if (colliders.size() != 0)
      {
        new_particle.time_left_to_live = 0.f;
      }
    }
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
  glGenBuffers(1, &instance_billboard_location_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_billboard_location_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(vec4), (void *)0, GL_DYNAMIC_DRAW);
  glGenBuffers(1, &instance_attribute0_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute0_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(vec4), (void *)0, GL_DYNAMIC_DRAW);
  glGenBuffers(1, &instance_attribute1_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute1_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(vec4), (void *)0, GL_DYNAMIC_DRAW);
  glGenBuffers(1, &instance_attribute2_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute2_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(vec4), (void *)0, GL_DYNAMIC_DRAW);
  glGenBuffers(1, &instance_attribute3_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_attribute3_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCE_COUNT * sizeof(vec4), (void *)0, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  initialized = true;
}

void Particle_Array::destroy()
{
  glDeleteBuffers(1, &instance_mvp_buffer);
  glDeleteBuffers(1, &instance_model_buffer);
  glDeleteBuffers(1, &instance_billboard_location_buffer);
  glDeleteBuffers(1, &instance_attribute0_buffer);
  glDeleteBuffers(1, &instance_attribute1_buffer);
  glDeleteBuffers(1, &instance_attribute2_buffer);
  glDeleteBuffers(1, &instance_attribute3_buffer);
  initialized = false;
}

Particle_Array::Particle_Array(Particle_Array &&rhs) {}

void Particle_Array::compute_attributes(mat4 projection, mat4 view, Timer *active)
{
  MVP_Matrices.clear();
  Model_Matrices.clear();
  billboard_locations.clear();
  attributes0.clear();
  attributes1.clear();
  attributes2.clear();
  attributes3.clear();

  mat4 VP = projection * view;

  // vec3 camera_location = view[3];
  // for (Particle &i : particles)
  //{
  //  vec3 p;
  //  if (i.billboard)
  //  {
  //    p = view * vec4(i.position, 1);
  //  }
  //  else
  //  {
  //    p = i.position;
  //  }
  //  i.distance_to_camera = length(p-camera_location);
  //}

  // sort(particles.begin(),particles.end(),[](const Particle& p1, const Particle& p2){return p1.distance_to_camera
  // > p2.distance_to_camera;});

  for (Particle &i : particles)
  {
    if (i.billboard)
    {
      use_billboarding = true;
      const mat4 R = toMat4(i.orientation);
      mat4 S = scale(i.scale);
      Model_Matrices.push_back(R * S);
      vec4 billboard_position = view * vec4(i.position, 1);
      billboard_locations.push_back(billboard_position);

      if (i.use_attribute0)
      {
        use_attribute0 = true;
        attributes0.push_back(i.attribute0);
      }
      if (i.use_attribute1)
      {
        use_attribute1 = true;
        attributes1.push_back(i.attribute1);
      }
      if (i.use_attribute2)
      {
        use_attribute2 = true;
        attributes2.push_back(i.attribute2);
      }
      if (i.use_attribute3)
      {
        use_attribute3 = true;
        attributes3.push_back(i.attribute3);
      }
      continue;
    }

    ASSERT(!use_billboarding); // all particles must use or not use
    const mat4 R = toMat4(i.orientation);
    const mat4 S = scale(i.scale);
    const mat4 T = translate(i.position);
    const mat4 model = T * R * S;
    const mat4 MVP = VP * model;
    MVP_Matrices.push_back(MVP);
    Model_Matrices.push_back(model);
    if (i.use_attribute0)
    {
      use_attribute0 = true;
      attributes0.push_back(i.attribute0);
    }
    if (i.use_attribute1)
    {
      use_attribute1 = true;
      attributes1.push_back(i.attribute1);
    }
    if (i.use_attribute2)
    {
      use_attribute2 = true;
      attributes2.push_back(i.attribute2);
    }
    if (i.use_attribute3)
    {
      use_attribute3 = true;
      attributes3.push_back(i.attribute3);
    }
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
  // use model matrices size instead of particles
  // because we might have early-outted from the compute attributes function
  uint32 num_instances = Model_Matrices.size();
  if (num_instances == 0)
    return false;

  ASSERT(num_instances <= MAX_INSTANCE_COUNT);

  if (!use_billboarding)
  {
    glBindBuffer(GL_ARRAY_BUFFER, instance_mvp_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(mat4), &MVP_Matrices[0][0][0]);
  }
  glBindBuffer(GL_ARRAY_BUFFER, instance_model_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(mat4), &Model_Matrices[0][0][0]);

  if (use_billboarding)
  {
    glBindBuffer(GL_ARRAY_BUFFER, instance_billboard_location_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(vec4), &billboard_locations[0]);
  }

  if (use_attribute0)
  {
    glBindBuffer(GL_ARRAY_BUFFER, instance_attribute0_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(vec4), &attributes0[0]);
  }
  if (use_attribute1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, instance_attribute1_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(vec4), &attributes1[0]);
  }
  if (use_attribute2)
  {
    glBindBuffer(GL_ARRAY_BUFFER, instance_attribute2_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(vec4), &attributes2[0]);
  }
  if (use_attribute3)
  {
    glBindBuffer(GL_ARRAY_BUFFER, instance_attribute3_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, num_instances * sizeof(vec4), &attributes3[0]);
  }

  result.mvp_buffer = instance_mvp_buffer;
  result.model_buffer = instance_model_buffer;
  result.attribute0_buffer = instance_attribute0_buffer;
  result.attribute1_buffer = instance_attribute1_buffer;
  result.attribute2_buffer = instance_attribute2_buffer;
  result.attribute3_buffer = instance_attribute3_buffer;
  result.instance_billboard_location_buffer = instance_billboard_location_buffer;
  result.size = num_instances;
  result.use_billboarding = use_billboarding;
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

Image2::Image2(string filename)
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

void Image2::rotate90()
{
  const int floats_per_pixel = 4;
  int32 size = width * height * floats_per_pixel;
  Image2 result;
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

Texture Texture_Paint::create_new_texture(ivec2 size, const char *name)
{
  const char *tname = "Untitled";
  if (name)
  {
    tname = name;
  }
  Texture t =
      Texture(tname, vec2(size), 1, GL_RGBA32F, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
  t.load();
  return t;
}

void Texture_Paint::preset_pen()
{
  intensity = 6.1;
  exponent = 4.37;
  size = 4.5;
  hdr = true;
  brush_selection = 4;
  constant_density = false;
  smoothing_count = 28;
  blendmode = 1;
  brush_color = vec4(0);
}

void Texture_Paint::preset_pen2()
{
  intensity = -14.5;
  exponent = 1.8;
  size = 1.8;
  brush_selection = 4;
  constant_density = false;
  smoothing_count = 28;
  blendmode = 2;
  brush_color = vec4(1);
}

void Texture_Paint::iterate(Texture *t, float32 current_time) {}

Texture *Texture_Paint::change_texture_to(int32 index)
{
  ASSERT(textures.size() > index);
  selected_texture = index;
  Texture *surface = &textures[selected_texture];
  while (!surface->texture || !surface->texture->texture)
  {
    surface->load();
  }
  Texture_Descriptor td;
  td.size = surface->texture->size;
  td.format = surface->texture->internalformat;
  ASSERT(td.size != ivec2(0));
  td.source = "generate";

  td.name = "display_surface of Texture_Paint";
  display_surface = Texture(td);
  display_surface.load();
  fbo_display.color_attachments[0] = display_surface;
  fbo_display.init();

  td.name = "intermediate of Texture_Paint";
  intermediate = Texture(td);
  intermediate.load();
  fbo_intermediate.color_attachments[0] = intermediate;
  fbo_intermediate.init();

  fbo_drawing.color_attachments[0] = *surface;
  fbo_drawing.init();

  return surface;
}
void Texture_Paint::run(std::vector<SDL_Event> *imgui_event_accumulator)
{
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  ASSERT(imgui_event_accumulator);
  if (!window_open)
    return;
  float32 time = (float32)get_real_time();
  if (!initialized)
  {
    Mesh_Descriptor md(plane, "Texture_Paint's quad");
    quad = Mesh(md);
    drawing_shader = Shader("passthrough.vert", "paint.frag");
    copy = Shader("passthrough.vert", "passthrough.frag");
    postprocessing_shader = Shader("passthrough.vert", "paint_postprocessing.frag");
    if (textures.size() == 0)
    {
      textures.push_back(create_new_texture(new_texture_size, "primary heightmap"));
      display_surface = create_new_texture(new_texture_size, "display_surface");
      intermediate = create_new_texture(new_texture_size, "texture_paint_intermediate");
    }
    change_texture_to(0);
    preview = Texture("texture_paint_brush_preview", vec2(128), 1, GL_RGB32F, GL_LINEAR, GL_LINEAR);
    fbo_preview.color_attachments[0] = preview;
    preview.load();
    fbo_preview.init();
    initialized = true;
  }

  // ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
  // ImGui::Begin("Texture_Paint", &window_open, window_flags);
  imgui_visit_count += 1;
  const ImVec2 mouse = ImGui::GetMousePos();
  const ImVec2 windowp = ImGui::GetWindowPos();
  const ImVec2 window_size = ImGui::GetWindowSize();
  ivec2 window_mouse_pos = ivec2(mouse.x - windowp.x, mouse.y - windowp.y);
  // bool out_of_window = window_cursor_pos.x < 0 || window_cursor_pos.x > window_size.x || window_cursor_pos.y < 0
  // ||
  //                    window_cursor_pos.y > window_size.y;
  window_mouse_pos = clamp(window_mouse_pos, ivec2(0), ivec2(window_size.x, window_size.y));
  Texture *surface = &textures[selected_texture];

  if (!surface->texture || !surface->bind_for_sampling_at(0))
  {
    ImGui::Text("Surface not ready");
    ImGui::End();
    return;
  }
  ImGui::BeginChild("asdgasda", ImVec2(240, 0), true);
  ImGui::Checkbox("HDR", &hdr);
  ImGui::SameLine();
  ImGui::Checkbox("Cursor", &draw_cursor);
  if (ImGui::Button("Clear Color"))
  {
    clear = 2;
  }
  ImGui::SameLine();
  ImGui::ColorEdit4("clearcolor", &clear_color[0], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
  ImGui::InputFloat("Zoom", &zoom, 0.01);
  Array_String selected_blendmode = "";
  if (blendmode == 0)
    selected_blendmode = "Mix";
  if (blendmode == 1)
    selected_blendmode = "Blend";
  if (blendmode == 2)
    selected_blendmode = "Add";

  if (ImGui::BeginCombo("", "Presets"))
  {
    if (ImGui::Selectable("Pen"))
    {
      preset_pen();
    }
    if (ImGui::Selectable("Pen2"))
    {
      preset_pen2();
    }
    ImGui::EndCombo();
  }
  ImGui::Text("Mask:");
  ImGui::SameLine();
  ImGui::Checkbox("R", (bool *)&mask.r);
  ImGui::SameLine();
  ImGui::Checkbox("G", (bool *)&mask.g);
  ImGui::SameLine();
  ImGui::Checkbox("B", (bool *)&mask.b);
  ImGui::SameLine();
  ImGui::Checkbox("A", (bool *)&mask.a);

  ImGui::PushID("blendmode");
  if (ImGui::BeginCombo("", &selected_blendmode.str[0]))
  {
    if (ImGui::Selectable("Blend"))
    {
      blendmode = 1;
    }

    if (ImGui::MenuItem("Mix"))
    {
      blendmode = 0;
    }

    if (ImGui::Selectable("Add"))
    {
      blendmode = 2;
    }
    ImGui::EndCombo();
  }
  ImGui::PopID();
  ImGui::SetNextItemWidth(60);
  ImGui::DragFloat("Intensity", &intensity, 0.001, -100, 100, "%.3f", 2.5f);
  ImGui::SetNextItemWidth(60);
  ImGui::DragFloat("Exponent", &exponent, 0.001, 0.1, 25, "%.3f", 1.5f);
  ImGui::SetNextItemWidth(60);
  ImGui::DragFloat("Size", &size, 0.03, 0, 1000, "%.3f", 2.5f);
  ImGui::Separator();
  ImGui::SetNextItemWidth(60);
  ImGui::DragFloat("Exposure", &exposure_delta, 0.005, 0.0f, 3.0f);

  if (ImGui::Button("Apply Darker"))
  {
    apply_exposure = -1;
  }
  ImGui::SameLine();
  if (ImGui::Button("Apply Brighter"))
  {
    apply_exposure = 1;
  }

  ImGui::Separator();
  ImGui::Separator();
  ImGui::Checkbox("ACES Tonemap", &postprocess_aces);

  if (ImGui::Button("Apply ACES Tonemap"))
  {
    apply_tonemap = 1;
  }

  ImGui::Separator();

  ImGui::Separator();
  ImGui::SetNextItemWidth(60);
  ImGui::DragFloat("Pow()", &pow, 0.01, 0.1f, 16.0f, "%.3f", 2.0f);

  ImGui::SameLine();
  if (ImGui::Button("Apply"))
  {
    apply_pow = 1;
  }

  ImGui::Separator();
  ImGui::Separator();
  ImGui::InputInt("Brush", &brush_selection);

  ImGui::Checkbox("Constant Density", &constant_density);
  if (constant_density)
  {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(40);
    ImGui::DragFloat("", &density, 0.1, 0.1, 100, "%.2f", 2.5f);
  }
  else
  {
    ImGui::InputInt("Smoothing Count", &smoothing_count);
  }
  put_imgui_texture_button(&preview, vec2(128));

  ImGui::SetNextItemWidth(160);
  ImGui::ColorPicker4("Color", &brush_color[0]);

  ImGui::Separator();
  ImGui::Separator();
  if (ImGui::Button("Load file"))
  {
    texture_picker.window_open = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Paste Texture"))
  {
    Texture new_texture = COPIED_TEXTURE_HANDLE;
    textures.insert(textures.begin() + selected_texture, new_texture);
    surface = change_texture_to(selected_texture);

    if (is_float_format(new_texture.texture->internalformat))
      hdr = true;
    else
      hdr = false;
  }
  if (texture_picker.run())
  {
    std::string new_texture_name = texture_picker.get_result();
    Texture_Descriptor td;
    td.name = new_texture_name;
    td.source = new_texture_name;
    td.format = GL_RGBA32F;
    Texture new_texture = td;
    new_texture.load();
    textures.insert(textures.begin() + selected_texture, new_texture);
    surface = change_texture_to(selected_texture);
  }
  if (ImGui::Button("Clone Texture"))
  {
    Texture_Descriptor td;
    td.source = "generate";
    td.size = surface->texture->size;
    td.format = surface->texture->internalformat;
    Texture new_texture(td);
    new_texture.load();
    Texture last_intermediate = fbo_intermediate.color_attachments[0];
    fbo_intermediate.color_attachments[0] = new_texture;
    fbo_intermediate.init();
    fbo_drawing.color_attachments[0] = *surface;
    fbo_drawing.init();
    blit_attachment(fbo_drawing, fbo_intermediate);
    fbo_intermediate.color_attachments[0] = last_intermediate;
    textures.insert(textures.begin() + selected_texture, new_texture);
    // surface = &textures[selected_texture];
    surface = change_texture_to(selected_texture);
  }
  ImGui::SameLine();

  // green texture means its transparent i think
  // should have the texture draw ui for the renderer split up
  // alpha channel into a 2nd black/white image
  // or just make a checkbox to swapto/use alpha

  if (ImGui::Button("Clear all"))
  {
    textures.clear();
    textures.push_back(create_new_texture(new_texture_size));
    surface = change_texture_to(0);
  }

  for (uint32 i = 0; i < textures.size(); ++i)
  {
    Texture *this_texture = &textures[i];
    ImGui::PushID(s(i).c_str());
    bool green = selected_texture == i;
    if (green)
    {
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 1, 0, 1));
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 1, 0, 1));
    }
    if (put_imgui_texture_button(this_texture, vec2(160), false))
    {
      selected_texture = i;
      surface = change_texture_to(selected_texture);
    }
    if (green)
    {
      ImGui::PopStyleColor();
      ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
    if (ImGui::Button("X"))
    {
      textures.erase(textures.begin() + i);
      if (selected_texture >= i)
        selected_texture = i - 1;
      if (selected_texture < 0)
      {
        selected_texture = 0;
        if (textures.size() == 0)
        {
          textures.push_back(create_new_texture(new_texture_size));
        }
      }
      surface = change_texture_to(selected_texture);
      this_texture = nullptr;
    }
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
    ImGui::Dummy(ImVec2(10, 45));
    if (ImGui::Button("S") && this_texture != nullptr)
    {
      ImGui::OpenPopup("Save Texture");
      filename = BASE_TEXTURE_PATH + this_texture->t.name;
    }
    ImGui::PopStyleColor();
    if (ImGui::BeginPopupModal("Save Texture", NULL, ImGuiWindowFlags_AlwaysAutoResize) && this_texture != nullptr)
    {
      std::string type = "";
      Texture *saved_texture = this_texture;
      put_imgui_texture(saved_texture, vec2(320));
      filename.resize(64);
      ImGui::InputText("filename", &filename[0], filename.size());
      filename.erase(std::find(filename.begin(), filename.end(), '\0'), filename.end());

      uint32 last_radio_state = save_type_radio_button_state;
      ImGui::Text("Image file type:");
      ImGui::RadioButton("HDR", &save_type_radio_button_state, 0);
      ImGui::SameLine();
      ImGui::RadioButton("PNG", &save_type_radio_button_state, 1);

      if (save_type_radio_button_state == 0)
      {
        type = "hdr";
      }
      if (save_type_radio_button_state == 1)
      {
        type = "png";
      }
      std::string extension = s(".", type);
      bool file_exists = std::filesystem::exists(s(filename, extension));
      ImGui::Separator();
      if (file_exists)
      {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
        ImGui::Text("File exists!");
        if (ImGui::Button("Overwrite", ImVec2(120, 0)))
        {
          int32 save_result = save_texture_type(saved_texture, filename, type);
          ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
        if (ImGui::Button("Generate Filename", ImVec2(120, 0)))
        {
          int32 save_result = save_texture_type(saved_texture, find_free_filename(filename, extension), type);
          ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
      }
      else if (ImGui::Button("Save", ImVec2(120, 0)))
      {
        int32 save_result = save_texture_type(saved_texture, filename, type);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SetItemDefaultFocus();

      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0)))
      {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::EndGroup();

    ImGui::PopID();
  }

  ImGui::EndChild();
  ImGui::SameLine();
  const ImVec2 childo = ImGui::GetCursorPos();
  ivec2 childoffset = vec2(childo.x, childo.y);
  // ImGuiWindowFlags childflags = ImGuiWindowFlags_HorizontalScrollbar;
  ImGuiWindowFlags childflags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, .2));
  ImGui::BeginChild("paintbox", ImVec2(0, 0), false, childflags);
  ImGui::PopStyleColor();
  bool is_within = ImGui::IsWindowHovered();
  // ImGui::Dummy(ImVec2(0.0f, 120.0f));
  // ImGui::Dummy(ImVec2(120.0f, 0.f));
  // ImGui::SameLine();
  ivec2 texture_size = zoom * vec2(surface->texture->size);
  const ImVec2 imgui_draw_cursor_pos = ImGui::GetCursorPos();
  ivec2 window_position_for_texture = ivec2(imgui_draw_cursor_pos.x, imgui_draw_cursor_pos.y);
  glGenerateTextureMipmap(display_surface.texture->texture);

  put_imgui_texture(&display_surface, ivec2(texture_size), false);

  // ImGui::SameLine();
  // ImGui::Dummy(ImVec2(120.0f, 0.f));
  // ImGui::Dummy(ImVec2(0.0f, 120.0f));
  if (imgui_visit_count == 2)
  {
    ImGui::SetScrollX(0.5f * ImGui::GetScrollMaxX());
    ImGui::SetScrollY(0.5f * ImGui::GetScrollMaxY());
  }
  const ivec2 scroll = vec2(ImGui::GetScrollX(), ImGui::GetScrollY());

  float32 zoom_step = 0.051f;
  for (SDL_Event &e : *imgui_event_accumulator)
  {
    if (is_within && e.type == SDL_MOUSEWHEEL)
    {
      if (e.wheel.y < 0)
        zoom -= zoom_step;
      else if (e.wheel.y > 0)
        zoom += zoom_step;
    }
  }

  zoom = clamp(zoom, 0.1f, 10.f);

  ivec2 texture_position_within_window = window_position_for_texture;
  const vec2 cursor_pos_within_texture = window_mouse_pos - texture_position_within_window - childoffset + scroll;

  // mat4 scroll = translate(vec3(cursor_pos_within_texture, 0));

  bool out_of_texture = cursor_pos_within_texture.x < 0 || cursor_pos_within_texture.x > texture_size.x ||
                        cursor_pos_within_texture.y < 0 || cursor_pos_within_texture.y > texture_size.y;

  // ImGui::Text(s(cursor_pos_within_texture.x, " ", cursor_pos_within_texture.y).c_str());
  vec2 ndc_cursor = cursor_pos_within_texture / vec2(texture_size);
  ndc_cursor = 2.0f * ndc_cursor;
  ndc_cursor = ndc_cursor - vec2(1);

  ImGui::Text(s("ndc:", ndc_cursor).c_str());

  ImVec2 imouse_delta = ImGui::GetIO().MouseDelta;
  vec2 mouse_delta = vec2(imouse_delta.x, imouse_delta.y);
  ImGuiContext &g = *ImGui::GetCurrentContext();
  ImGuiWindow *window = g.CurrentWindow;

  bool hovered = false;
  bool middle_mouse_held = false;
  uint32 data = (uint32)(IMGUI_TEXTURE_DRAWS.size() - 1) | 0xf0000000;

  // if (g.HoveredId == 0) // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
  ImGui::ButtonBehavior(window->Rect(), window->GetID("Texturasde_Paint"), &hovered, &middle_mouse_held,
      ImGuiButtonFlags_MouseButtonMiddle);
  if (middle_mouse_held)
  {
    ImGui::SetScrollX(scroll.x - mouse_delta.x);
    ImGui::SetScrollY(scroll.y - mouse_delta.y);
  }

  while (sim_time + dt < time)
  {
    // iterate(surface, sim_time);
    sim_time += dt;
  }

  blit_attachment(fbo_drawing, fbo_intermediate);

  GLint p;
  glGetNamedFramebufferAttachmentParameteriv(
      fbo_drawing.fbo->fbo, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &p);

  if (p == GL_LINEAR)
  {
    glDisable(GL_FRAMEBUFFER_SRGB);
  }
  if (p == GL_SRGB)
  {
    glEnable(GL_FRAMEBUFFER_SRGB);
  }

  glViewport(0, 0, surface->texture->size.x, surface->texture->size.y);
  fbo_drawing.bind_for_drawing_dst();
  intermediate.bind_for_sampling_at(0);
  drawing_shader.use();
  drawing_shader.set_uniform("texture0_mod", vec4(1));
  drawing_shader.set_uniform("transform", fullscreen_quad());
  drawing_shader.set_uniform("mouse_pos", ndc_cursor);
  drawing_shader.set_uniform("size", size);
  drawing_shader.set_uniform("intensity", intensity);
  drawing_shader.set_uniform("brush_exponent", exponent);
  drawing_shader.set_uniform("hdr", (int32)hdr);
  drawing_shader.set_uniform("brush_selection", (int32)brush_selection);
  drawing_shader.set_uniform("brush_color", brush_color);
  drawing_shader.set_uniform("time", time);
  drawing_shader.set_uniform("blendmode", blendmode);
  drawing_shader.set_uniform("tonemap_pow", pow);
  drawing_shader.set_uniform("aspect", float32(surface->texture->size.x) / float32(surface->texture->size.y));
  drawing_shader.set_uniform("mask", vec4(mask));

  if (is_within)
  {
    bool draw = false;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
      drawing_shader.set_uniform("mode", 1);
      // quad.draw();
      draw = true;
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
      drawing_shader.set_uniform("mode", 2);
      draw = true;
    }
    if (!draw)
    {
      is_new_click = true;
    }

    float32 dt = max(0.00001f, time - last_run_visit_time);
    float32 d = length(last_drawn_ndc - ndc_cursor);
    float32 speed = d / dt;
    float32 deltaspeed = abs(speed - previous_speed);
    float32 d_curve = zoom * glm::pow(d, .50f);

    if (d == 0 && !is_new_click)
    {
      draw = false;
    }

    if (draw)
    {
      if (is_new_click || !constant_density)
      {
        if (!is_new_click && !constant_density)
        {
          fbo_drawing.bind_for_drawing_dst();
          intermediate.bind_for_sampling_at(0);

          uint32 smoothing_iterations = uint32(floor(float32(smoothing_count) * d_curve));
          smoothing_iterations = glm::max(smoothing_iterations, (uint32)1);
          set_message("smoothing_iterations", s(smoothing_iterations), 1.0f);
          for (uint32 i = 0; i < smoothing_iterations; ++i)
          {
            if (i > 100)
              break;

            vec2 p = mix(last_drawn_ndc, ndc_cursor, float32(i + 1) / smoothing_iterations);
            intermediate.bind_for_sampling_at(0);
            fbo_drawing.bind_for_drawing_dst();
            drawing_shader.use();
            drawing_shader.set_uniform("mouse_pos", p);

            vec4 calculated_brush_color = brush_color;
            if (blendmode == 2) // add
            {
              calculated_brush_color = (1.0f / smoothing_iterations) * brush_color;
            }
            float32 t_of_stroke = ((float32(i) + 1.f) / smoothing_iterations);
            // float32 adjusted_speed = mix(d*previous_speed,speed,t_of_stroke);
            float32 adjusted_speed = mix(previous_speed, speed, t_of_stroke);
            vec4 mul = vec4(vec3(adjusted_speed), 1.0f);
            calculated_brush_color = mul * calculated_brush_color;

            drawing_shader.set_uniform("brush_color", calculated_brush_color);
            if (i == smoothing_iterations - 1) // actual mouse pos
            {
              // drawing_shader.set_uniform("brush_color", vec4(1, 0, 0, 1));
            }
            if (last_drawn_ndc != ndc_cursor)
            { // we sometimes get the same cursor position even if we're moving
              quad.draw();
            }
            bool not_last_iteration = (i + 1) < smoothing_iterations;
            if (not_last_iteration)
            {
              fbo_intermediate.bind_for_drawing_dst();
              glViewport(0, 0, surface->texture->size.x, surface->texture->size.y);
              surface->bind_for_sampling_at(0);
              copy.use();
              quad.draw();
            }
          }
          last_drawn_ndc = ndc_cursor;
          is_new_click = false;
          accumulator = 0.0f;
        }
        else
        {
          // drawing_shader.set_uniform("brush_color", vec4(0, 1, 0, 1));
          drawing_shader.set_uniform("mouse_pos", ndc_cursor);
          drawing_shader.set_uniform("brush_color", brush_color);
          quad.draw();

          is_new_click = false;
          last_drawn_ndc = ndc_cursor;
          accumulator = 0.0f;
          last_run_visit_time = time;
        }
      }
      else
      {
        const float32 draw_dt = .1f / density;
        const vec2 d = ndc_cursor - last_drawn_ndc;
        const float32 len = length(d);
        const vec2 nd = normalize(d);
        const vec2 step = draw_dt * nd;
        const vec2 start = last_drawn_ndc + (draw_dt * nd);
        const bool mouse_stationary = last_seen_mouse_ndc == ndc_cursor;
        const bool too_short = length(start - ndc_cursor) < draw_dt;
        if (too_short || mouse_stationary || len == 0)
        {
          // return;
        }
        else
        {
          accumulator += length(start - ndc_cursor);
          ASSERT(len != 0);
          bool first_draw = true;
          vec2 draw_pos = start;
          uint8 count = 0;
          while (accumulator >= draw_dt)
          {
            if (count > 200)
            {
              break;
            }
            fbo_drawing.bind_for_drawing_dst();
            intermediate.bind_for_sampling_at(0);
            drawing_shader.use();
            // drawing_shader.set_uniform("brush_color", 4.f * accumulator * vec4(0, 1., 0, 1));
            drawing_shader.set_uniform("mouse_pos", draw_pos);
            if (first_draw)
            {
              // drawing_shader.set_uniform("brush_color", vec4(1, 0, 0, 1));
              first_draw = false;
            }
            quad.draw();
            // drawing_shader.set_uniform("brush_color", 10.f * accumulator * vec4(0, 5., 0, 1));
            last_drawn_ndc = draw_pos;
            draw_pos = draw_pos + step;
            accumulator -= draw_dt;
            ++count;
            bool not_last_iteration = accumulator >= draw_dt;
            if (not_last_iteration)
            {
              fbo_intermediate.bind_for_drawing_dst();
              glViewport(0, 0, surface->texture->size.x, surface->texture->size.y);
              surface->bind_for_sampling_at(0);
              copy.use();
              quad.draw();
            }
          }
        }
      }
    }

    if (!(d == 0 && !is_new_click))
    {
      previous_speed = speed;
    }
  }

  last_seen_mouse_ndc = ndc_cursor;

  if (clear)
  {
    if (clear == 1)
      drawing_shader.set_uniform("mode", 0);
    if (clear == 2)
    {
      drawing_shader.set_uniform("mode", 6);
      drawing_shader.set_uniform("brush_color", clear_color);
    }

    quad.draw();
    clear = 0;
  }

  if (apply_exposure != 0)
  {
    drawing_shader.set_uniform("tonemap_x", 1.0f + (apply_exposure * exposure_delta));
    drawing_shader.set_uniform("mode", 4);
    quad.draw();
    apply_exposure = 0;
  }

  if (apply_tonemap)
  {
    drawing_shader.set_uniform("mode", 5);
    quad.draw();
    apply_tonemap = 0;
  }

  if (apply_pow)
  {
    drawing_shader.set_uniform("mode", 7);
    quad.draw();
    apply_pow = 0;
  }
  if (custom_draw_mode_set != 0)
  {
    drawing_shader.set_uniform("mode", custom_draw_mode_set);
    quad.draw();
    custom_draw_mode_set = 0;
  }

  glViewport(0, 0, preview.texture->size.x, preview.texture->size.y);
  WHITE_TEXTURE.bind_for_sampling_at(0);
  fbo_preview.bind_for_drawing_dst();
  drawing_shader.use();
  drawing_shader.set_uniform("texture0_mod", clear_color);
  drawing_shader.set_uniform("transform", fullscreen_quad());
  drawing_shader.set_uniform("mouse_pos", vec2(0));
  drawing_shader.set_uniform("size", 450.f);
  drawing_shader.set_uniform("mode", 3);

  drawing_shader.set_uniform("aspect", 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  quad.draw();

  glViewport(0, 0, display_surface.texture->size.x, display_surface.texture->size.y);
  surface->bind_for_sampling_at(0);
  fbo_display.bind_for_drawing_dst();
  postprocessing_shader.use();
  postprocessing_shader.set_uniform("transform", fullscreen_quad());
  postprocessing_shader.set_uniform("size", size);
  postprocessing_shader.set_uniform("zoom", zoom);
  postprocessing_shader.set_uniform("time", time);
  postprocessing_shader.set_uniform("tonemap_aces", (int32)postprocess_aces);
  postprocessing_shader.set_uniform("mouse_pos", draw_cursor ? ndc_cursor : vec2(-9999999));
  postprocessing_shader.set_uniform("aspect", float32(surface->texture->size.x) / float32(surface->texture->size.y));
  glClear(GL_COLOR_BUFFER_BIT);
  quad.draw();

  glDisable(GL_FRAMEBUFFER_SRGB);

  glGenerateTextureMipmap(surface->texture->texture);
  ImGui::EndChild();
  // ImGui::End();
  last_run_visit_time = time;
}

void Liquid_Surface::init(State *state, vec3 pos, float32 scale, ivec2 resolution)
{
  vec3 size = vec3(scale);
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  ASSERT(!initialized);
  initialized = true;
  liquid_shader = Shader("passthrough.vert", "liquid.frag");
  Mesh_Descriptor md(plane, "Liquid_Surface's quad");
  quad = Mesh(md);

  Mesh_Descriptor mesh;
  mesh.name = "Liquid_Surface generated grid";
  mesh.mesh_data = generate_grid(resolution);
  Material_Descriptor material;

  material.emissive.mod = vec4(0, 0, 0.005, 1);
  material.albedo.mod = vec4(.034, .215, .289, .75351);
  material.uv_scale = vec2(1);
  material.roughness.source = "white";
  material.roughness.mod = vec4(0.054);
  material.metalness.mod = vec4(1);
  material.vertex_shader = "displacement.vert";
  material.frag_shader = "water.frag";
  material.backface_culling = true;
  material.translucent_pass = true;
  material.ior = 1.2f;
  material.uniform_set.bool_uniforms["ground"] = false;

  water = state->scene.add_mesh("water", &mesh, &material);
  state->scene.nodes[water].scale = size;
  state->scene.nodes[water].position = pos - vec3(0, 0, 0.1);
  material.frag_shader = "terrain.frag";

  // specific for terrain shader
  material.albedo.source = "grass_albedo.png";
  material.normal.source = "ground1_normal.png";
  material.emissive.source = "Snow006_2K_Normal.jpg";
  material.uv_scale = vec2(85);
  material.translucent_pass = false;
  material.uniform_set.bool_uniforms["ground"] = true;
  ground = state->scene.add_mesh("ground", &mesh, &material);
  state->scene.nodes[ground].scale = size;
  state->scene.nodes[ground].position = pos;

  Material_Index mi = state->scene.nodes[ground].model[0].second;
  Material_Descriptor *groundmat = &state->scene.resource_manager->material_pool[mi].descriptor;
  if (groundmat->derivative_offset == 0.0)
  {
    set_message("Liquid_Surface::init bad offset after add", "", 330.f);
  }

  this->state = state;

  heightmap_resolution = resolution;

  painter.new_texture_size = resolution;
  painter.textures.clear();
  painter.textures.push_back(painter.create_new_texture(painter.new_texture_size));
  painter.change_texture_to(0);
  set_heightmap(painter.textures[0]);
}

Liquid_Surface::~Liquid_Surface()
{
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  glDeleteBuffers(1, &heightmap_pbo);
  glDeleteBuffers(1, &velocity_pbo);
  glDeleteSync(read_sync);
  state->scene.delete_node(water);
  state->scene.delete_node(ground);
}

void Liquid_Surface::set_heightmap(Texture texture)
{
  while (!texture.texture || !texture.texture->texture)
  {
    texture.load();
  }
  ASSERT(texture.texture->size != ivec2(0));
  ASSERT(texture.texture && texture.texture->texture);
  heightmap_resolution = texture.texture->size;
  heightmap = heightmap_fbo.color_attachments[0] = texture;
  heightmap_fbo.init();

  glDeleteSync(read_sync);
  read_sync = 0;
  glDeleteBuffers(1, &heightmap_pbo);
  glDeleteBuffers(1, &velocity_pbo);
  current_buffer_size = 0;

  glCreateBuffers(1, &heightmap_pbo);
  glCreateBuffers(1, &velocity_pbo);

  velocity.t.size = texture.texture->size;
  velocity.t.format = texture.texture->internalformat;
  velocity.t.name = "Liquid_Surface::velocity";
  velocity.t.source = "generate";
  velocity = Texture(velocity.t);
  velocity.load();
  velocity_fbo.color_attachments[0] = velocity;
  velocity_fbo.init();

  heightmap_copy.t.size = texture.texture->size;
  heightmap_copy.t.format = texture.texture->internalformat;
  heightmap_copy.t.name = "Liquid_Surface::heightmap_copy";
  heightmap_copy.t.source = "generate";
  heightmap_copy = Texture(heightmap_copy.t);
  heightmap_copy.load();
  copy_heightmap_fbo.color_attachments[0] = heightmap_copy;
  copy_heightmap_fbo.init();

  velocity_copy.t.size = texture.texture->size;
  velocity_copy.t.format = texture.texture->internalformat;
  velocity_copy.t.name = "Liquid_Surface::velocity_copy";
  velocity_copy.t.source = "generate";
  velocity_copy = Texture(velocity_copy.t);
  velocity_copy.load();
  copy_velocity_fbo.color_attachments[0] = velocity_copy;
  copy_velocity_fbo.init();

  liquid_shader_fbo.color_attachments.resize(2);
  liquid_shader_fbo.color_attachments[0] = heightmap;
  liquid_shader_fbo.color_attachments[1] = velocity;
  liquid_shader_fbo.init();

  zero_velocity();
  blit_attachment(heightmap_fbo, copy_heightmap_fbo);
  blit_attachment(velocity_fbo, copy_velocity_fbo);

  Texture_Descriptor td;
  td.size = desired_download_resolution;
  td.format = heightmap.texture->internalformat;
  td.source = "generate";
  td.name = "Liquid_Surface::height_dst";

  height_dst = Texture(td);
  height_dst.load();
  heightmap_resize_fbo.color_attachments[0] = height_dst;
  heightmap_resize_fbo.init();

  td.name = "Liquid_Surface::velocity_dst";
  velocity_dst = Texture(td);
  velocity_dst.load();
  velocity_resize_fbo.color_attachments[0] = velocity_dst;
  velocity_resize_fbo.init();

  Scene_Graph_Node *node = &state->scene.nodes[water];
  Material_Index mi = node->model[0].second;
  Material *mat = &state->scene.resource_manager->material_pool[mi];
  if (mat->descriptor.derivative_offset == 0.0)
  {
    set_message("Liquid_Surface::set_heightmap bad offset", "", 330.f);
  }
  mat->load();
  mat->displacement = heightmap;
  mat->descriptor.uniform_set.texture_uniforms[water_velocity] = velocity_fbo.color_attachments[0];
  mat->descriptor.uniform_set.uint32_uniforms["displacement_map_size"] = heightmap_resolution.x;
  node = &state->scene.nodes[ground];
  mi = node->model[0].second;
  mat = &state->scene.resource_manager->material_pool[mi];
  mat->load();
  mat->displacement = heightmap;
  mat->descriptor.uniform_set.uint32_uniforms["displacement_map_size"] = heightmap_resolution.x;
}

void Liquid_Surface::generate_geometry_from_heightmap(vec4 *heightmap_pixel_array, vec4 *velocity_pixel_array)
{
  if (last_generated_terrain_geometry_resolution != desired_download_resolution)
  {
    terrain_geometry = {std::string("Liquid_Surface cpu terrain"), generate_grid(desired_download_resolution)};
    last_generated_terrain_geometry_resolution = desired_download_resolution;
  }
  if (!heightmap_pixel_array)
  {
    ASSERT(heightmap_pixels.size() > 0);
    heightmap_pixel_array = &heightmap_pixels[0];
  }
  if (!velocity_pixel_array)
  {
    ASSERT(velocity_pixels.size() > 0);
    velocity_pixel_array = &velocity_pixels[0];
  }

  for (uint32 i = 0; i < terrain_geometry.mesh_data.positions.size(); ++i)
  {
    vec3 *pos = &terrain_geometry.mesh_data.positions[i];
    vec2 uv = terrain_geometry.mesh_data.texture_coordinates[i];
    ivec2 &size = desired_download_resolution;

    float32 eps = 0.0001f;
    vec2 sample_p = (vec2(size - 1) * uv) + vec2(eps);
    sample_p = floor(sample_p);
    sample_p = clamp(sample_p, vec2(0), vec2(size));

    // p = [0,1]
    // size = (3,3)
    // samplep = [0,3]

    // texture:
    /*
    x x x
    x x x
    0 x x
    */

    // reads into array as
    /*
    0 x x
    x x x
    x x x
    */

    // origin is the same at 0,0

    /*
    6 7 8
    3 4 5
    0 1 2
    */
    uint32 rows = (uint32)sample_p.y;
    uint32 cols = (uint32)sample_p.x;

    bool out_of_bounds = (cols > uint32(heightmap.t.size.x) - 1) || (rows > uint32(heightmap.t.size.y) - 1);

    uint32 index = rows * uint32(size.x) + cols;

    float32 ground_height = 0;
    float32 water_height = 0;
    float32 biome = 0;
    vec4 vel_pack = vec4(0);
    if (!out_of_bounds)
    {
      ground_height = heightmap_pixel_array[index].g;
      water_height = heightmap_pixel_array[index].r;
      biome = velocity_pixel_array[index].b;
      vel_pack = velocity_pixel_array[index];
    }
    pos->z = ground_height;
  }
  last_generated_terrain_tick += 1;
}

void Liquid_Surface::zero_velocity()
{
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  velocity_fbo.init();
  velocity_fbo.bind_for_drawing_dst();
  glViewport(
      0, 0, velocity_fbo.color_attachments[0].texture->size.x, velocity_fbo.color_attachments[0].texture->size.y);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
}
bool Liquid_Surface::apply_geometry_to_octree_if_necessary(Scene_Graph *scene)
{

  if ((last_applied_terrain_tick < last_generated_terrain_tick))
  {
    scene->collision_octree.clear();
    mat4 m = scene->build_transformation(ground);
    scene->collision_octree.push(&terrain_geometry, &m);
    last_applied_terrain_tick = last_generated_terrain_tick;
    return true;
  }
  return false;
}

bool Liquid_Surface::finish_texture_download_and_generate_geometry()
{
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  GLint ready;
  glGetSynciv(read_sync, GL_SYNC_STATUS, 1, NULL, &ready);
  if (ready != GL_SIGNALED)
  {
    return false;
  }
  glDeleteSync(read_sync);
  read_sync = 0;
  float32 *heightmap_ptr = (float32 *)glMapNamedBuffer(heightmap_pbo, GL_READ_ONLY);
  float32 *velocity_ptr = (float32 *)glMapNamedBuffer(velocity_pbo, GL_READ_ONLY);
  generate_geometry_from_heightmap((vec4 *)heightmap_ptr, (vec4 *)velocity_ptr);
  glUnmapNamedBuffer(heightmap_pbo);
  glUnmapNamedBuffer(velocity_pbo);
  return true;
}

bool Liquid_Surface::finish_texture_download()
{
  GLint ready;
  glGetSynciv(read_sync, GL_SYNC_STATUS, 1, NULL, &ready);
  if (ready != GL_SIGNALED)
  {
    return false;
  }
  glDeleteSync(read_sync);
  read_sync = 0;
  float32 *heightmap_ptr = (float32 *)glMapNamedBuffer(heightmap_pbo, GL_READ_ONLY);
  float32 *velocity_ptr = (float32 *)glMapNamedBuffer(velocity_pbo, GL_READ_ONLY);
  uint32 pixel_count = heightmap.t.size.x * heightmap.t.size.y;
  uint32 subpixel_count = 4 * pixel_count;
  heightmap_pixels.clear();
  velocity_pixels.clear();
  heightmap_pixels.reserve(pixel_count);
  velocity_pixels.reserve(pixel_count);
  for (uint32 i = 0; i < subpixel_count; i = i + 4)
  {
    heightmap_pixels.push_back(*(vec4 *)(&heightmap_ptr[i]));
  }
  for (uint32 i = 0; i < subpixel_count; i = i + 4)
  {
    velocity_pixels.push_back(*(vec4 *)(&velocity_ptr[i]));
  }
  glUnmapNamedBuffer(heightmap_pbo);
  glUnmapNamedBuffer(velocity_pbo);
  return true;
}

void Liquid_Surface::start_texture_download()
{
  bool resize_to_desired_resolution = true;
  if (resize_to_desired_resolution)
  {
    heightmap_fbo.init();
    blit_attachment(heightmap_fbo, heightmap_resize_fbo);
    blit_attachment(velocity_fbo, velocity_resize_fbo);

    ASSERT(read_sync == 0);
    uint32 buff_size = heightmap_resize_fbo.color_attachments[0].t.size.x *
                       heightmap_resize_fbo.color_attachments[0].t.size.y * 4 * sizeof(float32);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, heightmap_pbo);
    if (current_buffer_size != buff_size)
    {
      glBufferData(GL_PIXEL_PACK_BUFFER, buff_size, 0, GL_DYNAMIC_READ);
    }
    glGetTextureImage(heightmap_resize_fbo.color_attachments[0].texture->texture, 0, GL_RGBA, GL_FLOAT, buff_size, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, velocity_pbo);
    if (current_buffer_size != buff_size)
    {
      glBufferData(GL_PIXEL_PACK_BUFFER, buff_size, 0, GL_DYNAMIC_READ);
    }
    glGetTextureImage(velocity_resize_fbo.color_attachments[0].texture->texture, 0, GL_RGBA, GL_FLOAT, buff_size, 0);
    read_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    current_buffer_size = buff_size;
    return;
  }

  ASSERT(read_sync == 0);
  uint32 buff_size =
      heightmap_fbo.color_attachments[0].t.size.x * heightmap_fbo.color_attachments[0].t.size.y * 4 * sizeof(float32);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, heightmap_pbo);
  if (current_buffer_size != buff_size)
  {
    glBufferData(GL_PIXEL_PACK_BUFFER, buff_size, 0, GL_DYNAMIC_READ);
  }
  glGetTextureImage(heightmap_fbo.color_attachments[0].texture->texture, 0, GL_RGBA, GL_FLOAT, buff_size, 0);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, velocity_pbo);
  if (current_buffer_size != buff_size)
  {
    glBufferData(GL_PIXEL_PACK_BUFFER, buff_size, 0, GL_DYNAMIC_READ);
  }
  glGetTextureImage(velocity_fbo.color_attachments[0].texture->texture, 0, GL_RGBA, GL_FLOAT, buff_size, 0);
  read_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  current_buffer_size = buff_size;
}

void Liquid_Surface::run(State *state)
{
  // painter.run(state->imgui_event_accumulator);
  // return;

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
  ImGui::Begin("Liquid_Surface", &window_open, window_flags);
  if (ImGui::Button("Generate Terrain"))
  {
    painter.custom_draw_mode_set = 8;
  }
  ImGui::SameLine();
  if (ImGui::Button("Generate Water"))
  {
    painter.custom_draw_mode_set = 9;
  }
  if (ImGui::Button("Clear Water"))
  {
    painter.custom_draw_mode_set = 10;
  }
  if (ImGui::Button("Clear"))
  {
    zero_velocity();
    painter.clear = 2;
  }
  ImGui::SameLine();
  if (ImGui::Button("Generate Terrain Octree") || want_texture_download)
  {
    start_texture_download();
    want_texture_download = false;
  }
  if (read_sync != 0)
  { // if finish is successful, read_sync will be 0 until the next download request
    bool success = finish_texture_download_and_generate_geometry();
  }

  // if the grid is smaller than the source heightmap
  // we will resize the texture to the size of the grid
  // and put that texture in the painter
  // rather than leaving the oversized one in there
  if (ImGui::Button("Set Heightmap To Downloaded"))
  {
    start_texture_download();
    while (!finish_texture_download_and_generate_geometry())
    {
    }
    painter.new_texture_size = heightmap_resize_fbo.color_attachments[0].t.size;
    painter.textures.push_back(heightmap_resize_fbo.color_attachments[0]);
    painter.change_texture_to(uint32(painter.textures.size()) - 1);
    set_heightmap(heightmap_resize_fbo.color_attachments[0]);
  }

  ImGui::InputInt("Water Iterations", &iterations);
  iterations = max(iterations, 0);
  ImGui::DragFloat3("Ambient Waves", &ambient_wave_scale[0], 0.05f, 0.0f, 100.f, "%.3f", 1.5f);

  if (!heightmap_fbo.color_attachments[0].texture || !heightmap_fbo.color_attachments[0].texture->texture)
  {
    return;
  }

  bool automatically_swap_to_painter_selected_texture = true;
  if (automatically_swap_to_painter_selected_texture)
  {
    if (painter.textures.size() > 0)
    {
      bool painter_has_valid_texture = painter.textures[painter.selected_texture].texture != nullptr;
      if (painter_has_valid_texture)
      {
        bool painter_changed_texture =
            heightmap.texture->texture != painter.textures[painter.selected_texture].texture->texture;
        if (painter_changed_texture)
        {
          set_heightmap(painter.textures[painter.selected_texture]);
        }
      }
    }
  }
  for (int32 i = 0; i < iterations; ++i)
  {
    my_time = my_time + dt;

    blit_attachment(heightmap_fbo, copy_heightmap_fbo);
    blit_attachment(velocity_fbo, copy_velocity_fbo);

    liquid_shader_fbo.bind_for_drawing_dst();
    copy_heightmap_fbo.color_attachments[0].bind_for_sampling_at(0);
    copy_velocity_fbo.color_attachments[0].bind_for_sampling_at(1);
    liquid_shader.use();
    liquid_shader.set_uniform("transform", fullscreen_quad());
    liquid_shader.set_uniform("time", float32(my_time));
    liquid_shader.set_uniform("size", vec2(copy_heightmap_fbo.color_attachments[0].t.size));
    liquid_shader.set_uniform("dt", dt);
    liquid_shader.set_uniform(
        "ambient_wave_scale", 0.001f * ambient_wave_scale.z * vec2(ambient_wave_scale.x, ambient_wave_scale.y));
    quad.draw();
  }

  painter.run(state->imgui_event_accumulator);

  ImGui::End();
}

/*




float height[N*M];
float vertical_derivative[N*M];
float previous_height[N*M];
// ... initialize to zero ...
// ... begin loop over frames ...
// --- This is the propagation code ---
// Convolve height with the kernel
// and put it into vertical_derivative
Convolve( height, vertical_derivative );
float temp;
for(int k=0;k<N*M;k++)
{
temp = height[k];
height[k] = height[k]*(2.0-alpha*dt)/(1.0+alpha*dt)- previous_height[k]/(1.0+alpha*dt)-
vertical_derivative[k]*g*dt*dt/(1.0+alpha*dt); previous_height[k] = temp;
}
// --- end propagation code ---
// ... end loop over frames ...



h(x,y,t) is height (respect to mean) at pixel x,y at time t


-g* is gravitational restoring force



sqrt(-delta^2) is mass conservation operator, aka vertical derivative of the surface

we need to evaluate this as a convolution


kernel generation:

uint32 WIDTH = 7;
float32 G[WIDTH][WIDTH] = {0};

G0 = 0
for(uint32 n = 0; n < 10000; ++n)
{
float o = 1.0f
float a = (n * 0.001)
G0 += (a*a) * exp(-o * (a*a));
}
for(uint32 y = 0; y < WIDTH; ++y)
{
for(uint32 x = 0; x < WIDTH; ++x)
{
for(uint32 n = 0; n < 10000;++n)
{
  float r = sqrt(x*x+y*y);
  float a = (n * 0.001)
  G[y][x] += (a*a) * exp(-o * (a*a)) * J(a*r) / G0
}
}
}











convolution:

just as with the gaussian shader, we will have a look up table of values that are calculated
and we use them just the same as the guass shader, where we iterate in two dimensions with iterators offsetx and
offsety

where p is this pixels position:
vertical_derivative_of_pixel = G(offsetx,offsety) * h(p.x+offsetx,p.y+offsety);

the number of iterations on the offsets, which is the width of the kernel, affects the quality


however we need to sample in the full square kernel here, unlike the gauss shader that can do one axis at a time to
save work








propagation:

per pix:

result = heightsample *(2.0-alpha*dt) -  (previous_frame_height_sample)/(1+alpha*dt) - vertical_derivative_sample
*g*dt*dt / (1.0+alpha*dt); previous_frame_height = heightsample return result






so in summary,
G(x,y) is a kernel sample at x,y
h(x,y) is a heightmap sample at x,y


1): we precompute the kernel like the gauss shader, except it will be a full 2d lut
2): we store a texture for the vertical derivative and compute it with:
  inputs required: heightmap, kernel LUT (can be hardcoded in the shader code)
  vertical_derivative_of_pixel = G(offsetx,offsety) * h(p.x+offsetx,p.y+offsety);
3): we copy the heightmap texture, we will need it last frame
4): we propagate the waves
  inputs required: vertical derivative texture, heightmap, previous_heightmap
  output: heightmap

  code:
  g is gravity
  a = heightsample*(2.0-alpha*dt)
  b = previous_frame_height_sample/(1+alpha*dt)
  c = vertical_derivative_sample*g*dt*dt / (1.0+alpha*dt)
  result = a - b - c;






**/
//
//// const uint32 WIDTH = 2 * 7;
// const int32 P = 7;
// static float32 G[(2 * P) + 1][(2 * P) + 1] = {0};

// const float32 o = 1.0f;
// static float32 G0 = 0;

// static bool computed = false;
// if (!computed)
//{
//  computed = true;
//  for (int32 n = 1; n <= 10000; ++n)
//  {
//    float32 qn = 0.001f * n;
//    float32 qsqr = qn * qn;
//    G0 = G0 + (qsqr * exp(-o * qsqr));
//  }
//  for (int32 l = -P; l <= P; ++l)
//  {
//    for (int32 k = -P; k <= P; ++k)
//    {
//      float32 r = sqrt(k * k + l * l);
//      int32 xi = P + k;
//      int32 yi = P + l;
//      for (int32 n = 1; n <= 10000; ++n)
//      {
//        float32 qn = 0.001f * n;
//        float32 qsqr = qn * qn;
//        G[yi][xi] = G[yi][xi] + (qsqr * exp(-o * qsqr) * j0(qn * r));
//      }
//      G[yi][xi] = G[yi][xi] / G0;
//    }
//  }
//}

// ImGui::PlotLines("plotterboi", &G[0][0], 2 * P + 1, 0, "overlaytext", -2, 2, ImVec2(320, 320));
