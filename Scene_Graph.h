#pragma once
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "SDL_Imgui_State.h"
#include "UI.h"
#include "Serialization_Buffer.h"
#include <array>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <atomic>
#include <glm/glm.hpp>
#include <unordered_map>
enum struct imgui_pane
{
  node_tree,
  resource_man,
  light_array,
  selected_node,
  selected_mes,
  selected_mat,
  particle_emit,
  octree,
  console,
  blank,
  end
};

extern std::vector<Imgui_Texture_Descriptor> IMGUI_TEXTURE_DRAWS;
const uint32 default_assimp_flags = // aiProcess_FlipWindingOrder |
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

struct Plane_nd
{
  vec3 n;
  float32 d;
};

// ccw
Plane_nd compute_plane(vec3 a, vec3 b, vec3 c);

float32 TriArea2D(float32 x1, float32 y1, float32 x2, float32 y2, float32 x3, float32 y3);

// compute barycentric coordinates uvw for
// point p in triangle abc
vec3 barycentric(vec3 a, vec3 b, vec3 c, vec3 p);

struct Octree;
struct Octree_Node;

bool point_within_square(vec3 p, vec3 ps, float32 size);

#ifdef NDEBUG
#define MAX_OCTREE_DEPTH 10
#else
#define MAX_OCTREE_DEPTH 4
#endif
#define TRIANGLES_PER_NODE 4096

// if split style is on, then triangles are stored in each max-depth node that they touch only
// if the nodes are too small relative to the triangles, then there is excess storage required
// if split style is off, then triangles are only stored in the node that entirely contains them
#define OCTREE_SPLIT_STYLE

#define OCTREE_VECTOR_STYLE

////the d00 d01 d11 terms can be computed once for multiple points p since they dont depend on p
// vec3 barycentric(vec3 a, vec3 b, vec3 c, vec3 p)
//{
//  vec3 v0 = b - a, v1 = c - a, v2 = p - a;
//  float32 d00 = dot(v0, v0);
//  float32 d01 = dot(v0, v1);
//  float32 d11 = dot(v1, v1);
//  float32 d20 = dot(v2, v0);
//  float32 d21 = dot(v2, v1);
//  float32 denom = d00 * d11 - d01 * d01;
//  vec3 result;
//  result.y = (d11 * d20 - d01 * d21) / denom; //v
//  result.z = (d00 * d21 - d01 * d20) / denom; //w
//  result.x = 1.0f - result.x - result.y; //u
//  return result;
//}

struct AABB
{
  AABB(vec3 center)
  {
    min = center;
    max = center;
  }
  vec3 min;
  vec3 max;
};

bool aabb_intersection(const vec3 &mina, const vec3 &maxa, const vec3 &minb, const vec3 &maxb);
bool aabb_intersection(const AABB &a, const AABB &b);

bool aabb_plane_intersection(const AABB &b, const vec3 &n, float32 d);

void push_aabb(AABB &aabb, const vec3 &p);

bool aabb_triangle_intersection(const AABB &aabb, const Triangle_Normal &triangle);

AABB aabb_from_octree_child_index(uint8 i, vec3 minimum, float32 halfsize, float32 size);
enum Octree_Child_Index
{
  forwardupleft,
  forwardupright,
  forwarddownright,
  forwarddownleft,
  backupleft,
  backupright,
  backdownright,
  backdownleft
};

struct Octree_Node
{
  bool insert_triangle(const Triangle_Normal &tri) noexcept;
  bool push(const Triangle_Normal &triangle, uint8 depth, Octree *owner) noexcept;
  const Triangle_Normal *test_this(const AABB &probe, uint32 *counter, std::vector<Triangle_Normal> *accumulator) const;
  const Triangle_Normal *test(const AABB &probe, uint8 depth, uint32 *counter) const;
  void testall(const AABB &probe, uint8 depth, uint32 *counter, std::vector<Triangle_Normal> *accumulator) const;
  void clear();

  std::array<Octree_Node *, 8> children = {nullptr};
#ifdef OCTREE_VECTOR_STYLE
  std::vector<Triangle_Normal> occupying_triangles;
#else
  std::array<Triangle_Normal, TRIANGLES_PER_NODE> occupying_triangles;
  uint32 free_triangle_index = 0;
#endif

  // std::array<AABB, 256> occupying_aabbs;
  vec3 minimum;
  vec3 center;
  float32 size;
  // float32 halfsize;
  // warning about some reallocating owning object...
  // Octree *owner = nullptr;
  uint8 mydepth;
};

struct Octree
{
  Octree();
  void push(Mesh_Descriptor *mesh, mat4 *transform = nullptr, vec3 *velocity = nullptr);

  const Triangle_Normal *test(const AABB &probe, uint32 *counter) const;
  std::vector<Triangle_Normal> test_all(const AABB &probe, uint32 *counter) const
  {
    std::vector<Triangle_Normal> colliders;
    root->testall(probe, 0, counter, &colliders);
    return colliders;
  }

  void test_all(const AABB &probe, uint32 *counter, std::vector<Triangle_Normal> *acc) const
  {
    root->testall(probe, 0, counter, acc);
  }

  void set_size(float32 size)
  {
    clear();
    root = &nodes[0];
    root->size = size;
    // root->halfsize = 0.5f*size;
    root->minimum = -vec3(0.5f * root->size);
    root->center = root->minimum + vec3(0.5f * root->size);
  }

  Octree_Node *root;
  // std::array<Octree_Node, 65536> nodes;
  std::vector<Octree_Node> nodes; // size is constant, set in constructor
  Octree_Node *new_node(vec3 p, float32 size, uint8 parent_depth) noexcept;
  void clear();
  std::vector<Render_Entity> get_render_entities(Scene_Graph *owner);
  void pack_chosen_entities()
  {
    chosen_render_entities.clear();
    if (draw_nodes)
    {
      chosen_render_entities = cubes;
    }
    if (draw_triangles)
    {
      chosen_render_entities.push_back(triangles);
    }
    if (draw_normals)
    {
      chosen_render_entities.push_back(normals);
    }
    if (draw_velocity)
    {
      chosen_render_entities.push_back(velocities);
    }
  }
  uint32 free_node = 0;
  std::vector<Render_Entity> chosen_render_entities;
  std::vector<Render_Entity> cubes;
  Render_Entity triangles;
  Render_Entity normals;
  Render_Entity velocities;
  int32 depth_to_render = MAX_OCTREE_DEPTH; //-1 for all
  bool update_render_entities = false;
  bool draw_nodes = false;
  bool draw_triangles = false;
  bool draw_normals = false;
  bool draw_velocity = false;
  Mesh_Index mesh_depth_1 = NODE_NULL;
  Mesh_Index mesh_depth_2 = NODE_NULL;
  Mesh_Index mesh_depth_3 = NODE_NULL;
  Mesh_Index mesh_triangles = NODE_NULL;
  Mesh_Index mesh_normals = NODE_NULL;
  Mesh_Index mesh_velocities = NODE_NULL;
  Material_Index mat1 = NODE_NULL;
  Material_Index mat2 = NODE_NULL;
  Material_Index mat3 = NODE_NULL;
  Material_Index mat_triangles = NODE_NULL;
  Material_Index mat_normals = NODE_NULL;
  Material_Index mat_velocities = NODE_NULL;

  uint32 pushed_triangle_count = 0;
  uint32 stored_triangle_count = 0;
};

struct Imported_Scene_Node
{
  Imported_Scene_Node() = default;
  Imported_Scene_Node(const Imported_Scene_Node &rhs) = default;
  Imported_Scene_Node(Imported_Scene_Node &&rhs) = default;
  Imported_Scene_Node &operator=(Imported_Scene_Node &&rhs) = default;
  Imported_Scene_Node &operator=(const Imported_Scene_Node &rhs) = default;

  std::string name;
  std::vector<Imported_Scene_Node> children;
  std::vector<Mesh_Index> mesh_indices;
  std::vector<Material_Index> material_indices;
  glm::mat4 transform;
};

struct Imported_Scene_Data
{
  std::string assimp_filename;
  Imported_Scene_Node root_node;
  std::vector<Mesh_Descriptor> meshes;
  std::vector<Material_Descriptor> materials;
  std::vector<Skeletal_Animation> animations;

  // for all bones in the import
  std::unordered_map<std::string, Bone> all_imported_bones;

  float64 scale_factor = 1.;
  uint32 import_flags = default_assimp_flags;
  std::atomic<bool> valid = false;
  bool import_failure = false;
};

struct Resource_Manager
{
  Material_Index push_material(Material_Descriptor *d);
  Mesh_Index push_mesh(Mesh_Descriptor *d);

  // todo: not the best, refactor these so they construct in place
  uint32 push_bone_set(std::unordered_map<std::string, Bone> *bones);
  uint32 push_animation_set(std::vector<Skeletal_Animation> *animation_set);
  uint32 push_animation_state();

#define MAX_POOL_SIZE 5000
  std::array<Mesh, MAX_POOL_SIZE> mesh_pool;
  uint32 current_mesh_pool_size = 0;

  std::array<Material, MAX_POOL_SIZE> material_pool;
  uint32 current_material_pool_size = 0;

  std::array<Skeletal_Animation_Set, MAX_POOL_SIZE> animation_set_pool;
  uint32 current_animation_set_pool_size = 0;

  std::array<Model_Bone_Set, MAX_POOL_SIZE> model_bone_set_pool;
  uint32 current_model_bone_set_pool_size = 0;

  std::array<Skeletal_Animation_State, MAX_POOL_SIZE> animation_state_pool;
  uint32 current_animation_state_pool_size = 0;

  Material default_material;

  bool thread_active = false;
  std::thread import_thread;

  // opens assimp file using the importer, recursively calls _import_aiscene_node to build the Imported_Scene_Data
  static bool import_aiscene_new(Imported_Scene_Data *result);

  // must be called from main thread
  // returns nullptr if busy loading
  bool import_aiscene_async(Imported_Scene_Data *dst);

  // begins loading of the assimp resource
  // returns pointer to the import if it is ready, else returns null
  // blocks and guarantees a valid scene if wait_for_valid is true
  // if wait_for_valid is false, this must be called repeatedly in order for it to
  // eventually produce the Imported_Scene_Data
  Imported_Scene_Data *request_valid_resource(std::string assimp_path, bool wait_for_valid = true);

  static Imported_Scene_Node _import_aiscene_node(
      std::string assimp_filename, const aiScene *scene, const aiNode *node);

#if 0
  // pure - gets rid of nodes that have no meshes
  void propagate_transformations_of_empty_nodes(Imported_Scene_Node *this_node, std::vector<Imported_Scene_Node> *temp);
#endif

  // this holds the model data in system ram for all imports and isnt cleared
  // if the model data inside were to be cleared, then it would ruin the mesh cache
  // if the entire map entry was erased, it would cause a needless reloading from disk
  // may not be needless if we actually need the ram
  std::unordered_map<std::string, Imported_Scene_Data> import_data;
};

struct Scene_Graph_Node
{
  Scene_Graph_Node();
  // todo: billboarding in visit_nodes
  // todo: frustrum culling
  Array_String name;
  vec3 position = {0, 0, 0};
  quat orientation = glm::angleAxis(0.f, glm::vec3(0, 0, 1));
  vec3 scale = {1, 1, 1};
  vec3 oriented_scale = {1, 1, 1};
  vec3 scale_vertex = {1, 1, 1};
  vec3 velocity = {0, 0, 0};
  // mat4 import_basis = mat4(1);
  std::array<std::pair<Mesh_Index, Material_Index>, MAX_MESHES_PER_NODE> model;

  // pointer for our animation controller - needed for visit nodes
  uint32 animation_state_pool_index = NODE_NULL;

  // the set of bones our import had
  uint32 model_bone_set_pool_index = NODE_NULL;

  std::array<Node_Index, MAX_CHILDREN> children;
  Node_Index parent = NODE_NULL;
  bool exists = false;
  bool visible = true;
  bool propagate_visibility = true;
  bool wait_on_resource = true;
  bool is_a_bone = false;

  // require the mesh and materials to be available immediately
  bool interpolate_this_node_with_last = false; // if set to true, prepare_renderer can interpolate the last two
                                                // positions (when implemented) - set to false if the object is new or
                                                // moved a node index
  Array_String filename_of_import;
};

struct Scene_Graph
{
  Scene_Graph(Resource_Manager *manager);

  void clear();

  // may return NODE_NULL if async loading flag wait_on_resource == false
  // every call after the first of a given path will have its import read from a cache instead of disk
  // and should be somewhat quick but not good for doing over and over in realtime
  // instead, if possible, use deep_clone()
  // Node_Index add_aiscene_old1(
  //    std::string scene_file_path, std::string name = "Unnamed_Node", bool wait_on_resource = true);

  Node_Index add_aiscene_new(
      std::string scene_file_path, std::string name = "Unnamed_Node", bool wait_on_resource = true);

  // todo: collider object grabbyhand thing

  template <typename M> Node_Index add_aiscene(std::string scene_file_path, std::string name, M) = delete;

  // clones the node tree from node into a new node
  // if copy_materials is true, the new tree will have copies of the source nodes' materials
  // instead of references
  Node_Index tree_clone(Node_Index node, bool copy_materials)
  {
    Node_Index result;
    ASSERT(0);
    return result;
  }

  Node_Index add_mesh(std::string name, Mesh_Descriptor *mesh, Material_Descriptor *material);
  Node_Index add_mesh(Mesh_Primitive p, std::string name, Material_Descriptor *material);

  Node_Index new_node();
  Node_Index new_node(std::string name, std::pair<Mesh_Index, Material_Index> model0 = {NODE_NULL, NODE_NULL},
      Node_Index parent = NODE_NULL);

  // todo: accelerate these with spatial partitioning
  Node_Index ray_intersects_node(vec3 p, vec3 dir, Node_Index node, vec3 &result);
  bool ray_intersects_node_aabb(vec3 p, vec3 v, Node_Index node)
  { // todo: implement early-out aabb testing
    return true;
  }

  Node_Index find_by_name(Node_Index parent, const char *name);
  // grab doesnt require any particular parent/child relation before calling, it can even be a child of another parent
  void grab(Node_Index grabber, Node_Index grabee);
  // brings the node to world basis and world parent
  void drop(Node_Index child);

  Material_Descriptor *get_modifiable_material_pointer_for(Node_Index node, Model_Index model);

  // if modify_or_overwrite is true, the import will only change the textures in the given material descriptor that are
  // different than a default constructed material descriptor
  // if false, it will simply use the given material_descriptor for all objects
  void modify_material(Material_Index material_index, Material_Descriptor *material, bool modify_or_overwrite);
  void modify_all_materials(
      Node_Index i, Material_Descriptor *m, bool modify_or_overwrite = true, bool children_too = true);

  void delete_node(Node_Index i);
  void set_parent(Node_Index i, Node_Index desired_parent = NODE_NULL);

  std::vector<Render_Entity> visit_nodes_start();

  // use visit nodes if you need the transforms of all objects in the graph
  glm::mat4 build_transformation(Node_Index node_index, bool include_vertex_scale = true);

  std::string serialize() const;
  void deserialize(std::string src); // instant overwrite of node array

  bool draw_collision_octree = false;
  Octree collision_octree;
  std::array<Scene_Graph_Node, MAX_NODES> nodes;
  Light_Array lights;

  void initialize_lighting(std::string radiance, std::string irradiance, bool generate_light_spheres = true);
  void set_lights_for_renderer(Renderer *r);
  void push_particle_emitters_for_renderer(Renderer *r);

  std::vector<Particle_Emitter> particle_emitters;

  // particle_emitters need to be initialized in the main thread -
  //    they have opengl state so we need yet another descriptor->object interface like the pools

  Resource_Manager *resource_manager;
  Material_Index collider_material_index = NODE_NULL;

  void draw_imgui(std::string name);
  bool file_type = false; // true for radiance, false for irradiance

private:
  uint32 highest_allocated_node = 0;
  std::vector<Render_Entity> accumulator;
  std::vector<World_Object> accumulator1;
  void visit_nodes(
      Node_Index Node_Index, const mat4 &M, const mat4 &INVERSE_ROOT, std::vector<Render_Entity> &accumulator);
  glm::mat4 __build_transformation(Node_Index node_index);
  void assert_valid_parent_ptr(Node_Index child);
  Node_Index add_import_node(Imported_Scene_Data *scene, Imported_Scene_Node *node,
      const std::pair<Mesh_Index, Material_Index> &base_indices, uint32 bone_pool_index,
      uint32 animation_state_pool_index);

  // imgui:
  Node_Index imgui_selected_node = NODE_NULL;
  Mesh_Index imgui_selected_mesh = NODE_NULL;
  Material_Index imgui_selected_material = NODE_NULL;
  Array_String imgui_selected_import_filename;

  File_Picker texture_picker = File_Picker(".");
  uint32 texture_picking_slot = 0;

  bool imgui_open = true;
  bool command_window_open = false;
  bool showing_model = false;
  bool showing_mesh_data = false;

  std::vector<std::vector<imgui_pane>> imgui_rows = {
      {imgui_pane::node_tree, imgui_pane::selected_node}, {imgui_pane::selected_mat, imgui_pane::light_array}};
  uint32 imgui_rows_count = 1;
  uint32 imgui_col_count = 1;
  // imgui_pane imgui_panes[6] = {node_tree, resource_man, light_array, selected_node, selected_mat, blank};

  void draw_imgui_specific_mesh(Mesh_Index mesh_index);
  void draw_imgui_specific_material(Material_Index material_index);
  void draw_imgui_light_array();
  void draw_imgui_command_interface();
  void draw_imgui_specific_node(Node_Index node_index);
  void draw_imgui_tree_node(Node_Index node_index);
  bool draw_imgui_texture_element(const char *name, Texture *ptr, uint32 slot);
  void draw_imgui_const_texture_element(const char *name, Texture_Descriptor *ptr);
  void draw_imgui_resource_manager();
  void draw_imgui_particle_emitter();
  void draw_imgui_octree();
  void draw_imgui_console(ImVec2 section_size);
  void draw_active_nodes();
  void draw_imgui_pane_selection_button(imgui_pane *modifying);
  void draw_imgui_selected_pane(imgui_pane p);
  void handle_console_command(std::string_view cmd);
};
