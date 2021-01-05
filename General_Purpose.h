#pragma once
#include "Timer.h"
#include "Array_String.h"
#include <unordered_map>
#include <queue>
#include <mutex>
#include "glad/glad.h"
#include "Forward_Declarations.h"
enum struct Light_Type;
#define ASSERT(x) _errr(x, __FILE__, __LINE__)

void nop(bool i);

#ifdef NDEBUG
#define DEBUGASSERT(x) nop(x)
#else
#define DEBUGASSERT(x) ASSERT(x)
#endif

void __set_message(std::string identifier, std::string message, float64 msg_duration, const char *, uint32);
#define CREATE_3(x, y, z) __set_message(x, y, z, __FILE__, __LINE__)
#define CREATE_2(x, y) CREATE_3(x, y, 0.0)
#define CREATE_1(x) CREATE_2(x, "")
#define CREATE_0() CREATE_1("")

#define FUNC_CHOOSER(_f1, _f2, _f3, _f4, ...) _f4
#define FUNC_RECOMPOSER(argsWithParentheses) FUNC_CHOOSER argsWithParentheses
#define CHOOSE_FROM_ARG_COUNT(...) FUNC_RECOMPOSER((__VA_ARGS__, CREATE_3, CREATE_2, CREATE_1, ))
#define NO_ARG_EXPANDER() , , CREATE_0
#define MACRO_CHOOSER(...) CHOOSE_FROM_ARG_COUNT(NO_ARG_EXPANDER __VA_ARGS__())

// adds a message to a container that is retrieved by get_messages()
// subsequent calls to this function with identical identifiers will
// overwrite the older message when retrieved with get_message_log()
// but ALL messages will be logged to disk with push_log_to_disk()
// a duration of 0 will not show up in the console, but will be logged
// if identifier == "", then all of those messages will pass through to
// the get_message_log(), and console report
#define set_message(...) MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

// returns messages set with set_message()
// as string(identifier+" "+message)
// for all messages with unique identifier
// and all instances of identifier == ""
std::string get_messages();

// appends all messages to disk that have been set
// since the last call to this function
void push_log_to_disk();

std::string get_message_log();
template <typename T> void _errr(T t, const char *file, uint32 line)
{
  using namespace std;
#ifndef DISABLE_ASSERT
  if (!t)
  {
    set_message("", "Assertion failed in:" + string(file) + "\non line:" + to_string(line), 1.0);
    string message_log = get_message_log();
    push_log_to_disk();
    string end_of_log;
   // size_t length = message_log.size();
    size_t char_count = 10000;
   // if (length < char_count)
     // char_count = length;
   // else
    //{
     // end_of_log = message_log.substr(length - char_count, string::npos);
   // }
    //end_of_log = end_of_log + "sdlfjghnserokghnbwesroiujkghbnw4eour5ghbnw3e4ourjlgh";
    fstream file("warg_crash_log.txt", ios::trunc | ios::out );
    file.write(message_log.c_str(), message_log.size());
    file.flush();
    file.close();
    cout << end_of_log << endl;
    throw;
  }
#endif
}

template <typename C, typename P, typename T> auto find_by(C &c, P p, T &t)
{
  return std::find_if(c.begin(), c.end(), [&](auto &v) { return std::invoke(p, v); });
}
template <typename C, typename F> auto erase_if(C &c, F f)
{
  return c.erase(std::remove_if(c.begin(), c.end(), f), c.end());
}
template <typename C, typename F> auto any_of(C &c, F f)
{
  return std::any_of(c.begin(), c.end(), f);
}
template <typename C, typename F> auto find_if(C &c, F f)
{
  return std::find_if(c.begin(), c.end(), f);
}
template <typename C, typename F> auto none_of(C &c, F f)
{
  return std::none_of(c.begin(), c.end(), f);
}
template <typename C0, typename C1, typename CMP, typename F> void join_for(C0 &c0, C1 &c1, CMP cmp, F f)
{
  auto c0_it = c0.begin();
  auto c1_it = c1.begin();
  while (c0_it != c0.end() & c1_it != c1.end())
  {
    int c = cmp(*c0_it, *c1_it);
    if (c < 0)
      c0_it++;
    else if (c > 0)
      c1_it++;
    else
      f(*(c0_it++), *(c1_it++));
  }
}
template <typename C0, typename C1, typename CMP, typename F> bool join_any_of(C0 &c0, C1 &c1, CMP cmp, F f)
{
  auto c0_it = c0.begin();
  auto c1_it = c1.begin();
  while (c0_it != c0.end() & c1_it != c1.end())
  {
    int c = cmp(*c0_it, *c1_it);
    if (c < 0)
      c0_it++;
    else if (c > 0)
      c1_it++;
    else if (f(*(c0_it++), *(c1_it++)))
      return true;
  }
  return false;
}
template <typename C0, typename C1, typename CMP, typename F> bool join_none_of(C0 &c0, C1 &c1, CMP cmp, F f)
{
  return !join_any_of(c0, c1, cmp, f);
}
template <typename C0, typename C1, typename F> void join_for(C0 &c0, C1 &c1, F f)
{
  auto c0_it = c0.begin();
  auto c1_it = c1.begin();
  while (c0_it != c0.end() & c1_it != c1.end())
  {
    int c = cmp(*c0_it, *c1_it);
    if (c < 0)
      c0_it++;
    else if (c > 0)
      c1_it++;
    else
      f(*(c0_it++), *(c1_it++));
  }
}
template <typename C0, typename C1, typename F> bool join_any_of(C0 &c0, C1 &c1, F f)
{
  auto c0_it = c0.begin();
  auto c1_it = c1.begin();
  while (c0_it != c0.end() & c1_it != c1.end())
  {
    int c = cmp(*c0_it, *c1_it);
    if (c < 0)
      c0_it++;
    else if (c > 0)
      c1_it++;
    else if (f(*(c0_it++), *(c1_it++)))
      return true;
  }
  return false;
}
template <typename C0, typename C1, typename F> bool join_none_of(C0 &c0, C1 &c1, F f)
{
  return !join_any_of(c0, c1, f);
}

struct Config
{ // if you add more settings, be sure to put them in load() and save()
  ivec2 resolution = ivec2(1280, 720);
  float32 render_scale = 1.0f;
  float32 fov = 60;
  float32 shadow_map_scale = 1.0f;
  bool use_low_quality_specular = false;
  bool render_simple = false;
  bool fullscreen = false;
  std::string wargspy_address;
  std::string character_name = "Cubeboi";

  void load(std::string filename);
  void save(std::string filename);
};

struct Image_Data
{
  void *data = nullptr;
  int32 x = 0;
  int32 y = 0;
  int32 comp;
  uint32 data_size;
  GLenum data_type;
  bool initialized = false;
};

struct Image_Loader
{
  static const int thread_count = 17;

public:
  Image_Loader();
  ~Image_Loader();
  bool load(std::string filename, Image_Data *data, GLenum format);
  static void loader_loop(Image_Loader *loader);

  bool running = true;

private:
  std::unordered_map<std::string, Image_Data> database;
  std::queue<std::string> load_queue;
  std::mutex db_mtx;
  std::mutex queue_mtx;
  std::atomic<size_t> queue_size;
  std::thread threads[thread_count];
};

bool all_equal(int32 a, int32 b, int32 c);
bool all_equal(int32 a, int32 b, int32 c, int32 d);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f, int32 g);
float32 wrap_to_range(const float32 input, const float32 min, const float32 max);

float32 rand(float32 min, float32 max);
vec3 rand(vec3 max);
// used to fix double escaped or wrong-slash file paths
// probably need something better
std::string fix_filename(std::string str);
std::string copy(const aiString *str);
vec3 copy(const aiVector3D v);
quat copy(const aiQuaternion q);
std::string read_file(const char *path);
std::string to_string(const glm::vec2 v);
//std::string to_string(glm::vec4 v);
std::string qtos(const glm::quat v);
std::string to_string(const Material_Blend_Mode& mode);
enum Particle_Emission_Type;
enum Particle_Physics_Type;
std::string to_string(const Light_Type &value);
std::string to_string(const Particle_Emission_Type& t);
std::string to_string(const Particle_Physics_Type& t);
std::string to_string(const glm::mat4 &m);
std::string to_string(Array_String &s);
std::string to_string(const vec4 &value);
std::string to_string(const vec3 &v);
std::string to_string(const quat &q);
std::string to_string(std::string_view& value);
// std::string to_string(ivec4& value);

glm::mat4 copy(aiMatrix4x4 m);
vec4 rgb_vec4(uint8 r, uint8 g, uint8 b);
float64 random_between(float64 min, float64 max);
int32 random_between(int32 min, int32 max);
float32 random_between(float32 min, float32 max);
glm::vec3 random_between(glm::vec3 min, glm::vec3 max);
glm::vec2 random_within(const vec2 &vec);
glm::vec3 random_within(const vec3 &vec);
glm::vec4 random_within(const vec4 &vec);
vec2 random_2D_unit_vector();
vec3 random_3D_unit_vector();
vec2 random_2D_unit_vector(float32 azimuth_min, float32 azimuth_max);
vec3 random_3D_unit_vector(float32 azimuth_min, float32 azimuth_max, float32 altitude_min, float32 altitude_max);
// Uint32 string_to_U32_color(std::string color);
// glm::vec4 string_to_float4_color(std::string color);
const char *texture_format_to_string(GLenum texture_format);
const char *filter_format_to_string(GLenum filter_format);
bool has_img_file_extension(std::string name);
bool has_hdr_file_extension(std::string name);
bool is_float_format(GLenum texture_format);
int32 save_texture(Texture *texture, std::string filename, uint32 level = 0);
int32 save_texture_type(Texture *texture, std::string filename, std::string type = "png", uint32 level = 0);
int32 save_texture_type(GLuint texture, ivec2 size, std::string filename, std::string type = "png", uint32 level = 0);
uint32 type_of_float_format(GLenum texture_format);
uint32 components_of_float_format(GLenum texture_format);

std::string find_free_filename(std::string filename, std::string extension);
std::string strip_file_extension(std::string file);
Uint64 dankhash(const float32 *data, uint32 size);
void checkSDLError(int32 line = -1);
void check_gl_error();
float64 get_real_time();
typedef uint32_t UID;
UID uid();
std::string ip_address_string(uint32 ip_address);