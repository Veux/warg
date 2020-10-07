#include "stdafx.h"
#include "Globals.h"
#include "Scene_Graph.h"

#ifdef NDEBUG
const float32 dt = 1.0f / 120.0f;
#else
const float32 dt = 1.0f / 60.f;
#endif
const float32 MOVE_SPEED = 4.f;
const float32 STEP_SIZE = MOVE_SPEED / 3.f;
const float32 MOUSE_X_SENS = .0041f;
const float32 MOUSE_Y_SENS = .0041f;
const float32 ZOOM_STEP = 0.5f;
const float32 ATK_RANGE = 5.0f;
const float32 JUMP_IMPULSE = 4.0f;
const std::string BASE_ASSET_PATH = ROOT_PATH + "Assets/";
const std::string BASE_TEXTURE_PATH = BASE_ASSET_PATH + std::string("Textures/");
const std::string BASE_SHADER_PATH = BASE_ASSET_PATH + std::string("Shaders/");
const std::string BASE_MODEL_PATH = BASE_ASSET_PATH + std::string("Models/");
const std::string ERROR_TEXTURE_PATH = BASE_TEXTURE_PATH + "err.png";

std::mt19937 generator;
std::string SCRATCH_STRING;
Timer PERF_TIMER = Timer(1000);
Timer FRAME_TIMER = Timer(60);
Timer SWAP_TIMER = Timer(60);
Config CONFIG;
bool WARG_SERVER;
bool WARG_RUNNING = true;
bool PROCESS_USES_SDL_AND_OPENGL = true;
std::thread::id MAIN_THREAD_ID;
SDL_Imgui_State IMGUI;
std::mutex IMGUI_MUTEX;
std::vector<Imgui_Texture_Descriptor> IMGUI_TEXTURE_DRAWS;
std::vector<Imgui_Texture_Descriptor> IMGUI_TEXTURE_CACHE;
