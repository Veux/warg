#pragma once
#include "Globals.h"
#include "Render.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <atomic>
#include <glm/glm.hpp>
#include <unordered_map>
struct Material;
struct Material_Descriptor;
struct Scene_Graph;
struct Scene_Graph_Node;

typedef std::shared_ptr<Scene_Graph_Node> Node_Ptr;





struct Scene_Graph_Node
{
  Scene_Graph_Node() {}
  std::string name;
  vec3 position = {0, 0, 0};
  quat orientation;
  vec3 scale = {1, 1, 1};
  vec3 velocity = {0, 0, 0};
  std::vector<std::pair<Mesh, Material>> model;

  // controls whether the entity will be rendered
  bool visible = true;
  // controls whether the visibility affects all children under this node in the
  // tree, or just this specific node
  bool propagate_visibility = true;

  Scene_Graph_Node(std::string name, const mat4 *import_basis = nullptr);
  Scene_Graph_Node(std::string name, const aiNode *node,
                   const mat4 *import_basis_, const aiScene *scene,
                   std::string scene_path, Uint32 *mesh_num,
                   Material_Descriptor *material_override);

protected:
  friend Scene_Graph;
  // assimp's import mtransformation, propagates to children
  mat4 basis = mat4(1);

  // user-defined import_basis does NOT propagate down the matrix stack
  // applied before all other transformations
  // essentially, this takes us from import basis -> our world basis
  // should be the same for every node that was part of the same import
  mat4 import_basis = mat4(1);

  std::weak_ptr<Scene_Graph_Node> parent;

  std::vector<std::shared_ptr<Scene_Graph_Node>> owned_children;
  std::vector<std::weak_ptr<Scene_Graph_Node>> unowned_children;
};

struct Scene_Graph
{
  Scene_Graph();

  // makes all transformations applied to ptr relative to the parent
  // parent_owned indicates whether or not the parent owns the pointer and
  // resource  use false if you will manage the ownership of the Node_Ptr yourself
  // use true if you would like the parent entity to own the pointer
  void set_parent(std::weak_ptr<Scene_Graph_Node> ptr,
                  std::weak_ptr<Scene_Graph_Node> desired_parent,
                  bool parent_owned = false);

  std::shared_ptr<Scene_Graph_Node>
  add_aiscene(std::string scene_file_path,
              Material_Descriptor *material_override = nullptr);

  std::shared_ptr<Scene_Graph_Node>
  add_aiscene(std::string scene_file_path, const mat4 *import_basis,
              Material_Descriptor *material_override = nullptr);

  std::shared_ptr<Scene_Graph_Node>
  add_aiscene(const aiScene *scene, std::string asset_path,
              const mat4 *import_basis = nullptr,
              Material_Descriptor *material_override = nullptr);

  // construct a node using the load_mesh function in Mesh_Loader
  // does not yet check for nor cache duplicate meshes/materials
  // Node_Ptr will stay valid for as long as the Scene_Graph is alive
  std::shared_ptr<Scene_Graph_Node>
  add_primitive_mesh(Mesh_Primitive p, std::string name, Material_Descriptor m,
                     const mat4 *import_basis = nullptr);

  std::shared_ptr<Scene_Graph_Node>
  add_mesh(Mesh_Data m, Material_Descriptor md, std::string name,
           const mat4 *import_basis = nullptr);

  // traverse the entire graph, computing the final transformation matrices
  // for each entity, and return all entities flattened into a vector
  // async currently performance bugged on w7
  std::vector<Render_Entity> visit_nodes_async_start();

  // traverse the entire graph, computing the final transformation matrices
  // for each entity, and return all entities flattened into a vector
  std::vector<Render_Entity> visit_nodes_st_start();

  Light_Array lights;

  std::shared_ptr<Scene_Graph_Node> root;

private:
  // add a Scene_Graph_Node to the Scene_Graph using an aiNode, aiScene, and
  // parent Node_Ptr
  void add_graph_node(const aiNode *node,
                      std::weak_ptr<Scene_Graph_Node> parent,
                      const mat4 *import_basis, const aiScene *aiscene,
                      std::string path, Uint32 *mesh_num,
                      Material_Descriptor *material_override);

  uint32 last_accumulator_size = 0;

  // various node traversal algorithms
  void visit_nodes(std::weak_ptr<Scene_Graph_Node> node_ptr, const mat4 &M,
                   std::vector<Render_Entity> &accumulator);
  void visit_nodes_locked_accumulator(std::weak_ptr<Scene_Graph_Node> node_ptr,
                                      const mat4 &M,
                                      std::vector<Render_Entity> *accumulator,
                                      std::atomic_flag *lock);
  void visit_root_node_base_index(uint32 node_index, uint32 count,
                                  std::vector<Render_Entity> *accumulator,
                                  std::atomic_flag *lock);
};
