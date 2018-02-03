#pragma once
#include "Timer.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <fstream>
#include <glbinding/gl33core/gl.h>
#include <glm/glm.hpp>
#include <iostream>
#include <random>
#include <unordered_map>
using namespace glm;
using namespace gl33core;
struct aiString;

#define MAX_INSTANCE_COUNT 100
#define UNIFORM_LIGHT_LOCATION 20
#define MAX_LIGHTS 10  // reminder to change the Texture_Location::s1...sn shadow map enums
#define DYNAMIC_TEXTURE_RELOADING 1
#define DYNAMIC_FRAMERATE_TARGET 0
#define DEBUG 1
#define ENABLE_ASSERTS 1
#define ENABLE_OPENGL_ERROR_CATCHING_AND_LOG 0
#define INCLUDE_FILE_LINE_IN_LOG 0
#define MAX_TEXTURE_SAMPLERS 20

struct Warg_State;
struct Render_Test_State;

extern std::mt19937 generator;
extern const float32 dt;
extern const float32 MOVE_SPEED;
extern const float32 ATK_RANGE;
extern const float32 MOUSE_X_SENS;
extern const float32 MOUSE_Y_SENS;
extern const float32 ZOOM_STEP;
extern uint32 LAST_RENDER_ENTITY_COUNT;

extern const std::string BASE_ASSET_PATH;
extern const std::string BASE_TEXTURE_PATH;
extern const std::string BASE_SHADER_PATH;
extern const std::string BASE_MODEL_PATH;
extern const std::string ERROR_TEXTURE_PATH;
extern Timer PERF_TIMER;

// load an aiScene
// will cache the result so that future loads
// of the same path won't have to read from disk
extern const aiScene *load_aiscene(std::string path,
                                   const int *assimp_flags = nullptr);

// todo: make some variadic template for this crap:
bool all_equal(int32 a, int32 b, int32 c);
bool all_equal(int32 a, int32 b, int32 c, int32 d);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f, int32 g);

float32 wrap_to_range(const float32 input, const float32 min,
                      const float32 max);
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

#define ASSERT(x) _errr(x, __FILE__, __LINE__)

Uint32 string_to_color(std::string color);
Uint64 dankhash(float32 *data, uint32 size);


void checkSDLError(int32 line = -1);
void check_gl_error();

// actual program time
// don't use for game simulation
float64 get_real_time();

void __set_message(std::string identifier, std::string message,
  float64 msg_duration, const char *, uint32);
#define CREATE_3(x, y, z) __set_message(x, y, z, __FILE__, __LINE__)
#define CREATE_2(x, y) CREATE_3(x, y, 0.0)
#define CREATE_1(x) CREATE_2(x, "")
#define CREATE_0() CREATE_1("")

#define FUNC_CHOOSER(_f1, _f2, _f3, _f4, ...) _f4
#define FUNC_RECOMPOSER(argsWithParentheses) FUNC_CHOOSER argsWithParentheses
#define CHOOSE_FROM_ARG_COUNT(...)                                             \
  FUNC_RECOMPOSER((__VA_ARGS__, CREATE_3, CREATE_2, CREATE_1, ))
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
    set_message("",
                "Assertion failed in:" + std::string(file) +
                    "\non line:" + std::to_string(line),
                1.0);
    std::cout << get_message_log() << std::endl;
    push_log_to_disk();
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
std::string vtos(glm::vec2 v);
std::string vtos(glm::vec3 v);
std::string vtos(glm::vec4 v);
std::string mtos(glm::mat4 m);

template <typename T> std::string s(T value) { return std::to_string(value); }

template <typename T, typename... Args> std::string s(T first, Args... args)
{
  return s(first) + s(args...);
}
template <> std::string s<const char *>(const char *value);
template <> std::string s<std::string>(std::string value);
