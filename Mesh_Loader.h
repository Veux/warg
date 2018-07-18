#pragma once
#include "Globals.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <glm/glm.hpp>
#include <vector>
using namespace glm;

enum Mesh_Primitive
{
  null,
  plane,
  cube
};
struct Mesh_Data
{
  std::vector<vec3> positions;
  std::vector<vec3> normals;
  std::vector<vec2> texture_coordinates;
  std::vector<vec3> tangents;
  std::vector<vec3> bitangents;
  std::vector<uint32> indices;
  std::string name = "NULL";

  std::string build_unique_identifier() const
  {
    if (positions.size() == 0)
      return "";
    uint64 p = dankhash(&positions[0][0], 3 * positions.size());
    return s(p);
  }
  //// two mesh data structs with the same unique_ID are assumed
  //// to contain the same exact data above
  // std::string unique_identifier = "NULL";
};
struct Mesh_Descriptor
{
  Mesh_Descriptor() {}
  Mesh_Descriptor(Mesh_Primitive p, std::string name)
  {
    primitive = p;
    name = this->name;
  }

  // always used:
  std::string name = ""; // name for the mesh object itself

  // if these two are filled, scene graph can load the mesh
  std::string assimp_filename = ""; // filename of the import, if its an assimp mesh
  uint32 assimp_index = -1;         // mesh is n'th index of the assimp import

  // used for primitives:
  // don't need to fill mesh data for primitives
  Mesh_Primitive primitive = null; // used only if assimp_* is left blank

  // used for custom mesh data:
  // if true, it mesh_data must be filled out when given to scene graph
  // if false, mesh_data can be blank when loaded with the scene graph interface
  bool is_custom_mesh_data = false;

  Mesh_Data mesh_data;

  std::string build_unique_identifier() const
  {
    return s(assimp_filename, assimp_index, primitive, mesh_data.build_unique_identifier());
  }
};

void add_triangle(vec3 a, vec3 b, vec3 c, Mesh_Data &mesh);
void add_quad(vec3 a, vec3 b, vec3 c, vec3 d, Mesh_Data &mesh);
Mesh_Data load_mesh(Mesh_Primitive p);
Mesh_Data load_mesh_cube();
Mesh_Data load_mesh_plane();
std::string to_string(Mesh_Primitive p);
Mesh_Primitive s_to_primitive(std::string p);
void copy_mesh_data(std::vector<vec3> &dst, aiVector3D *src, uint32 length);
void copy_mesh_data(std::vector<vec2> &dst, aiVector3D *src, uint32 length);

Mesh_Descriptor build_mesh_descriptor(const aiScene *scene, uint32 i, std::string path);