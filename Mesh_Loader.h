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

//just the import data of a single bone - its name and offset matrices
struct Bone
{
  //must be the unique assimp-given name because we have to search for it
  std::string name;

  //translations to and from our bone's model space position
  mat4 offset; //model to bone
  mat4 inverse_offset;//bone to model
};



//not all the bones in the draw call - only for this specific vert
#define MAX_BONES_PER_VERTEX 4
struct Vertex_Bone_Data
{
  std::array<uint32, MAX_BONES_PER_VERTEX> indices = {0,0,0,0};
  std::array<float32, MAX_BONES_PER_VERTEX> weights = {0.f,0.f,0.f,0.f};
};

typedef uint32 Skeletal_Animation_Keyframe_Index;
typedef uint32 Bone_Index;


//this is a named bone with keyframes 
//is a constant - does not change - reference data
struct Bone_Animation
{
  //this name must/will match the name in the node heirarchy
  //not sure why exactly
  
  //our import_bone_data bones have names

  //we search for it inside of animation_resolve
  std::string bone_node_name;



  //should the offset matrices be in here?
  //no - we might have tons of animations
  //yet we still only have a few bones per model


  //all the keyframes
  std::vector<vec3> translations;
  std::vector<vec3> scales;
  std::vector<quat> rotations;
  std::vector<float32> timestamp;
};

//one for walk, one for jump, etc
//is a constant - does not change - reference data
struct Skeletal_Animation
{
  std::string name;
  float64 duration;
  float64 ticks_per_sec;
  std::vector<Bone_Animation> bone_animations;
};

//a set of available animations that a model can use
//is a constant - does not change - reference data
struct Skeletal_Animation_Set
{
  std::vector<Skeletal_Animation> animation_set;
};

//specific for each entity in the game
//points at an animation set, and contains the entity's
//computed pose result
struct Skeletal_Animation_State
{
  uint32 currently_playing_animation = 0;
  float32 time = 0.f;

  //pointer to our available animations
  uint32 animation_set_index = NODE_NULL;  

  //pointer to our model's bones
  uint32 model_bone_set_index = NODE_NULL;



  //bone names to their completed transformations
  std::unordered_map<std::string,mat4> final_bone_transforms;
};
 
struct Model_Bone_Set
{
  std::unordered_map<std::string, Bone> bones;
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

    //not reserving this because we use it as a flag to see
    //if there are any bones
    //it gets resized anyway on import
    //bone_weights.reserve(size);
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
  std::vector<Bone> bones;

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

