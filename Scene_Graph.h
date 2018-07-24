#pragma once
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "SDL_Imgui_State.h"
#include "UI.h"
#include <array>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <atomic>
#include <glm/glm.hpp>
#include <unordered_map>

extern std::vector<Imgui_Texture_Descriptor> IMGUI_TEXTURE_DRAWS;
const uint32 default_assimp_flags = aiProcess_FlipWindingOrder |
                                    // aiProcess_Triangulate |
                                    // aiProcess_FlipUVs |
                                    aiProcess_CalcTangentSpace |
                                    // aiProcess_MakeLeftHanded|
                                    // aiProcess_JoinIdenticalVertices |
                                    // aiProcess_PreTransformVertices |
                                    // aiProcess_GenUVCoords |
                                    // aiProcess_OptimizeGraph|
                                    // aiProcess_ImproveCacheLocality|
                                    // aiProcess_OptimizeMeshes|
                                    // aiProcess_GenNormals|
                                    // aiProcess_GenSmoothNormals|
                                    // aiProcess_FixInfacingNormals |
                                    0;

// everything we care about in an assimp node
struct Imported_Scene_Node
{
  std::string name;
  std::vector<Imported_Scene_Node> children;
  std::vector<Mesh_Index> mesh_indices;
  std::vector<Material_Index> material_indices;
  glm::mat4 transform;
};

// everything we care about in an assimp import
struct Imported_Scene_Data
{
  std::string assimp_filename;
  std::vector<Imported_Scene_Node> children;

  // all meshes and all materials in the import
  std::vector<Mesh_Descriptor> meshes; // mesh_data filled
  std::vector<Material_Descriptor> materials;

  // allocates the resources for opengl
  void allocate_resources();
  std::vector<Mesh> allocated_meshes;
  std::vector<Material> allocated_materials;

  bool valid = false;
  bool thread_working_on_import = false;
  bool allocated = false;
};

/*
Usage:
There are two ways to add and retrieve resources:

  Custom/generated meshes using the pools:
    Add:
    push_custom_material (sync)
    push_custom_mesh (sync)

    Retrieve:
    retrieve_mesh_descriptor_from_pool (sync)
    retrieve_material_descriptor_from_pool (sync)

    retrieve_pool_mesh (gl)(sync)
    retrieve_pool_material (gl)(sync)(textures async)

  -----------------------------------------
  Assimp imports:
    Add:
    request_valid_resource(sync or async)

    Retrieve:
    retrieve_assimp_mesh_descriptor (sync or async)
    retrieve_assimp_material_descriptor (sync or async)

    retrieve_assimp_mesh_resource (gl)(sync or async)
    retrieve_assimp_material_resource (gl)(sync or sync)(textures async)



Serialization:

  serialize_mesh_descriptor_pool
  serialize_material__descriptor_pool
  deserialize_mesh_descriptor_pool
  deserialize_material_descriptor_pool



Notes:
// todo: separate thread for assimp importing

// problem: how do you refcount and free unused meshes?
// on graph traversal, ping the resource manager with +1 to the mesh count of each object that will be drawn
// resource manager itself could decide when to free
// freeing isnt necessary at all until a memory limit is nearing max anyway
// could keep track of allocated memory and then do a garbage collection pass if it gets high
*/
struct Resource_Manager
{
  Resource_Manager(bool disable_opengl = false) : opengl_disabled(disable_opengl) {}

  void init();
  std::string serialize_mesh_descriptor_pool();
  std::string serialize_material__descriptor_pool();

  // immediately allocates resource if opengl_enabled
  Material_Index push_custom_material(Material_Descriptor *d);
  Mesh_Index push_custom_mesh(Mesh_Descriptor *d);

  // immediately overwrites mesh_descriptor_pool
  // async loads assimp imports (but the assimp load will only be applied if this function is called again sometime
  // after it has finished) immediately uploads primitives/custom mesh assimp meshes are only uploaded to gpu if opengl
  // is enabled and the import is available
  void deserialize_mesh_descriptor_pool(json data);

  // immediately overwrites material_descriptor_pool
  // immediately overwrites material_pool to gpu upload the material (if opengl enabled)
  void deserialize_material_descriptor_pool(json data);

  // begins loading of the assimp resource
  // returns pointer to the import if it is ready, else returns null
  // blocks and guarantees a valid scene if wait_for_valid is true
  // resources are guaranteed to be gpu allocated if return ptr is valid
  // if wait_for_valid is false, this must be called repeatedly in order for it to
  // eventually produce the Imported_Scene_Data
  Imported_Scene_Data *request_valid_resource(std::string assimp_path, bool wait_for_valid = true);

  // retrieves a pointer to the GPU resource if available
  // returns null if assimp import hasnt finished
  Mesh *retrieve_assimp_mesh_resource(std::string assimp_path, Mesh_Index i);
  Material *retrieve_assimp_material_resource(std::string assimp_path, Material_Index i);

  // returns null if it doesnt yet exist
  Material_Descriptor *retrieve_assimp_material_descriptor(std::string assimp_path, Material_Index i);
  Mesh_Descriptor *retrieve_assimp_mesh_descriptor(std::string assimp_path, Mesh_Index i);

  // pointers invalid after call to scene_graph::add_mesh*
  // or Resource_Manager::push_custom*
  Mesh *retrieve_pool_mesh(Mesh_Index i);
  Material *retrieve_pool_material(Material_Index i);
  Material_Descriptor *retrieve_pool_material_descriptor(Material_Index i);
  Mesh_Descriptor *retrieve_pool_mesh_descriptor(Mesh_Index i);

  // simply makes changes made to the descriptor pool apply to the main pool
  void update_material_pool_index(Material_Index i);
  void push_mesh_pool_change(Mesh_Index i);

  const bool opengl_disabled;

private:
  friend Flat_Scene_Graph;
  // opens assimp file using the importer, recursively calls _import_aiscene_node to build the Imported_Scene_Data
  Imported_Scene_Data import_aiscene(std::string path, uint32 assimp_flags = default_assimp_flags);

  // must be called from main thread
  // returns nullptr if busy loading
  std::unique_ptr<Imported_Scene_Data> import_aiscene_async(
      std::string path, uint32 assimp_flags = default_assimp_flags);

  // pure
  Imported_Scene_Node _import_aiscene_node(std::string assimp_filename, const aiScene *scene, const aiNode *node);

  // to release ownership of gpu resource, simply overwrite the Imported_Scene_Data with default construct
  std::unordered_map<std::string, Imported_Scene_Data> import_data;

  std::vector<Mesh> mesh_pool;         // empty if opengl is disabled
  std::vector<Material> material_pool; // empty if opengl is disabled

  std::vector<Mesh_Descriptor> mesh_descriptor_pool;
  std::vector<Material_Descriptor> material_descriptor_pool;

  std::unique_ptr<Material> null_material;

  std::unique_ptr<Material_Descriptor> null_material_descriptor;
};

struct Flat_Scene_Graph_Node
{
  Flat_Scene_Graph_Node();
  // todo: generate aabb for everything on import
  // todo: billboarding in visit_nodes
  // todo: bounding volumes for culling
  // todo: occlusion culling
  // todo: frustrum culling
  Array_String name;
  vec3 position = {0, 0, 0};
  quat orientation = glm::angleAxis(0.f, glm::vec3(0, 0, 1));
  vec3 scale = {1, 1, 1};

  // a gotcha: be sure youre setting this to the node that has the mesh you want to scale in vertex space, and not an
  // intermediate empty node
  vec3 scale_vertex = {1, 1, 1};
  vec3 velocity = {0, 0, 0};
  mat4 import_basis = mat4(1);
  std::array<std::pair<Mesh_Index, Material_Index>, MAX_MESHES_PER_NODE> model;
  std::array<Node_Index, MAX_CHILDREN> children;
  Node_Index parent = NODE_NULL;
  Array_String filename_of_import; // is blank for non-assimp imports
  bool exists = false;
  bool visible = true;              // only controls rendering
  bool propagate_visibility = true; // only controls rendering
  bool propagate_translation = true;
  bool propagate_rotation = true;
  bool propagate_scale = true;
  bool wait_on_resource = true; // require the mesh and materials to be available immediately
};

struct Flat_Scene_Graph
{
  Flat_Scene_Graph(Resource_Manager *manager) : resource_manager(manager){};
  void clear();

  template <typename M, typename B> Node_Index add_aiscene(std::string, std::string, M, B) = delete;

  // may return NODE_NULL if async loading flag wait_on_resource == false
  Node_Index add_aiscene(std::string name, std::string scene_file_path, bool wait_on_resource = true);
  Node_Index add_aiscene(std::string name, std::string scene_file_path, Material_Descriptor *material_override,
      bool modify_or_overwrite = true, bool wait_on_resource = true);

  Node_Index add_mesh(std::string name, Mesh_Descriptor *mesh, Material_Descriptor *material);
  Node_Index add_mesh(Mesh_Primitive p, std::string name, Material_Descriptor *material);

  Node_Index new_node();

  Node_Index find_child_by_name(Node_Index parent, const char *name)
  {
    Flat_Scene_Graph_Node *ptr = &nodes[parent];
    for (uint32 i = 0; i < ptr->children.size(); ++i)
    {
      Node_Index child = ptr->children[i];
      if (child != NODE_NULL)
      {
        Flat_Scene_Graph_Node *cptr = &nodes[child];
        if (cptr->name == Array_String(name))
        {
          return child;
        }
      }
    }
    return NODE_NULL;
  }
  // grab doesnt require any particular parent/child relation before calling, it can even be a child of another parent
  void grab(Node_Index grabber, Node_Index grabee);
  // brings the node to world basis and world parent
  void drop(Node_Index child);

  // use once to bring an assimp-materialed object into the custom material pool, then mod directly from there
  void copy_all_materials_to_new_pool_slots(
      Node_Index i, Material_Descriptor *m = nullptr, bool modify_or_overwrite = true);
  Material_Index copy_material_to_new_pool_slot(
      Node_Index i, Model_Index model, Material_Descriptor *m, bool modify_or_overwrite = true);

  //
  // these require the materials to be pool pointers
  void modify_material(Material_Index i, Material_Descriptor *m, bool modify_or_overwrite = true);
  void modify_all_materials(
      Node_Index i, Material_Descriptor *m, bool modify_or_overwrite = true, bool children_too = true);

  void reset_material(Node_Index node_index, Model_Index model_index);
  void reset_all_materials(Node_Index node_index, bool children_too = true);

  // if you made a change to a Material_Descriptor* you got from a Material_Index
  // you will need to call this after making changes to it
  void push_material_change(Material_Index i);

  void delete_node(Node_Index i);
  void set_parent(Node_Index i, Node_Index desired_parent = NODE_NULL);

  std::vector<Render_Entity> visit_nodes_client_start();
  std::vector<World_Object> visit_nodes_server_start();

  // use visit nodes if you need the transforms of all objects in the graph
  glm::mat4 build_transformation(Node_Index node_index, bool include_vertex_scale = true);

  std::string serialize() const;     // string size is const: sizeof(Flat_Scene_Graph::nodes)
  void deserialize(std::string src); // instant overwrite of node array

  std::array<Flat_Scene_Graph_Node, MAX_NODES> nodes;
  Light_Array lights;

  std::vector<Particle_Emitter> particle_emitters;
  Resource_Manager *resource_manager;

  void draw_imgui();

private:
  std::vector<Render_Entity> accumulator;
  std::vector<World_Object> accumulator1;
  void visit_nodes(Node_Index Node_Index, const mat4 &M, std::vector<Render_Entity> &accumulator);

  glm::mat4 __build_transformation(Node_Index node_index);
  void visit_nodes(Node_Index Node_Index, const mat4 &M, std::vector<World_Object> &accumulator);
  void assert_valid_parent_ptr(Node_Index child);
  Node_Index add_import_node(Imported_Scene_Node *node, Node_Index parent, std::string assimp_filename);

  Node_Index add_import_node(Imported_Scene_Node *node, Node_Index parent, std::string assimp_filename,
      Material_Descriptor *material_override, bool modify_or_overwrite);

  // imgui:
  Node_Index imgui_selected_node = NODE_NULL;
  Mesh_Index imgui_selected_mesh;
  Material_Index imgui_selected_material;
  Array_String imgui_selected_import_filename;

  File_Picker texture_picker = File_Picker(".");
  bool texture_picker_in_use = false;
  uint32 texture_picking_slot = 0;

  bool imgui_open = true;
  bool command_window_open = false;
  bool showing_model = false;
  bool showing_mesh_data = false;
  bool favor_resource_mesh = false;

  void draw_imgui_specific_mesh(Mesh_Index mesh_index);
  void draw_imgui_specific_material(Material_Index material_index);
  void draw_imgui_light_array();
  void draw_imgui_command_interface();
  void draw_imgui_specific_node(Node_Index node_index);
  void draw_imgui_tree_node(Node_Index node_index);
  void draw_imgui_texture_element(const char *name, Texture_Descriptor *ptr, uint32 slot);
  void draw_imgui_const_texture_element(const char *name, Texture_Descriptor *ptr);
  void draw_imgui_resource_manager();
  void draw_active_nodes();
};
//
///*
// OLD SCENE GRAPH:
//*/
// struct Scene_Graph;
// struct Scene_Graph_Node;
//
// typedef std::shared_ptr<Scene_Graph_Node> Node_Ptr;
//
// struct Scene_Graph_Node
//{
//  Scene_Graph_Node() {}
//  std::string name;
//  vec3 position = {0, 0, 0};
//  quat orientation;
//  vec3 scale = {1, 1, 1};
//  vec3 velocity = {0, 0, 0};
//  std::vector<std::pair<Mesh, Material>> model;
//
//  bool propagate_translation = true;
//  bool propagate_rotation = true;
//  bool propagate_scale = true;
//
//  // controls whether the entity will be rendered
//  bool visible = true;
//  // controls whether the visibility affects all children under this node in the
//  // tree, or just this specific node
//  bool propagate_visibility = true;
//
//  Scene_Graph_Node(std::string name, const aiNode *node, const mat4 *import_basis_, const aiScene *scene,
//      std::string scene_path, Uint32 *mesh_num, Material_Descriptor *material_override);
//
//  std::vector<std::shared_ptr<Scene_Graph_Node>> owned_children;
//  std::vector<std::weak_ptr<Scene_Graph_Node>> unowned_children;
//
//  bool include_in_save = true;
//
//  // user-defined import_basis does NOT propagate down the matrix stack
//  // applied before all other transformations
//  // essentially, this takes us from import basis -> our world basis
//  // should be the same for every node that was part of the same import
//  mat4 import_basis = mat4(1);
//
//  // assimp's import mtransformation, propagates to children
//  mat4 basis = mat4(1);
//
//  // is blank for non-assimp imports
//  std::string filename_of_import;
//  bool is_root_of_import = false;
//
// protected:
//  friend Scene_Graph;
//
//  std::weak_ptr<Scene_Graph_Node> parent;
//};
//
// struct Scene_Graph
//{
//  Scene_Graph();
//
//  // makes all transformations applied to ptr relative to the parent
//
//  // parent_owned indicates whether or not the parent owns the pointer and
//  // resource
//  // false if you will manage the ownership of the Node_Ptr yourself
//  // true if you would like the parent entity to own the pointer
//  // todo: preserve_current_world_space_transformation
//  //
//  void set_parent(
//      std::weak_ptr<Scene_Graph_Node> ptr, std::weak_ptr<Scene_Graph_Node> desired_parent, bool parent_owned =
//      false);
//
//  std::shared_ptr<Scene_Graph_Node> add_aiscene(
//      std::string scene_file_path, Material_Descriptor *material_override = nullptr);
//
//  std::shared_ptr<Scene_Graph_Node> add_aiscene(
//      std::string scene_file_path, const mat4 *import_basis, Material_Descriptor *material_override = nullptr);
//
//  std::shared_ptr<Scene_Graph_Node> add_aiscene(const aiScene *scene, std::string asset_path,
//      const mat4 *import_basis = nullptr, Material_Descriptor *material_override = nullptr);
//
//  // construct a node using the load_mesh function in Mesh_Loader
//  // does not yet check for nor cache duplicate meshes/materials
//  // Node_Ptr will stay valid for as long as the Scene_Graph is alive
//  std::shared_ptr<Scene_Graph_Node> add_primitive_mesh(
//      Mesh_Primitive p, std::string name, Material_Descriptor m, const mat4 *import_basis = nullptr);
//
//  std::shared_ptr<Scene_Graph_Node> add_mesh(
//      Mesh_Data m, Material_Descriptor md, std::string name, const mat4 *import_basis = nullptr);
//
//  // traverse the entire graph, computing the final transformation matrices
//  // for each entity, and return all entities flattened into a vector
//  // async currently performance bugged on w7
//  std::vector<Render_Entity> visit_nodes_async_start();
//
//  // traverse the entire graph, computing the final transformation matrices
//  // for each entity, and return all entities flattened into a vector
//  std::vector<Render_Entity> visit_nodes_st_start();
//
//  Light_Array lights;
//
//  std::shared_ptr<Scene_Graph_Node> root;
//
// private:
//  // add a Scene_Graph_Node to the Scene_Graph using an aiNode, aiScene, and
//  // parent Node_Ptr
//  void add_graph_node(const aiNode *node, std::weak_ptr<Scene_Graph_Node> parent, const mat4 *import_basis,
//      const aiScene *aiscene, std::string path, Uint32 *mesh_num, Material_Descriptor *material_override);
//
//  uint32 last_accumulator_size = 0;
//
//  // various node traversal algorithms
//  void visit_nodes(std::weak_ptr<Scene_Graph_Node> node_ptr, const mat4 &M, std::vector<Render_Entity>
//  &accumulator); void visit_nodes_locked_accumulator(std::weak_ptr<Scene_Graph_Node> node_ptr, const mat4 &M,
//      std::vector<Render_Entity> *accumulator, std::atomic_flag *lock);
//  void visit_root_node_base_index(
//      uint32 node_index, uint32 count, std::vector<Render_Entity> *accumulator, std::atomic_flag *lock);
//};
