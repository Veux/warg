#include "stdafx.h"
#include "General_Purpose.h"
#include "Json.h"
#include "Third_party/stb/stb_image.h"
#include "Render.h"
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

  if (a.length() == 0)
  {
    return vec4(0);
  }
  if (a.back() != ')')
  {
    return vec4(0);
  }

  // n,n,n,n)
  a.pop_back();
  // n,n,n,n

  std::string buffer;
  vec4 result(0);

  int i = 0;
  auto it = a.begin();
  while (true)
  {
    if (it == a.end() || *it == ',')
    {
      if (i == 4)
      { // more than three ','
        return vec4(0);
      }
      result[i] = std::stof(buffer);
      if (it == a.end())
      {
        break;
      }
      buffer.clear();
      ++i;
      ++it;
      continue;
    }

    const char c = *it;
    if (!isdigit(c) && c != '.' && c != '-' && c != 'f')
    {
      return vec4(0);
    }
    buffer.push_back(c);
    ++it;
  }

  return result;
}

Uint64 dankhash(const float32 *data, uint32 size)
{
  uint32 h = 0;
  std::hash<float32> hasher;
  static Timer hashtimer(1000);
  hashtimer.start();
  for (uint32 i = 0; i < size; ++i)
  {
    float32 f = data[i];
    h ^= hasher(f) + 0x9e3779b9 + (h<<6) + (h>>2);
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
struct Message
{
  std::string identifier;
  std::string message;
  float64 time_of_expiry;
  std::string thread_id;
};
static std::vector<Message> messages;
static std::string message_log = "";
std::string get_message_log()
{
  return message_log;
}
std::mutex SET_MESSAGE_MUTEX;
void __set_message(std::string identifier, std::string message, float64 msg_duration, const char *file, uint32 line)
{
  std::lock_guard<std::mutex> l(SET_MESSAGE_MUTEX);
  std::thread::id thread_id = std::this_thread::get_id();
  std::stringstream ss;
  ss << std::this_thread::get_id();
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
    Message m = {identifier, message, time + msg_duration, ss.str()};
    messages.push_back(std::move(m));
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
    result = result + s(it->thread_id, ": ", it->identifier, " ", it->message, "\n");
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
  std::fstream file("warg_log.txt", std::ios::in | std::ios::out | std::ios::app);
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
    if (v[i] >= 0.0f)
    {
      result += " ";
    }
    result += std::to_string(v[i]) + " ";
  }
  return result;
}
std::string vtos(glm::vec3 v)
{
  std::string result = "";
  for (uint32 i = 0; i < 3; ++i)
  {
    if (v[i] >= 0.0f)
    {
      result += " ";
    }
    result += std::to_string(v[i]) + " ";
  }
  return result;
}
std::string vtos(glm::vec4 v)
{
  std::string result = "";
  for (uint32 i = 0; i < 4; ++i)
  {
    if (v[i] >= 0.0f)
    {
      result += " ";
    }
    std::string sf = std::to_string(v[i]);
    while (sf.length() > 6)
      sf.pop_back();
    while (sf.length() <= 6)
      sf.push_back('0');
    result += sf + " ";
  }
  return result;
}

std::string qtos(glm::quat v)
{
  std::string result = "";
  for (uint32 i = 0; i < 4; ++i)
  {
    if (v[i] >= 0.0f)
    {
      result += " ";
    }
    std::string sf = std::to_string(v[i]);
    while (sf.length() > 6)
      sf.pop_back();
    while (sf.length() <= 6)
      sf.push_back('0');
    result += sf + " ";
  }
  return result;
}

std::string to_string(glm::mat4 &m)
{
  std::string result = "";
  for (uint32 column = 0; column < 4; ++column)
  {
    result += "|";
    for (uint32 row = 0; row < 4; ++row)
    {
      float32 f = m[row][column];
      std::string sf = s(f);
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

std::string to_string(vec4& value)
{
  std::string r = std::to_string(value.r);
  std::string g = std::to_string(value.g);
  std::string b = std::to_string(value.b);
  std::string a = std::to_string(value.a);
  return "color(" + r + "," + g + "," + b + "," + a + ")";
}
template <> std::string s<const char *>(const char *value)
{
  return std::string(value);
}
template <> std::string s<std::string>(std::string value)
{
  return value;
}

std::string to_string(Light_Type& value)
{
  if (value == Light_Type::parallel)
    return "Parallel";
  if (value == Light_Type::omnidirectional)
    return "Omnidirectional";
  if (value == Light_Type::spot)
    return "Spotlight";

  return "s(): Unknown Light_Type";
}

//#define check_gl_error() _check_gl_error(__FILE__, __LINE__)
#define check_gl_error() _check_gl_error()

UID uid()
{
  static UID i = 1;
  ASSERT(i);
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

std::string to_string(Array_String &s)
{
  std::string result;
  for (uint32 i = 0; i < MAX_ARRAY_STRING_LENGTH; ++i)
  {
    char *ch = &s.str[i];
    if (*ch == '\0')
      return result;
    result.push_back(*ch);
  }
  return result;
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

  if ((i = j.find("Low Quality Specular Convolution")) != j.end())
    use_low_quality_specular = *i;

  if ((i = j.find("Render Simple")) != j.end())
    render_simple = *i;
}

void Config::save(std::string filename)
{
  json j;
  j["Resolution"] = resolution;
  j["Render Scale"] = render_scale;
  j["Fov"] = fov;
  j["Shadow Map Scale"] = shadow_map_scale;
  j["Low Quality Specular Convolution"] = use_low_quality_specular;
  j["Render Simple"] = render_simple;

  std::string str = pretty_dump(j);
  std::fstream file(filename, std::ios::out);
  file.write(str.c_str(), str.size());
}

Image_Data load_image(std::string path)
{
  Image_Data data;
  const bool is_hdr = stbi_is_hdr(path.c_str());
  if (is_hdr)
  {
    data.data = stbi_loadf(path.c_str(), &data.x, &data.y, &data.comp, 4);
    data.format = GL_FLOAT;
  }
  else
  {
    data.data = stbi_load(path.c_str(), &data.x, &data.y, &data.comp, 4);
    data.format = GL_UNSIGNED_BYTE;
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

    std::string task;
    {
      std::lock_guard<std::mutex> lock0(loader->queue_mtx);
      if (loader->queue_size == 0)
        continue;
      task = loader->load_queue.front();
      loader->load_queue.pop();
      loader->queue_size = loader->load_queue.size();
    }

    Image_Data data = load_image(task);
    std::lock_guard<std::mutex> lock1(loader->db_mtx);
    loader->database[task] = data;
  }
}

Image_Loader::Image_Loader()
{
  threads[0] = std::thread(loader_loop, this);
  threads[1] = std::thread(loader_loop, this);
  threads[2] = std::thread(loader_loop, this);
  // threads[3] = std::thread(loader_loop, this);
}
Image_Loader::~Image_Loader()
{
  running = false;
  threads[0].join();
  threads[1].join();
  threads[2].join();
  // threads[3].join();
}

bool Image_Loader::load(std::string path, Image_Data *data)
{
  std::lock_guard<std::mutex> lock0(db_mtx);
  if (database.count(path))
  {
    if (database[path].initialized)
    {
      *data = database[path];
      database.erase(path);
      return true;
    }
    return false;
  }
  database[path];
  std::lock_guard<std::mutex> lock1(queue_mtx);
  load_queue.push(path);
  queue_size = load_queue.size();
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
