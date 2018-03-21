#pragma once
#include "Render.h"
#include "Scene_Graph.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#define JSON_INDENT 2

void to_json(json &result, const Mesh &p);
void from_json(const json &j, Mesh &p);

void to_json(json &result, const Material &p);
void from_json(const json &j, Material &p);

void to_json(json &result, const Texture_Descriptor &p);
void from_json(const json &j, Texture_Descriptor &p);

void to_json(json &result, const Material_Descriptor &p);
void from_json(const json &j, Material_Descriptor &p);

void to_json(json &result, const Light_Array &p);
void from_json(const json &j, Light_Array &p);

void to_json(json &result, const std::shared_ptr<Scene_Graph_Node> &node_ptr);
void to_json(json &result, const Scene_Graph_Node &node);
Node_Ptr build_node_graph_from_json(const json &j, Scene_Graph &scene);

void to_json(json &result, const Scene_Graph &scene);
void from_json(const json &k, Scene_Graph &scene);

void to_json(json &result, const Light &p);
void from_json(const json &j, Light &p);

namespace nlohmann
{
template <> struct adl_serializer<glm::ivec2>
{
  static void to_json(json &result, const glm::ivec2 &p)
  {
    json j;
    for (uint32 i = 0; i < 2; ++i)
    {
      j.push_back(p[i]);
    }
    result = j;
  }
  static void from_json(const json &j, glm::ivec2 &p)
  {
    ivec2 v;
    for (uint32 i = 0; i < 2; ++i)
    {
      v[i] = j.at(i);
    }
    p = v;
  }
};

template <> struct adl_serializer<glm::vec2>
{
  static void to_json(json &result, const glm::vec2 &p)
  {
    json j;
    for (uint32 i = 0; i < 2; ++i)
    {
      j.push_back(p[i]);
    }
    result = j;
  }
  static void from_json(const json &j, glm::vec2 &p)
  {
    vec2 v;
    for (uint32 i = 0; i < 2; ++i)
    {
      v[i] = j.at(i);
    }
    p = v;
  }
};

template <> struct adl_serializer<glm::vec3>
{
  static void to_json(json &result, const glm::vec3 &p)
  {
    json j;
    for (uint32 i = 0; i < 3; ++i)
    {
      j.push_back(p[i]);
    }
    result = j;
  }
  static void from_json(const json &j, glm::vec3 &p)
  {
    vec3 v;
    for (uint32 i = 0; i < 3; ++i)
    {
      v[i] = j.at(i);
    }
    p = v;
  }
};

template <> struct adl_serializer<glm::vec4>
{
  static void to_json(json &result, const glm::vec4 &p)
  {
    json j;
    for (uint32 i = 0; i < 4; ++i)
    {
      j.push_back(p[i]);
    }
    result = j;
  }
  static void from_json(const json &j, glm::vec4 &p)
  {
    vec4 v;
    for (uint32 i = 0; i < 4; ++i)
    {
      v[i] = j.at(i);
    }
    p = v;
  }
};
template <> struct adl_serializer<glm::quat>
{
  static void to_json(json &result, const glm::quat &p)
  {
    json j;
    for (uint32 i = 0; i < 4; ++i)
    {
      j.push_back(p[i]);
    }
    result = j;
  }
  static void from_json(const json &j, glm::quat &p)
  {
    quat v;
    for (uint32 i = 0; i < 4; ++i)
    {
      v[i] = j.at(i);
    }
    p = v;
  }
};

template <> struct adl_serializer<glm::mat4>
{
  static void to_json(json &result, const glm::mat4 &p)
  { // todo: change this to use 4 vec4s instead so its easier to read in the
    // json
    // file
    json j;
    for (uint32 i = 0; i < 4; ++i)
    {
      for (uint32 k = 0; k < 4; ++k)
      {
        float f = p[i][k];
        j.push_back(f);
      }
    }
    result = j;
  }
  static void from_json(const json &j, glm::mat4 &p)
  {
    mat4 v;
    for (uint32 i = 0; i < 4; ++i)
    {
      for (uint32 k = 0; k < 4; ++k)
      {
        float f = j.at(i * 4 + k);
        v[i][k] = f;
      }
    }
    p = v;
  }
};
}

// Note: Use imgui to add things, rather than constructor code, because
// you will keep getting more duplicates every time you relaunch the game

// todo: Limitations: Modifications (other than materials) done to child nodes
// created by assimp imports will not be restored with dejsonificate.

// Ideally the user would be able to specify if the vertex data should be
// encoded entirely within the json file or not, this would allow to save
// absolute model state if turned on or, the user could turn it off and the
// models in the scene would be updated automatically if the asset file was
// modified.
//
// This requires storing a handle to the Mesh_Data from the import in
// the Mesh struct, a flag in the Node_Ptr to change the save method in
// jsonify(), and a vertex data hash to be able to generate a unique_identifier
// for the raw data, in order to use the MESH_CACHE

// Will only save and restore nodes that are flagged
// Scene_Graph_Node::include_in_next_save = true;  Remember to set this flag for
// Scene_Graph::root and all nodes in the chain  The nodes to be saved must also
// be owned by their parent, use set_parent(node,parent,true);  on nodes you
// wish to be included in the json.  Note: in order to delete those nodes, you
// must explicitly remove them from the parent's owned_children vector, or call
// set_parent(node,parent,false) and drop the Node_Ptr handle;
json jsonify(Scene_Graph &scene);

// does the magical dejsonificationing, will dump the json to warg_log if it
// fails and default-construct the scene
void dejsonificate(Scene_Graph *scene, json j);

void _pretty_json(std::string &pretty_json, const std::string &json_string,
    std::string::size_type &pos, size_t le = 0, size_t indent = JSON_INDENT);

std::string pretty_dump(const json &j);
