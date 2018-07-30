#pragma once
#include "Forward_Declarations.h"
#include "Timer.h"
#include "Array_String.h"
#include <glbinding/Binding.h>
#include <glbinding/gl33core/gl.h>
#include <unordered_map>
#include <queue>
#include <mutex>
using namespace gl;
using namespace gl33core;
#define ASSERT(x) _errr(x, __FILE__, __LINE__)

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
#ifndef DISABLE_ASSERT
  if (!t)
  {
    set_message("", "Assertion failed in:" + std::string(file) + "\non line:" + std::to_string(line), 1.0);
    std::string message_log = get_message_log();
    push_log_to_disk();
    std::string end_of_log;
    uint32 length = message_log.size();
    uint32 char_count = 1000;
    if (length < char_count)
      char_count = length;
    else
    {
      end_of_log = message_log.substr(length - char_count, std::string::npos);
    }
    std::cout << end_of_log << std::endl;
    SDL_Delay(500);
    throw;
  }
#endif
}

struct Config
{ // if you add more settings, be sure to put them in load() and save()
  ivec2 resolution = ivec2(1280, 720);
  float32 render_scale = 1.0f;
  float32 fov = 60;
  float32 shadow_map_scale = 1.0f;
  bool use_low_quality_specular = false;
  bool render_simple = false;

  void load(std::string filename);
  void save(std::string filename);
};

struct Image_Data
{
  void *data = nullptr;
  int32 x;
  int32 y;
  int32 comp;
  GLenum format;
  bool initialized = false;
};

struct Image_Loader
{
public:
  Image_Loader();
  ~Image_Loader();
  bool load(std::string filename, Image_Data *data);
  static void loader_loop(Image_Loader *loader);

private:
  std::unordered_map<std::string, Image_Data> database;
  std::queue<std::string> load_queue;
  std::mutex db_mtx;
  std::mutex queue_mtx;
  std::atomic<uint32> queue_size;
  bool running = true;
  std::thread threads[4];
};

bool all_equal(int32 a, int32 b, int32 c);
bool all_equal(int32 a, int32 b, int32 c, int32 d);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f, int32 g);
float32 wrap_to_range(const float32 input, const float32 min, const float32 max);
template <typename T> uint32 array_count(T t)
{
  return sizeof(t) / sizeof(t[0]);
}
float32 rand(float32 min, float32 max);
vec3 rand(vec3 max);
// used to fix double escaped or wrong-slash file paths
// probably need something better
std::string fix_filename(std::string str);
std::string copy(const aiString *str);
std::string read_file(const char *path);
std::string vtos(glm::vec2 v);
std::string vtos(glm::vec3 v);
std::string vtos(glm::vec4 v);
std::string qtos(glm::quat v);

std::string to_string(Light_Type value);
std::string to_string(glm::mat4 m);
std::string to_string(Array_String &s);
std::string to_string(vec4 value);

vec4 rgb_vec4(uint8 r, uint8 g, uint8 b);
float64 random_between(float64 min, float64 max);
int32 random_between(int32 min, int32 max);
float32 random_between(float32 min, float32 max);
glm::vec2 random_within(const vec2 &vec);
glm::vec3 random_within(const vec3 &vec);
glm::vec4 random_within(const vec4 &vec);
vec2 random_2D_unit_vector();
vec3 random_3D_unit_vector();
vec2 random_2D_unit_vector(float32 azimuth_min, float32 azimuth_max);
vec3 random_3D_unit_vector(float32 azimuth_min, float32 azimuth_max, float32 altitude_min, float32 altitude_max);
Uint32 string_to_U32_color(std::string color);
glm::vec4 string_to_float4_color(std::string color);
const char *texture_format_to_string(GLenum texture_format);
bool has_img_file_extension(std::string name);
bool is_float_format(GLenum texture_format);
std::string strip_file_extension(std::string file);
Uint64 dankhash(const float32 *data, uint32 size);
void checkSDLError(int32 line = -1);
void check_gl_error();
float64 get_real_time();
typedef uint32_t UID;
UID uid();
