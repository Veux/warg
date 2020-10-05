#pragma once
#include <glm/glm.hpp>
#include <string>
#include <random>
#include <mutex>
#include <thread>
#include "SDL_Imgui_State.h"
#include "General_Purpose.h"
#include "Timer.h"
#define blah 1
#define MAX_INSTANCE_COUNT 40000
#define UNIFORM_LIGHT_LOCATION 20
#define MAX_LIGHTS 10 // reminder to change the Texture_Location::s1...sn shadow map enums
#define DYNAMIC_TEXTURE_RELOADING 0
#define DYNAMIC_FRAMERATE_TARGET 0
#define ENABLE_ASSERTS 1
#define ENABLE_OPENGL_ERROR_CATCHING_AND_LOG 1
#define INCLUDE_FILE_LINE_IN_LOG 1
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
#define MAX_CHARACTERS 100
#define MAX_SPELL_OBJECTS 10
#define MAX_CHARACTER_NAME_LENGTH 16
#define MAX_BUFFS 16
#define MAX_DEBUFFS 16
#define MAX_SPELLS 16
#define MAX_SPELLS 16
#define MAX_FILENAME_LENGTH 128

#ifdef NDEBUG
#define HEIGHTMAP_RESOLUTION 512
#else
#define HEIGHTMAP_RESOLUTION 64
#endif


#ifdef __linux__
#define ROOT_PATH std::string("../")
#elif _WIN32
#define ROOT_PATH std::string("")
#endif

#define MAX_ARRAY_STRING_LENGTH 64


template <typename T> std::string s(T value)
{
  using namespace std;
  return to_string(value);
}
template <typename T, typename... Args> std::string s(T first, Args... args)
{
  return s(first) + s(args...);
}
template <> std::string s<const char *>(const char *value);
template <> std::string s<std::string>(std::string value);

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
extern const std::string BASE_ASSET_PATH;
extern const std::string BASE_TEXTURE_PATH;
extern const std::string BASE_SHADER_PATH;
extern const std::string BASE_MODEL_PATH;
extern const std::string ERROR_TEXTURE_PATH;
extern Timer PERF_TIMER;
extern Timer FRAME_TIMER;
extern Timer SWAP_TIMER;
extern Config CONFIG;
extern Image_Loader IMAGE_LOADER;
extern bool WARG_SERVER;
extern bool WARG_RUNNING;
extern bool PROCESS_USES_SDL_AND_OPENGL;
extern std::thread::id MAIN_THREAD_ID;
extern std::mutex IMGUI_MUTEX;
extern SDL_Imgui_State IMGUI;
extern SDL_Imgui_State TRASH_IMGUI;