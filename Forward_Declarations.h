#pragma once
struct Array_String;
struct aiString;
struct Material;
struct Material_Descriptor;
struct Scene_Graph;
struct Scene_Graph_Node;
struct Framebuffer;
struct Mesh_Data;
struct Environment_Map;
struct Environment_Map_Descriptor;
struct Texture_Descriptor;
struct Mesh_Descriptor;
struct Light_Array;
struct Light;
struct Warg_State;
struct Render_Test_State;
struct Resource_Manager;
enum struct Light_Type;
enum struct Material_Blend_Mode;
struct Timer;
struct Config;
struct Octree;
struct Image_Loader;
struct Texture;
struct Texture_Descriptor;
struct SDL_Imgui_State;
struct Texture_Paint;
struct State;
struct Warg_State;
struct Warg_Server;
struct Context_Managed_Imgui;
typedef glm::uint32 Node_Index;
typedef glm::uint32 Model_Index;
typedef glm::uint32 Mesh_Index;
typedef glm::uint32 Material_Index;

#include <glm/glm.hpp>
using namespace glm;
#include <array>
#include <atomic>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <assimp/cimport.h>
#include <cstring>
#include <enet/enet.h>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>
#include <queue>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <sstream>
#include <stack>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL_image.h>
#include <time.h>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>
#include <Windows.h>