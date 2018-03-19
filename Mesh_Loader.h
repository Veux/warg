#pragma once
#include "Globals.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <glm/glm.hpp>
#include <vector>
using namespace glm;
struct Mesh_Data
{
  std::vector<vec3> positions;
  std::vector<vec3> normals;
  std::vector<vec2> texture_coordinates;
  std::vector<vec3> tangents;
  std::vector<vec3> bitangents;
  std::vector<uint32> indices;
  std::string name;

  //two mesh data structs with the same unique_ID are assumed
  //to contain the same exact data above
  std::string unique_identifier = "NULL";
};
enum Mesh_Primitive
{
  null,
  plane,
  cube
}; 

// expects clockwise abcd vertices for front-facing side
void add_triangle(vec3 a, vec3 b, vec3 c, Mesh_Data& mesh);
void add_quad(vec3 a, vec3 b, vec3 c, vec3 d, Mesh_Data &mesh);

Mesh_Data load_mesh(Mesh_Primitive p);
Mesh_Data load_mesh_plane();

std::string to_string(Mesh_Primitive p);
Mesh_Primitive s_to_primitive(std::string p);

void copy_mesh_data(std::vector<vec3> &dst, aiVector3D *src, uint32 length);
void copy_mesh_data(std::vector<vec2> &dst, aiVector3D *src, uint32 length);
Mesh_Data load_mesh(const aiMesh *aimesh, std::string unique_identifier);
