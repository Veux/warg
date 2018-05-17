#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include <SDL2/SDL.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
using namespace glm;
std::mt19937 generator;
const float32 dt = 1.0f / 150.0f;
const float32 MOVE_SPEED = 5.0f * dt;
const float32 MOUSE_X_SENS = .0041f;
const float32 MOUSE_Y_SENS = .0041f;
const float32 ZOOM_STEP = 0.5f;
const float32 ATK_RANGE = 5.0f;
const float32 JUMP_IMPULSE = 4.0f;

const std::string BASE_ASSET_PATH = ROOT_PATH + "Assets/";
const std::string BASE_TEXTURE_PATH =
    BASE_ASSET_PATH + std::string("Textures/");
const std::string BASE_SHADER_PATH = BASE_ASSET_PATH + std::string("Shaders/");
const std::string BASE_MODEL_PATH = BASE_ASSET_PATH + std::string("Models/");
const std::string ERROR_TEXTURE_PATH = BASE_TEXTURE_PATH + "err.png";
Timer PERF_TIMER = Timer(1000);
Timer FRAME_TIMER = Timer(60);
Timer SWAP_TIMER = Timer(60);
Config CONFIG;
float32 wrap_to_range(const float32 input, const float32 min, const float32 max)
{
  const float32 range = max - min;
  const float32 offset = input - min;
  return (offset - (floor(offset / range) * range)) + min;
}

static Assimp::Importer importer;

const int default_assimp_flags = aiProcess_FlipWindingOrder |
                                 // aiProcess_Triangulate | 
                                 // aiProcess_FlipUVs |
                                 aiProcess_CalcTangentSpace |
                                 // aiProcess_MakeLeftHanded|
                                 // aiProcess_JoinIdenticalVertices |
                                 // aiProcess_PreTransformVertices |
                                 // aiProcess_GenUVCoords |
                                 // aiProcess_OptimizeGraph|
                                 // aiProcess_ImproveCacheLocality|
                                 // aiProcess_OptimizeMeshes|
                                 // aiProcess_GenNormals|
                                 // aiProcess_GenSmoothNormals|
                                 // aiProcess_FixInfacingNormals |
                                 0;

const aiScene *load_aiscene(std::string path, const int *assimp_flags)
{
  if (!assimp_flags)
    assimp_flags = &default_assimp_flags;
  path = BASE_MODEL_PATH + path;
  int flags = *assimp_flags;
  auto p = importer.ReadFile(path.c_str(), flags);
  if (!p || p->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !p->mRootNode)
  {
    set_message("ERROR::ASSIMP::", importer.GetErrorString());
    ASSERT(0);
  }
  return p;
}

bool all_equal(int32 a, int32 b, int32 c) { return (a == b) && (a == c); }
bool all_equal(int32 a, int32 b, int32 c, int32 d)
{
  return (a == b) && (a == c) && (a == d);
}
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f)
{
  return (a == b) && (a == c) && (a == d) && (a == f);
}
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f, int32 g)
{
  return (a == b) && (a == c) && (a == d) && (a == f) && (a == g);
}
float32 rand(float32 min, float32 max)
{
  std::uniform_real_distribution<float32> dist(min, max);
  float32 result = dist(generator);
  return result;
}
vec3 rand(vec3 max)
{
  return vec3(rand(0, max.x), rand(0, max.y), rand(0, max.z));
}

// used to fix double escaped or wrong-slash file paths that assimp sometimes
// gives
// todo: UTF8 support for all filenames
std::string fix_filename(std::string str)
{
  std::string result;
  uint32 size = str.size();
  for (uint32 i = 0; i < size; ++i)
  {
    char c = str[i];
    if (c == '\\')
    { // replace a backslash with a forward slash
      uint32 i2 = i + 1;
      if (i2 < size)
      {
        char c2 = str[i2];
        if (c2 == '\\')
        { // skip the second backslash if exists
          ++i;
          continue;
        }
      }
      result.push_back('/');
      continue;
    }
    result.push_back(c);
  }
  return result;
}

std::string copy(aiString str)
{
  std::string result;
  result.resize(str.length);
  result = str.C_Str();
  return result;
}
std::string copy(const aiString *str)
{
  ASSERT(str);
  std::string result;
  result.resize(str->length);
  result = str->C_Str();
  return result;
}

std::string read_file(const char *path)
{
  std::string result;
  std::ifstream f(path, std::ios::in);
  if (!f.is_open())
  {
    std::cerr << "Could not read file " << path << ". File does not exist."
              << std::endl;
    return "";
  }
  std::string line = "";
  while (!f.eof())
  {
    std::getline(f, line);
    result.append(line + "\n");
  }
  f.close();
  return result;
}

Uint32 string_to_U32_color(std::string color)
{
  // color(n,n,n,n)
  std::string a = color.substr(6);
  // n,n,n,n)
  a.pop_back();
  // n,n,n,n

  std::string c;
  ivec4 r(0);

  int i = 0;
  auto it = a.begin();
  while (true)
  {
    if (it == a.end() || *it == ',')
    {
      ASSERT(i < 4);
      r[i] = std::stoi(c);
      if (it == a.end())
      {
        break;
      }
      c.clear();
      ++i;
      ++it;
      continue;
    }
    c.push_back(*it);
    ++it;
  }
  Uint32 result = (r.r << 24) + (r.g << 16) + (r.b << 8) + r.a;
  return result;
}

glm::vec4 string_to_float4_color(std::string color)
{
  // color(n,n,n,n)
  std::string a = color.substr(6);
  // n,n,n,n)
  a.pop_back();
  // n,n,n,n

  std::string c;
  vec4 result(0);

  int i = 0;
  auto it = a.begin();
  while (true)
  {
    if (it == a.end() || *it == ',')
    {
      ASSERT(i < 4);
      result[i] = std::stof(c);
      if (it == a.end())
      {
        break;
      }
      c.clear();
      ++i;
      ++it;
      continue;
    }
    c.push_back(*it);
    ++it;
  }

  return result;
}

Uint64 dankhash(float32 *data, uint32 size)
{
  Uint64 h = 1631243561234777777;
  Uint64 acc = 0;
  for (uint32 i = 0; i < size; ++i)
  {
    float64 c = data[i] * h;
    Uint64 a = (Uint64)c;
    acc += a;
    h = (h ^ a) * a * i; // idk what im doing
  }
  return h + acc;
}

// void _check_gl_error(const char *file, uint32 line)
//{
//  set_message("Checking GL Error in file: ",
//              std::string(file) + " Line: " + std::to_string(line));
//  glFlush();
//  GLenum err = glGetError();
//  glFlush();
//  if (err != GL_NO_ERROR)
//  {
//    std::string error;
//    switch (err)
//    {
//      case GL_INVALID_OPERATION:
//        error = "INVALID_OPERATION";
//        break;
//      case GL_INVALID_ENUM:
//        error = "fileINVALID_ENUM";
//        break;
//      case GL_INVALID_VALUE:
//        error = "INVALID_VALUE";
//        break;
//      case GL_OUT_OF_MEMORY:
//        error = "OUT_OF_MEMORY";
//        break;
//      case GL_INVALID_FRAMEBUFFER_OPERATION:
//        error = "INVALID_FRAMEBUFFER_OPERATION";
//        break;
//    }
//    set_message(
//        "GL ERROR",
//        "GL_" + error + " - " + file + ":" + std::to_string(line) +
//        "\n", 1.0);
//    std::cout << get_messages();
//    push_log_to_disk();
//    throw;
//  }
//  set_message("No GL Error found.", "");
//}
void check_gl_error()
{
  glFlush();
  GLenum err = glGetError();
  if (err != GL_NO_ERROR)
  {
    std::string error;
    switch (err)
    {
      case GL_INVALID_OPERATION:
        error = "INVALID_OPERATION";
        break;
      case GL_INVALID_ENUM:
        error = "INVALID_ENUM";
        break;
      case GL_INVALID_VALUE:
        error = "INVALID_VALUE";
        break;
      case GL_OUT_OF_MEMORY:
        error = "OUT_OF_MEMORY";
        break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        error = "INVALID_FRAMEBUFFER_OPERATION";
        break;
    }
    set_message("GL ERROR", "GL_" + error);
    push_log_to_disk();
    ASSERT(0);
  }
}
void checkSDLError(int32 line)
{
#ifdef DEBUG
  const char *error = SDL_GetError();
  if (*error != '\0')
  {
    std::string err = error;
    set_message("SDL Error:", err);
    if (line != -1)
      set_message("SDL Error line: ", std::to_string(line));
    // ASSERT(0);
  }
#endif
}

float64 get_real_time()
{
  static const Uint64 freq = SDL_GetPerformanceFrequency();
  static const Uint64 begin_time = SDL_GetPerformanceCounter();

  Uint64 current = SDL_GetPerformanceCounter();
  Uint64 elapsed = current - begin_time;
  return (float64)elapsed / (float64)freq;
}
struct Message
{
  std::string identifier;
  std::string message;
  float64 time_of_expiry;
};
static std::vector<Message> messages;
static std::string message_log = "";
std::string get_message_log() { return message_log; }
void __set_message(std::string identifier, std::string message,
    float64 msg_duration, const char *file, uint32 line)
{
  const float64 time = get_real_time();
  bool found = false;
  if (identifier != "")
  {
    for (auto &msg : messages)
    {
      if (msg.identifier == identifier)
      {
        msg.message = message;
        msg.time_of_expiry = time + msg_duration;
        found = true;
        break;
      }
    }
  }
  if (!found)
  {
    Message m = {identifier, message, time + msg_duration};
    messages.push_back(std::move(m));
  }
#if INCLUDE_FILE_LINE_IN_LOG
  message_log.append("Time: " + s(time) + " Event: " + identifier + " " +
                     message + " File: " + file + ": " + std::to_string(line) +
                     "\n\n");
#else
  message_log.append(
      "Time: " + s(time) + " Event: " + identifier + " " + message + "\n");
#endif
}

std::string get_messages()
{
  std::string result;
  float64 time = get_real_time();
  auto it = messages.begin();
  while (it != messages.end())
  {
    if (it->time_of_expiry < time)
    {
      it = messages.erase(it);
      continue;
    }
    result = result + it->identifier + std::string(" ") + it->message +
             std::string("\n");
    ++it;
  }
  return result;
}

void push_log_to_disk()
{
  static bool first = true;
  if (first)
  {
    std::fstream file("warg_log.txt", std::ios::out | std::ios::trunc);
    first = false;
  }
  std::fstream file(
      "warg_log.txt", std::ios::in | std::ios::out | std::ios::app);
  file.seekg(std::ios::end);
  file.write(message_log.c_str(), message_log.size());
  file.close();
  message_log.clear();
}
std::string vtos(glm::vec2 v)
{
  std::string result = "";
  for (uint32 i = 0; i < 2; ++i)
  {
    result += std::to_string(v[i]) + " ";
  }
  return result;
}
std::string vtos(glm::vec3 v)
{
  std::string result = "";
  for (uint32 i = 0; i < 3; ++i)
  {
    result += std::to_string(v[i]) + " ";
  }
  return result;
}
std::string vtos(glm::vec4 v)
{
  std::string result = "";
  for (uint32 i = 0; i < 4; ++i)
  {
    result += std::to_string(v[i]) + " ";
  }
  return result;
}

std::string mtos(glm::mat4 m)
{
  std::string result = "\n|";
  for (uint32 i = 0; i < 4; ++i)
  {
    result += "|" + vtos(m[i]) + "|\n";
  }
  return result;
}

template <> std::string s<Light_Type>(Light_Type value)
{
  if (value == Light_Type::parallel)
    return "Parallel";
  if (value == Light_Type::omnidirectional)
    return "Omnidirectional";
  if (value == Light_Type::spot)
    return "Spotlight";

  return "s(): Unknown Light_Type";
}

template <> std::string s<vec4>(vec4 value)
{
  std::string r = std::to_string(value.r);
  std::string g = std::to_string(value.g);
  std::string b = std::to_string(value.b);
  std::string a = std::to_string(value.a);
  return "color(" + r + "," + g + "," + b + "," + "a" + ")";
}

template <> std::string s<const char *>(const char *value)
{
  return std::string(value);
}
template <> std::string s<std::string>(std::string value) { return value; }

//#define check_gl_error() _check_gl_error(__FILE__, __LINE__)
#define check_gl_error() _check_gl_error()

UID uid()
{
  static UID i = 0;
  ASSERT(i >= 0);
  return i++;
}

const char *texture_format_to_string(GLenum texture_format)
{
  switch (texture_format)
  {
    case GL_SRGB8_ALPHA8:
      return "GL_SRGB8_ALPHA8";
    case GL_SRGB:
      return "GL_SRGB";
    case GL_RGBA:
      return "GL_RGBA";
    case GL_RGB:
      return "GL_RGB";
    case GL_RGBA32F:
      return "GL_RGBA32F";
    case GL_RGBA16F:
      return "GL_RGBA16F";
    case GL_RGB32F:
      return "GL_RGB32F";
    case GL_RGB16F:
      return "GL_RGB16F";
    case GL_RG32F:
      return "GL_RG32F";
    case GL_RG16F:
      return "GL_RG16F";
    case GL_R32F:
      return "GL_R32F";
    case GL_R16F:
      return "GL_R16F";
    default:
      return "UNKNOWN";
  }
}


bool is_float_format(GLenum texture_format)
{
  switch (texture_format)
  {
  case GL_RGBA32F:
    return true;
  case GL_RGBA16F:
    return true;
  case GL_RGB32F:
    return true;
  case GL_RGB16F:
    return true;
  case GL_RG32F:
    return true;
  case GL_RG16F:
    return true;
  case GL_R32F:
    return true;
  case GL_R16F:
    return true;
  default:
    return false;
  }
}

void Config::load(std::string filename)
{
  json j;
  try
  {
    std::string str = read_file(filename.c_str());
    j = json::parse(str);
  }
  catch (...)
  {
    return;
  }
  auto i = j.end();

  if ((i = j.find("Resolution")) != j.end())
    resolution = *i;

  if ((i = j.find("Render Scale")) != j.end())
    render_scale = *i;

  if ((i = j.find("Fov")) != j.end())
    fov = *i;

  if ((i = j.find("Shadow Map Scale")) != j.end())
    shadow_map_scale = *i;
}

void Config::save(std::string filename)
{
  json j;
  j["Resolution"] = resolution;
  j["Render Scale"] = render_scale;
  j["Fov"] = fov;
  j["Shadow Map Scale"] = shadow_map_scale;

  std::string str = pretty_dump(j);
  std::fstream file(filename, std::ios::out);
  file.write(str.c_str(), str.size());
}

bool has_img_file_extension(std::string name)
{
  uint32 size = name.size();

  if (size < 3)
    return false;

  std::string end = name.substr(size - 4, 4);
  if (end == ".jpg" || end == ".png" || end == ".hdr" || end == ".tga")
  {
    return true;
  }
  return false;
}

std::string strip_file_extension(std::string file)
{
  ASSERT(file.length() > 4);
  return file.substr(0, file.size() - 3);
}
