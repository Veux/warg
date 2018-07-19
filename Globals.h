#pragma once
#include "Forward_Declarations.h"
#include "Timer.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <fstream>
#include <glbinding/gl33core/gl.h>
#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <unordered_map>
using namespace glm;
using namespace gl33core;

#define MAX_INSTANCE_COUNT 100
#define UNIFORM_LIGHT_LOCATION 20
#define MAX_LIGHTS 10 // reminder to change the Texture_Location::s1...sn shadow map enums
#define DYNAMIC_TEXTURE_RELOADING 0
#define DYNAMIC_FRAMERATE_TARGET 0
#define ENABLE_ASSERTS 1
#define ENABLE_OPENGL_ERROR_CATCHING_AND_LOG 0
#define INCLUDE_FILE_LINE_IN_LOG 0
#define MAX_TEXTURE_SAMPLERS 20
#define MAX_ANISOTROPY 8
#define FRAMEBUFFER_FORMAT GL_RGBA16F
#define SHOW_UV_TEST_GRID 0
#define POSTPROCESSING 1
#define NODE_NULL uint32(-1)
#define MAX_MESHES_PER_NODE uint32(10)
#define MAX_CHILDREN uint32(30)
#define MAX_NODES uint32(10000)
#define INPUT_BUFFER_SIZE 100

#ifdef __linux__
#define ROOT_PATH std::string("../")
#elif _WIN32
#define ROOT_PATH std::string("../")
#endif

#define MAX_FILENAME_LENGTH 128
#define MAX_ARRAY_STRING_LENGTH 128

struct Warg_State;
struct Render_Test_State;
struct Resource_Manager;

extern std::mt19937 generator;
extern const float32 dt;
extern const float32 MOVE_SPEED;
extern const float32 STEP_SIZE;
extern const float32 ATK_RANGE;
extern const float32 MOUSE_X_SENS;
extern const float32 MOUSE_Y_SENS;
extern const float32 ZOOM_STEP;
extern const float32 JUMP_IMPULSE;
extern uint32 LAST_RENDER_ENTITY_COUNT;
extern Assimp::Importer IMPORTER;

extern Resource_Manager GL_ENABLED_RESOURCE_MANAGER;
extern Resource_Manager GL_DISABLED_RESOURCE_MANAGER;
extern const std::string BASE_ASSET_PATH;
extern const std::string BASE_TEXTURE_PATH;
extern const std::string BASE_SHADER_PATH;
extern const std::string BASE_MODEL_PATH;
extern const std::string ERROR_TEXTURE_PATH;
extern Timer PERF_TIMER;
extern Timer FRAME_TIMER;
extern Timer SWAP_TIMER;
const uint32 default_assimp_flags = aiProcess_FlipWindingOrder |
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
extern std::string SCRATCH_STRING; // just used to avoid some string allocations - use only in main thread

extern bool WARG_SERVER;
// todo: make some variadic template for this crap:
bool all_equal(int32 a, int32 b, int32 c);
bool all_equal(int32 a, int32 b, int32 c, int32 d);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f, int32 g);

float32 wrap_to_range(const float32 input, const float32 min, const float32 max);
template <typename T> uint32 array_count(T t) { return sizeof(t) / sizeof(t[0]); }
float32 rand(float32 min, float32 max);
vec3 rand(vec3 max);
// used to fix double escaped or wrong-slash file paths
// probably need something better
std::string fix_filename(std::string str);
std::string copy(const aiString *str);
std::string read_file(const char *path);

#define ASSERT(x) _errr(x, __FILE__, __LINE__)

Uint32 string_to_U32_color(std::string color);
glm::vec4 string_to_float4_color(std::string color);
Uint64 dankhash(const float32 *data, uint32 size);

void checkSDLError(int32 line = -1);
void check_gl_error();

// actual program time
// don't use for game simulation
float64 get_real_time();

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

template <typename T> std::string vtos(std::vector<T> v)
{
  std::string result = "";
  for (auto &e : v)
  {
    result += std::to_string(e) + " ";
  }
  return result;
}

enum struct Light_Type;
std::string vtos(glm::vec2 v);
std::string vtos(glm::vec3 v);
std::string vtos(glm::vec4 v);
std::string qtos(glm::quat v);
std::string to_string(glm::mat4 m);
std::string to_string(Array_String &s);
template <typename T> std::string s(T value)
{
  using namespace std;
  return to_string(value);
}
template <typename T, typename... Args> std::string s(T first, Args... args) { return s(first) + s(args...); }
template <> std::string s<const char *>(const char *value);
template <> std::string s<std::string>(std::string value);
template <> std::string s<Light_Type>(Light_Type value);
template <> std::string s<vec4>(vec4 value);
template <> std::string s<vec4>(vec4 value);

struct Array_String
{
  Array_String() { str[0] = '\0'; }
  Array_String(std::string &rhs)
  {
    ASSERT(rhs.size() <= MAX_ARRAY_STRING_LENGTH);
    SDL_strlcpy(&str[0], &rhs[0], rhs.size() + 1);
  }

  Array_String &operator=(std::string &rhs)
  {
    ASSERT(rhs.size() <= MAX_ARRAY_STRING_LENGTH);
    SDL_strlcpy(&str[0], &rhs[0], rhs.size() + 1);
    return *this;
  }
  Array_String(const char *rhs) : Array_String(std::string(rhs)) {}
  Array_String &operator=(const char *rhs)
  {
    *this = std::string(rhs);
    return *this;
  }
  bool operator==(Array_String &rhs);
  bool operator==(const char *rhs)
  {
    Array_String r = s(rhs);
    return *this == r;
  }

  std::array<char, MAX_ARRAY_STRING_LENGTH + 1> str;
};

typedef uint32_t UID;
UID uid();

struct Bezier_Curve
{
  Bezier_Curve() {}
  Bezier_Curve(std::vector<glm::vec4> pts) : points(pts) {}
  glm::vec4 lerp(float t)
  {
    if (remainder.size() == 0)
    {
      remainder = points;
    }
    if (remainder.size() == 1)
    {
      glm::vec4 p = remainder[0];
      remainder.clear();
      return p;
    }

    for (uint32 i = 0; i + 1 < remainder.size(); ++i)
    {
      glm::vec4 p = glm::mix(remainder[i], remainder[i + 1], t);
      remainder[i] = p;
    }
    remainder.pop_back();

    return lerp(t);
  }
  std::vector<glm::vec4> points;

private:
  std::vector<glm::vec4> remainder;
};

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
extern Config CONFIG;

struct Image_Data
{
  void *data;
  int32 x;
  int32 y;
  int32 comp;
  GLenum format;
  bool initialized = false;
};

class Image_Loader
{
public:
  void init();
  bool load(const char *filename, int32 req_comp, Image_Data *data);

private:
  std::unordered_map<std::string, Image_Data> database;
  std::queue<std::string> load_queue;
  std::mutex db_mtx;
  std::mutex queue_mtx;
  std::thread loader_thread;
};

const char *texture_format_to_string(GLenum texture_format);

bool has_img_file_extension(std::string name);
bool is_float_format(GLenum texture_format);
std::string strip_file_extension(std::string file);

extern Image_Loader IMAGE_LOADER;

vec4 rgb_vec4(uint8 r, uint8 g, uint8 b);

float64 random_between(float64 min, float64 max);
int32 random_between(int32 min, int32 max);
float32 random_between(float32 min, float32 max);
glm::vec2 random_within(const vec2 &vec);
glm::vec3 random_within(const vec3 &vec);
glm::vec4 random_within(const vec4 &vec);