#include "stdafx.h"
#include "General_Purpose.h"
#include "Json.h"
#include "Third_party/stb/stb_image.h"
#include "Render.h"
#include <filesystem>
using namespace std;
float32 wrap_to_range(const float32 input, const float32 min, const float32 max)
{
  const float32 range = max - min;
  const float32 offset = input - min;
  return (offset - (floor(offset / range) * range)) + min;
}

bool all_equal(int32 a, int32 b, int32 c)
{
  return (a == b) && (a == c);
}
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
std::string fix_filename(std::string str)
{
  std::string result;
  uint32 size = uint32(str.size());
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

  if (str.length > 10000)
  {
    set_message("Unusually long or invalid aiString", "", 10.0f);
    return string();
  }
  result.resize(str.length);
  result = str.C_Str();
  return result;
}
std::string copy(const aiString *str)
{
  ASSERT(str);

  std::string result;
  if (str->length > 10000)
  {
    set_message("Unusually long or invalid aiString", "", 10.0f);
    return result;
  }

  result.resize(str->length);
  result = str->C_Str();
  return result;
}

vec3 copy(const aiVector3D v)
{
    vec3 result;
    result.x = v.x;
    result.y = v.y;
    result.z = v.z;
    return result;
}

quat copy(const aiQuaternion q)
{
    quat quat;
    quat.x = q.x;
    quat.y = q.y;
    quat.z = q.z;
    quat.w = q.w;
    return quat;
}

glm::mat4 copy(aiMatrix4x4 m)
{
  // assimp is row-major
  // glm is column-major
  glm::mat4 result;
  for (uint32 i = 0; i < 4; ++i)
  {
    for (uint32 j = 0; j < 4; ++j)
    {
      result[i][j] = m[j][i];
    }
  }
  return result;
}


std::string read_file(const char *path)
{
  std::string result;
  std::ifstream f(path, std::ios::in);
  if (!f.is_open())
  {
    std::cerr << "Could not read file " << path << ". File does not exist." << std::endl;
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

// Uint32 string_to_U32_color(std::string color)
//{
//  // color(n,n,n,n)
//  std::string a = color.substr(6);
//  // n,n,n,n)
//  a.pop_back();
//  // n,n,n,n
//
//  std::string c;
//  ivec4 r(0);
//
//  int i = 0;
//  auto it = a.begin();
//  while (true)
//  {
//    if (it == a.end() || *it == ',')
//    {
//      ASSERT(i < 4);
//      r[i] = std::stoi(c);
//      if (it == a.end())
//      {
//        break;
//      }
//      c.clear();
//      ++i;
//      ++it;
//      continue;
//    }
//    c.push_back(*it);
//    ++it;
//  }
//  Uint32 result = (r.r << 24) + (r.g << 16) + (r.b << 8) + r.a;
//  return result;
//}

// glm::vec4 string_to_float4_color(std::string color)
//{
//  // color(n,n,n,n)
//  std::string a = color.substr(6);
//
//  if (a.length() == 0)
//  {
//    return vec4(0);
//  }
//  if (a.back() != ')')
//  {
//    return vec4(0);
//  }
//
//  // n,n,n,n)
//  a.pop_back();
//  // n,n,n,n
//
//  std::string buffer;
//  vec4 result(0);
//
//  int i = 0;
//  auto it = a.begin();
//  while (true)
//  {
//    if (it == a.end() || *it == ',')
//    {
//      if (i == 4)
//      { // more than three ','
//        return vec4(0);
//      }
//      result[i] = std::stof(buffer);
//      if (it == a.end())
//      {
//        break;
//      }
//      buffer.clear();
//      ++i;
//      ++it;
//      continue;
//    }
//
//    const char c = *it;
//    if (!isdigit(c) && c != '.' && c != '-' && c != 'f')
//    {
//      return vec4(0);
//    }
//    buffer.push_back(c);
//    ++it;
//  }
//
//  return result;
//}

Uint64 dankhash(const float32 *data, uint32 size)
{
  uint32 h = 0;
  std::hash<float32> hasher;
  static Timer hashtimer(1000);
  hashtimer.start();
  for (uint32 i = 0; i < size; ++i)
  {
    float32 f = data[i];
    h ^= hasher(f) + 0x9e3779b9 + (h << 6) + (h >> 2);
  }
  hashtimer.stop();
  set_message("hashtimer:", hashtimer.string_report(), 30.0f);
  return h;
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
struct Warg_Debug_Message
{
  string identifier;
  string message;
  float64 time_of_expiry;
  string thread_id;
};
static vector<Warg_Debug_Message> warg_messages;
static string message_log = "";
std::string get_message_log()
{
  return message_log;
}
std::mutex SET_MESSAGE_MUTEX;
void nop(bool i)
{
  return;
}
void __set_message(std::string identifier, std::string message, float64 msg_duration, const char *file, uint32 line)
{
  lock_guard<mutex> l(SET_MESSAGE_MUTEX);
  thread::id thread_id = this_thread::get_id();
  stringstream ss;
  ss << this_thread::get_id();
  const float64 time = get_real_time();
  if (message_log.length() > 11000)
  {
    message_log = message_log.substr(10000);
  }


  bool found = false;
  if (identifier != "")
  {
    for (auto &msg : warg_messages)
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
  //if (!found)
  {
    Warg_Debug_Message m = {identifier, message, time + msg_duration, ss.str()};
    warg_messages.push_back(std::move(m));
  }
#if INCLUDE_FILE_LINE_IN_LOG
  message_log.append(s("Thread: ", ss.str(), " Time: ", time, " Event: ", identifier, " ", message, " File: ", file,
      ": ", line, "\n\n"));
#else
  message_log.append(s("Thread: ", ss.str(), "Time: ", time, " Event: ", identifier, " ", message, "\n"));
#endif
}

std::string get_messages()
{
  string result;
  float64 time = get_real_time();
  auto it = warg_messages.begin();
  while (it != warg_messages.end())
  {
    if (it->time_of_expiry < time)
    {
      it = warg_messages.erase(it);
      continue;
    }
    result = result + s(it->thread_id, ": ", it->identifier, " ", it->message, "\n");
    ++it;
  }
  return result;
}

void push_log_to_disk()
{
  return;
  static bool first = true;
  if (first)
  {
    fstream file("warg_log.txt", ios::out | ios::trunc);
    first = false;
  }
  fstream file("warg_log.txt", ios::in | ios::out | ios::app);
  file.seekg(ios::end);
  file.write(message_log.c_str(), message_log.size());
  file.close();
  message_log.clear();
}
std::string to_string(const glm::vec2 v)
{
  return s("(", v.x, ",", v.y, ")");
}
std::string to_string(const glm::vec3 v)
{
  return s("(", v.x, ",", v.y, ",", v.z, ")");
}
std::string to_string(const glm::vec4 v)
{
  return s("(", v.x, ",", v.y, ",", v.z, ",", v.w, ")");
}

std::string qtos(const glm::quat v)
{
  string result = "";
  for (uint32 i = 0; i < 4; ++i)
  {
    if (v[i] >= 0.0f)
    {
      result += " ";
    }
    string sf = to_string(v[i]);
    while (sf.length() > 6)
      sf.pop_back();
    while (sf.length() <= 6)
      sf.push_back('0');
    result += sf + " ";
  }
  return result;
}

std::string to_string(const Particle_Physics_Type &t)
{
  if (t == Particle_Physics_Type::simple)
  {
    return "Simple";
  }
  if (t == Particle_Physics_Type::wind)
  {
    return "Wind";
  }
  return "Unknown";
}
std::string to_string(const glm::quat &q)
{
  return s("x = ", q.x, ", y = ", q.y, ", z = ", q.z, ", w = ", q.w);
}

std::string to_string(const glm::vec3 &v)
{
  return s("x = ", v.x, ", y = ", v.y, ", z = ", v.z);
}

std::string to_string(const glm::mat4 &m)
{
  string result = "";
  for (uint32 column = 0; column < 4; ++column)
  {
    result += "|";
    for (uint32 row = 0; row < 4; ++row)
    {
      float32 f = m[row][column];
      string sf = s(f);
      while (sf.length() > 6)
      {
        if (sf.back() != '.')
          sf.pop_back();
        else
          break;
      }
      while (sf.length() <= 6)
        sf.push_back('0');
      if (f >= 0)
      {
        result += " ";
        if (sf.back() != '.')
        {
          sf.pop_back();
        }
      }
      result += sf + " ";
    }
    result += "|\n";
  }
  return result;
}

std::string to_string(const vec4 &value)
{
  string r = to_string(value.r);
  string g = to_string(value.g);
  string b = to_string(value.b);
  string a = to_string(value.a);
  return "(" + r + "," + g + "," + b + "," + a + ")";
}


template <> string s<const char *>(const char *value)
{
  return string(value);
}
template <> string s<string>(string value)
{
  return value;
}

string to_string(const Light_Type &value)
{
  if (value == Light_Type::parallel)
    return "Parallel";
  if (value == Light_Type::omnidirectional)
    return "Omnidirectional";
  if (value == Light_Type::spot)
    return "Spotlight";

  return "s(): Unknown Light_Type";
}
std::string to_string(const Material_Blend_Mode& mode)
{
  if (mode == Material_Blend_Mode::blend)
  {
    return "blend";
  }
  if (mode == Material_Blend_Mode::add)
  {
    return "add";
  }
  return "Unknown";
}

std::string to_string(const Particle_Emission_Type &t)
{
  if (t == Particle_Emission_Type::stream)
  {
    return "Stream";
  }
  if (t == Particle_Emission_Type::explosion)
  {
    return "Explosion";
  }
  return "Unknown";
}

//#define check_gl_error() _check_gl_error(__FILE__, __LINE__)
#define check_gl_error() _check_gl_error()

UID uid()
{
  static UID i = 1;
  ASSERT(i);
  return i++;
}

const char *filter_format_to_string(GLenum filter_format)
{
  switch (filter_format)
  {
    case GL_NEAREST:
      return "GL_NEAREST";
    case GL_NEAREST_MIPMAP_LINEAR:
      return "GL_NEAREST_MIPMAP_LINEAR";
    case GL_NEAREST_MIPMAP_NEAREST:
      return "GL_NEAREST_MIPMAP_NEAREST";
    case GL_LINEAR_MIPMAP_NEAREST:
      return "GL_LINEAR_MIPMAP_NEAREST";
    case GL_LINEAR_MIPMAP_LINEAR:
      return "GL_LINEAR_MIPMAP_LINEAR";
    case GL_LINEAR:
      return "GL_LINEAR";
    default:
      return "UNKNOWN";
  }
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
    case GL_RGBA8:
      return "GL_RGBA8";
    case GL_RGB8:
      return "GL_RGB8";
    case GL_RG8:
      return "GL_RG8";
    case GL_R8:
      return "GL_R8";
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
    case GL_DEPTH_COMPONENT16:
      return "GL_DEPTH_COMPONENT16";
    case GL_DEPTH_COMPONENT24:
      return "GL_DEPTH_COMPONENT24";
    case GL_DEPTH_COMPONENT32F:
      return "GL_DEPTH_COMPONENT32F";
    case GL_DEPTH32F_STENCIL8:
      return "GL_DEPTH32F_STENCIL8";
    case GL_DEPTH24_STENCIL8:
      return "GL_DEPTH24_STENCIL8";
    case GL_STENCIL_INDEX8:
      return "GL_STENCIL_INDEX8";
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
uint32 components_of_float_format(GLenum texture_format)
{
  switch (texture_format)
  {
    case GL_RGBA32F:
      return 4;
    case GL_RGBA16F:
      return 4;
    case GL_RGB32F:
      return 3;
    case GL_RGB16F:
      return 3;
    case GL_RG32F:
      return 2;
    case GL_RG16F:
      return 2;
    case GL_R32F:
      return 1;
    case GL_R16F:
      return 1;
    default:
      return 0;
  }
}

uint32 type_of_float_format(GLenum texture_format)
{
  switch (texture_format)
  {
    case GL_RGBA32F:
      return 32;
    case GL_RGBA16F:
      return 16;
    case GL_RGB32F:
      return 32;
    case GL_RGB16F:
      return 16;
    case GL_RG32F:
      return 32;
    case GL_RG16F:
      return 16;
    case GL_R32F:
      return 32;
    case GL_R16F:
      return 16;
    default:
      return 0;
  }
}

std::string find_free_filename(std::string filename, std::string extension)
{
  bool exists = std::filesystem::exists(s(filename, extension));
  uint32 i = 0;
  while (exists)
  {
    exists = std::filesystem::exists(s(filename, i, extension));
    ++i;
  }
  return s(filename, i);
}

string to_string(Array_String &s)
{
  string result;
  for (uint32 i = 0; i < MAX_ARRAY_STRING_LENGTH; ++i)
  {
    char *ch = &s.str[i];
    if (*ch == '\0')
      return result;
    result.push_back(*ch);
  }
  return result;
}

string to_string(string_view& s)
{
  return string(s);
}

void Config::load(string filename)
{
  json j;
  try
  {
    string str = read_file(filename.c_str());
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

  if ((i = j.find("Low Quality Specular Convolution")) != j.end())
    use_low_quality_specular = *i;

  if ((i = j.find("Render Simple")) != j.end())
    render_simple = *i;

  if ((i = j.find("WargSpy Address")) != j.end())
    wargspy_address = i->get<std::string>();

  if ((i = j.find("Character Name")) != j.end())
    character_name = i->get<std::string>();
  if ((i = j.find("Fullscreen")) != j.end())
    fullscreen = *i;
}

void Config::save(string filename)
{
  json j;
  j["Resolution"] = resolution;
  j["Render Scale"] = render_scale;
  j["Fov"] = fov;
  j["Shadow Map Scale"] = shadow_map_scale;
  j["Low Quality Specular Convolution"] = use_low_quality_specular;
  j["Render Simple"] = render_simple;
  j["WargSpy Address"] = wargspy_address;
  j["Character Name"] = character_name;
  j["Fullscreen"] = fullscreen;

  string str = pretty_dump(j);
  fstream file(filename, ios::out);
  file.write(str.c_str(), str.size());
}

Image_Data load_image(string path, GLenum desiredinternalformat)
{
  Image_Data data;

  uint32 p = path.find("Assets/");
  if (p == -1)
  {
    path = BASE_TEXTURE_PATH + path;
  }

  bool is_hdr = stbi_is_hdr(path.c_str());
  is_hdr = is_float_format(desiredinternalformat);
  if (is_hdr)
  {
    data.data = stbi_loadf(path.c_str(), &data.x, &data.y, &data.comp, 4);
    data.data_type = GL_FLOAT;
    data.data_size = data.x * data.y * sizeof(float32) * 4;
  }
  else
  {
    data.data = stbi_load(path.c_str(), &data.x, &data.y, &data.comp, 4);
    data.data_type = GL_UNSIGNED_BYTE;
    data.data_size = data.x * data.y * sizeof(uint8) * 4;
  }
  data.initialized = true;
  return data;
}

void Image_Loader::loader_loop(Image_Loader *loader)
{
  while (loader->running)
  {
    Sleep(1);
    if (loader->queue_size == 0)
      continue;

    string task;
    {
      lock_guard<mutex> loc(loader->queue_mtx);
      if (loader->queue_size == 0)
        continue;
      task = loader->load_queue.front();
      loader->load_queue.pop();
      loader->queue_size = loader->load_queue.size();
    }
    uint32 back = task.find_last_of(",");
    string filename = task.substr(0, back);
    string fmt = task.substr(back + 1);
    GLenum gl_texture_internalformat = stoi(fmt);
    Image_Data data = load_image(filename, gl_texture_internalformat);

    if (data.data_type != GL_FLOAT)
    {
      if (gl_texture_internalformat == GL_SRGB8_ALPHA8 && data.data)
      { // premultiply alpha
        ASSERT(data.data_type == GL_UNSIGNED_BYTE);
        for (int32 i = 0; i < data.x * data.y; ++i)
        {
          break;
          uint32 pixel = ((uint32 *)data.data)[i];
          uint8 r = (uint8)(0x000000FF & pixel);
          uint8 g = (uint8)((0x0000FF00 & pixel) >> 8);
          uint8 b = (uint8)((0x00FF0000 & pixel) >> 16);
          uint8 a = (uint8)((0xFF000000 & pixel) >> 24);
          float32 af = float32(a) / 255.f;
          af = pow(af, 1 / 2.2);
          vec3 c(r, g, b);
          c = vec3(af) * c;
          r = (uint8)glm::clamp((uint32)round(c.r), 0u, 255u);
          g = (uint8)glm::clamp((uint32)round(c.g), 0u, 255u);
          b = (uint8)glm::clamp((uint32)round(c.b), 0u, 255u);
          //((uint32 *)data.data)[i] = (24 << a) | (16 << b) | (8 << g) | r;
        }
      }
      if (gl_texture_internalformat == GL_RGBA32F && data.data)
      {
        int a = 3;
      }
    }
    std::lock_guard<std::mutex> lock(loader->db_mtx);
    loader->database[task] = data;
  }
}

Image_Loader::Image_Loader()
{
  for (int i = 0; i < thread_count; i++)
    threads[i] = std::thread(loader_loop, this);
}
Image_Loader::~Image_Loader()
{
  running = false;
  for (int i = 0; i < thread_count; i++)
    threads[i].join();
}

bool Image_Loader::load(std::string path, Image_Data *data, GLenum internalformat)
{
  string key = s(path, ",", internalformat);
  std::lock_guard<std::mutex> lock0(db_mtx);
  if (database.count(key))
  {
    if (database[key].initialized)
    {
      *data = database[key];
      database.erase(key);

      return true;
    }
    return false;
  }
  database[key];
  std::lock_guard<std::mutex> lock1(queue_mtx);
  load_queue.push(key);
  queue_size = load_queue.size();
  return false;
}

bool has_hdr_file_extension(std::string name)
{
  size_t size = name.size();

  if (size <= 3)
    return false;

  std::string end = name.substr(size - 4, 4);
  if (end == ".hdr" || end == ".HDR")
  {
    return true;
  }
  return false;
}
bool has_img_file_extension(std::string name)
{
  size_t size = name.size();

  if (size <= 3)
    return false;

  std::string end = name.substr(size - 4, 4);
  if (end == ".jpg" || end == ".png" || end == ".hdr" || end == ".JPG" || end == ".PNG" || end == ".JPG" ||
      end == ".HDR" || end == ".TGA" || end == ".tga")
  {
    return true;
  }
  return false;
}
Image_Loader IMAGE_LOADER;

vec4 rgb_vec4(uint8 r, uint8 g, uint8 b)
{
  return vec4(r, g, b, 255.f) / vec4(255.f);
}

float64 random_between(float64 min, float64 max)
{
  std::uniform_real_distribution<float64> distribution(min, max);
  return distribution(generator);
}
int32 random_between(int32 min, int32 max)
{
  std::uniform_int_distribution<int32> distribution(min, max);
  return distribution(generator);
}
float32 random_between(float32 min, float32 max)
{
  std::uniform_real_distribution<float32> distribution(min, max);
  return distribution(generator);
}

glm::vec3 random_between(glm::vec3 min, glm::vec3 max)
{
  vec3 r;
  r.x = random_between(min.x, max.x);
  r.y = random_between(min.y, max.y);
  r.z = random_between(min.z, max.z);
  return r;
}

glm::vec2 random_within(const vec2 &vec)
{
  std::uniform_real_distribution<float32> x(0, vec.x);
  std::uniform_real_distribution<float32> y(0, vec.y);
  return vec2(x(generator), y(generator));
}
glm::vec3 random_within(const vec3 &vec)
{
  std::uniform_real_distribution<float32> x(0, vec.x);
  std::uniform_real_distribution<float32> y(0, vec.y);
  std::uniform_real_distribution<float32> z(0, vec.z);
  return vec3(x(generator), y(generator), z(generator));
}
glm::vec4 random_within(const vec4 &vec)
{
  std::uniform_real_distribution<float32> x(0, vec.x);
  std::uniform_real_distribution<float32> y(0, vec.y);
  std::uniform_real_distribution<float32> z(0, vec.x);
  std::uniform_real_distribution<float32> w(0, vec.y);
  return vec4(x(generator), y(generator), z(generator), w(generator));
}
vec2 random_2D_unit_vector()
{
  float32 azimuth = random_between(0.f, glm::two_pi<float32>());
  return vec2(cos(azimuth), sin(azimuth));
}
vec3 random_3D_unit_vector()
{
  float z = random_between(-1.f, 1.f);
  vec2 plane = random_2D_unit_vector();
  float plane_scale = sqrt(1.f - (z * z));
  return vec3(plane_scale * plane, z);
}
vec2 random_2D_unit_vector(float32 azimuth_min, float32 azimuth_max)
{
  float32 azimuth = random_between(azimuth_min, azimuth_max);
  return vec2(cos(azimuth), sin(azimuth));
}
vec3 random_3D_unit_vector(float32 azimuth_min, float32 azimuth_max, float32 altitude_min, float32 altitude_max)
{
  float z = random_between(altitude_min, altitude_max);
  vec2 plane = random_2D_unit_vector(azimuth_min, azimuth_max);
  float plane_scale = sqrt(1.f - (z * z));
  return vec3(plane_scale * plane, z);
}
