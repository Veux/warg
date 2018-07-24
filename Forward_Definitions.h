#pragma once
#include "Globals.h"
struct Mesh_Index
{
  uint32 i = NODE_NULL;
  uint32 assimp_original = NODE_NULL;
  // if true, the index will point to an item in the resource manager mesh pool instead of the
  // import piece so that it can be changed at runtime to an arbitrary mesh
  bool use_mesh_index_for_mesh_pool = true;
};
struct Material_Index
{
  uint32 i = NODE_NULL;
  uint32 assimp_original = NODE_NULL;
  // if true, the index will point to an item in the resource manager material pool instead of the
  // import piece so that it can be changed at runtime to an arbitrary material
  bool use_material_index_for_material_pool = true;
};