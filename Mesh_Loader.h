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



#define MAX_BONES_PER_VERTEX 4
struct Vertex_Bone_Data
{
  uint32 count = 0;
  uint32 indices[MAX_BONES_PER_VERTEX];
  uint32 weights[MAX_BONES_PER_VERTEX];
};

typedef uint32 Skeletal_Animation_Keyframe_Index;
typedef uint32 Bone_Index;
#define MAX_BONE_CHILDREN 15

//is a tree structure, indices reference keyframe mat4's
struct Bone_Animation
{
  //this name must/will match the name in the node heirarchy
  //not sure why exactly
  std::string name;

  //all the keyframes
  std::vector<vec3> translations;
  std::vector<vec3> scales;
  std::vector<quat> rotations;
  std::vector<float32> timestamp;
};


//one for walk, one for jump, etc
struct Skeletal_Animation
{
  std::string name;
  float64 duration;
  float64 ticks_per_sec;
  std::vector<Bone_Animation> bone_animations;
};


//specific for each entity in the game
struct Skeletal_Animation_System
{
  uint32 currently_playing_animation = 0;
  float32 time = 0.f;

  std::vector<Skeletal_Animation> animation_set;
};


//this bone has a name, which tells us which of the
//bones in the heirarchy it is
struct Bone
{
  std::string name;
  mat4 offsetmatrix;
};


struct Mesh_Data
{
  std::vector<vec3> positions;
  std::vector<vec3> normals;
  std::vector<vec2> texture_coordinates;
  std::vector<vec3> tangents;
  std::vector<vec3> bitangents;
  std::vector<uint32> indices;
  std::vector<Vertex_Bone_Data> bone_weights;


  void reserve(uint32 size)
  {
    positions.reserve(size);
    normals.reserve(size);
    texture_coordinates.reserve(size);
    tangents.reserve(size);
    bitangents.reserve(size);
    indices.reserve(size);
    bone_weights.reserve(size);
  }

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

Mesh_Data generate_grid(ivec2 tile_count);




void add_aabb(vec3 min, vec3 max, Mesh_Data &mesh);
Mesh_Data load_mesh_plane();
std::string to_string(Mesh_Primitive p);
Mesh_Primitive s_to_primitive(std::string p);
void copy_mesh_data(std::vector<vec3> &dst, aiVector3D *src, uint32 length);
void copy_mesh_data(std::vector<vec2> &dst, aiVector3D *src, uint32 length);


struct Mesh_Descriptor
{
  Mesh_Descriptor() {}
  Mesh_Descriptor(std::string name, Mesh_Data&& md)
  {
    this->name = name;
    mesh_data = std::move(md);
  }
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

