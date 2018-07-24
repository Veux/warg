#pragma once
struct Array_String;
struct aiString;
struct Material;
struct Material_Descriptor;
struct Flat_Scene_Graph;
struct Flat_Scene_Graph_Node;
struct Framebuffer;
struct Mesh_Data;
struct Environment_Map;
struct Environment_Map_Descriptor;
struct Texture_Descriptor;
struct Mesh_Descriptor;
struct Light_Array;
struct Light;
struct Mesh_Index;
struct Material_Index;
struct Warg_State;
struct Render_Test_State;
struct Resource_Manager;
enum struct Light_Type;
struct Timer;
struct Config;
struct Image_Loader;
struct Texture;
struct Texture_Descriptor;

#include <glm/glm.hpp>
using namespace glm;
typedef uint32 Node_Index;
typedef uint32 Model_Index;
#include "Globals.h"
#include "Forward_Definitions.h"