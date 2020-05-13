#pragma once
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <glm/glm.hpp>
#include <vector>
#include "Globals.h"
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

  std::string build_unique_identifier() const
  {
    if (positions.size() == 0)
      return "";
    uint64 p = dankhash(&positions[0][0], 3 * uint32(positions.size()));
    return s(p);
  }
};
void add_triangle(vec3 a, vec3 b, vec3 c, Mesh_Data &mesh);
void add_quad(vec3 a, vec3 b, vec3 c, vec3 d, Mesh_Data &mesh);
Mesh_Data load_mesh(Mesh_Primitive p);
Mesh_Data load_mesh_cube();

void add_aabb(vec3 min, vec3 max, Mesh_Data &mesh);
Mesh_Data load_mesh_plane();
std::string to_string(Mesh_Primitive p);
Mesh_Primitive s_to_primitive(std::string p);
void copy_mesh_data(std::vector<vec3> &dst, aiVector3D *src, uint32 length);
void copy_mesh_data(std::vector<vec2> &dst, aiVector3D *src, uint32 length);


struct Mesh_Descriptor
{
  Mesh_Descriptor() {}
  Mesh_Descriptor(Mesh_Primitive p, std::string name)
  {
    mesh_data = load_mesh(p);
    this->name = name;
  }
  std::string name = "";
  Mesh_Data mesh_data;
  std::string unique_identifier = "";
  const std::string& get_unique_identifier()  {
    if (unique_identifier == "")
    { 
      set_message("Warning, unique_identifier is not set in mesh_descriptor", s(name), 10.f);
      return mesh_data.build_unique_identifier();
    }
    return unique_identifier;
  }
 // std::string build_unique_identifier() const
 // {
  //  return s(name,assimp_filename, assimp_index, mesh_data.build_unique_identifier());
 // }
};

Mesh_Descriptor build_mesh_descriptor(const aiScene *scene, uint32 i, std::string path);