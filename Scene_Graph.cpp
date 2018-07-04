#include "Scene_Graph.h"
#include "Globals.h"
#include "Render.h"
#include <array>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <atomic>
#include <nlohmann/json.hpp>
#include <thread>
using json = nlohmann::json;
using namespace std;

uint32 new_ID()
{
  static uint32 last = 0;
  return last += 1;
}

glm::mat4 copy(aiMatrix4x4 m)
{
  // assimp is row-major
  // glm is column-major
  glm::mat4 result;
  for (uint32 i = 0; i < 4; ++i)
  {
    for (uint32 j = 0; j < 4; ++j)
    {
      result[i][j] = m[j][i];
    }
  }
  return result;
}

Scene_Graph_Node::Scene_Graph_Node(string name, const aiNode *node, const mat4 *import_basis_, const aiScene *scene,
    string scene_file_path, Uint32 *mesh_num, Material_Descriptor *material_override)
{
  ASSERT(node);
  ASSERT(scene);
  this->name = name;
  basis = copy(node->mTransformation);
  if (import_basis_)
    import_basis = *import_basis_;

  for (uint32 i = 0; i < node->mNumMeshes; ++i)
  {
    auto ai_i = node->mMeshes[i];
    const aiMesh *aimesh = scene->mMeshes[ai_i];
    (*mesh_num) += 1;
    string unique_id = scene_file_path + to_string(*mesh_num);
    Mesh mesh(aimesh, name, unique_id, scene);
    aiMaterial *ptr = scene->mMaterials[aimesh->mMaterialIndex];
    Material material(ptr, scene_file_path, material_override);
    model.push_back({mesh, material});
  }
  owned_children.reserve(node->mNumChildren);
}

Scene_Graph::Scene_Graph()
{
  Node_Ptr ptr = make_shared<Scene_Graph_Node>();
  ptr->name = "SCENE_GRAPH_ROOT";
  root = ptr;
}

void Scene_Graph::add_graph_node(const aiNode *node, weak_ptr<Scene_Graph_Node> parent, const mat4 *import_basis,
    const aiScene *aiscene, string scene_file_path, Uint32 *mesh_num, Material_Descriptor *material_override)
{
  ASSERT(node);
  ASSERT(aiscene);
  string name = copy(&node->mName);
  //// construct just this node in the main node array
  // auto thing = Scene_Graph_Node(name, node, import_basis, aiscene,
  //    scene_file_path, mesh_num, material_override);

  shared_ptr<Scene_Graph_Node> new_node =
      make_shared<Scene_Graph_Node>(name, node, import_basis, aiscene, scene_file_path, mesh_num, material_override);

  set_parent(new_node, parent, true);

  // construct all the new node's children, because its constructor doesn't
  for (uint32 i = 0; i < node->mNumChildren; ++i)
  {
    const aiNode *child = node->mChildren[i];
    add_graph_node(child, new_node, import_basis, aiscene, scene_file_path, mesh_num, material_override);
  }
}

shared_ptr<Scene_Graph_Node> Scene_Graph::add_mesh(
    Mesh_Data m, Material_Descriptor md, string name, const mat4 *import_basis)
{
  Node_Ptr new_node = make_shared<Scene_Graph_Node>();

  new_node->name = name;
  if (import_basis)
    new_node->import_basis = *import_basis;

  Mesh mesh(m, name);
  Material material(md);
  new_node->model.push_back({mesh, material});
  set_parent(new_node, root, false);
  return new_node;
}
void Scene_Graph::set_parent(weak_ptr<Scene_Graph_Node> p, weak_ptr<Scene_Graph_Node> desired_parent, bool parent_owned)
{
  shared_ptr<Scene_Graph_Node> child = p.lock();
  shared_ptr<Scene_Graph_Node> current_parent_ptr = child->parent.lock();

  if (current_parent_ptr)
  {
    for (auto i = current_parent_ptr->owned_children.begin(); i != current_parent_ptr->owned_children.end(); ++i)
    {
      if (*i == p.lock())
      {
        current_parent_ptr->owned_children.erase(i);
        break;
      }
    }
    for (auto i = current_parent_ptr->unowned_children.begin(); i != current_parent_ptr->unowned_children.end(); ++i)
    {
      if (i->lock() == p.lock())
      {
        current_parent_ptr->unowned_children.erase(i);
        break;
      }
    }
  }

  shared_ptr<Scene_Graph_Node> desired_parent_ptr = desired_parent.lock();
  if (desired_parent_ptr)
  {
    child->parent = desired_parent;
    if (parent_owned)
    {
      desired_parent_ptr->owned_children.push_back(Node_Ptr(p));
    }
    else
    {
      desired_parent_ptr->unowned_children.push_back(Node_Ptr(p));
    }
  }
  else
  {
    set_message("ERROR: set_parent parent doesnt exist.");
    ASSERT(0);
  }
}

shared_ptr<Scene_Graph_Node> Scene_Graph::add_aiscene(
    string scene_file_path, const mat4 *import_basis, Material_Descriptor *material_override)
{
  return add_aiscene(load_aiscene(scene_file_path), scene_file_path, import_basis, material_override);
}

shared_ptr<Scene_Graph_Node> Scene_Graph::add_aiscene(string scene_file_path, Material_Descriptor *material_override)
{
  return add_aiscene(load_aiscene(scene_file_path), scene_file_path, nullptr, material_override);
}

shared_ptr<Scene_Graph_Node> Scene_Graph::add_aiscene(
    const aiScene *scene, string scene_file_path, const mat4 *import_basis, Material_Descriptor *material_override)
{

  // accumulates as meshes are imported, used along with the scene file path
  // to create a unique_id for the mesh
  Uint32 mesh_num = 0;
  const aiNode *root = scene->mRootNode;

  // create the root node for this scene
  string name = copy(&root->mName);
  shared_ptr<Scene_Graph_Node> new_node = make_shared<Scene_Graph_Node>(
      name, root, import_basis, scene, BASE_MODEL_PATH + scene_file_path, &mesh_num, material_override);
  set_parent(new_node, this->root, false);
  new_node->filename_of_import = scene_file_path;
  new_node->is_root_of_import = true;

  // add every aiscene child to the new node
  const uint32 num_children = scene->mRootNode->mNumChildren;
  for (uint32 i = 0; i < num_children; ++i)
  {
    const aiNode *node = scene->mRootNode->mChildren[i];
    add_graph_node(
        node, new_node, import_basis, scene, BASE_MODEL_PATH + scene_file_path, &mesh_num, material_override);
  }
  return new_node;
}
void Scene_Graph::visit_nodes(
    const weak_ptr<Scene_Graph_Node> node_ptr, const mat4 &M, vector<Render_Entity> &accumulator)
{
  shared_ptr<Scene_Graph_Node> entity = node_ptr.lock();
  ASSERT(entity);
  if ((!entity->visible) && entity->propagate_visibility)
    return;

  const mat4 T = translate(entity->position);
  const mat4 S = scale(entity->scale);
  const mat4 R = toMat4(entity->orientation);
  const mat4 B = entity->basis;
  const mat4 I = entity->import_basis;

  mat4 STACK = mat4(1);

  if (entity->propagate_scale)
  {
    STACK = S;
  }
  if (entity->propagate_rotation)
  {
    STACK = R * STACK;
  }
  if (entity->propagate_translation)
  {
    STACK = T * STACK;
  }
  STACK = M * B * STACK;

  const mat4 final_transformation = M * B * T * R * S * I;

  const uint32 num_meshes = entity->model.size();
  for (uint32 i = 0; i < num_meshes; ++i)
  {
    Mesh *mesh_ptr = &entity->model[i].first;
    Material *material_ptr = &entity->model[i].second;

    if (entity->visible)
      accumulator.emplace_back(mesh_ptr, material_ptr, final_transformation);
  }
  for (auto i = entity->unowned_children.begin(); i != entity->unowned_children.end();)
  {
    weak_ptr<Scene_Graph_Node> child = *i;
    if (!child.lock())
    {
      i = entity->unowned_children.erase(i);
    }
    else
    {
      visit_nodes(child, STACK, accumulator);
      ++i;
    }
  }
  for (auto i = entity->owned_children.begin(); i != entity->owned_children.end(); ++i)
  {
    weak_ptr<Scene_Graph_Node> child = *i;
    visit_nodes(child, STACK, accumulator);
  }
}
void Scene_Graph::visit_nodes_locked_accumulator(
    weak_ptr<Scene_Graph_Node> node_ptr, const mat4 &M, vector<Render_Entity> *accumulator, atomic_flag *lock)
{
  shared_ptr<Scene_Graph_Node> entity = node_ptr.lock();
  if ((!entity->visible) && entity->propagate_visibility)
    return;
  const mat4 T = translate(entity->position);
  const mat4 S = scale(entity->scale);
  const mat4 R = toMat4(entity->orientation);
  const mat4 B = entity->basis;
  const mat4 STACK = M * B * T * R * S;
  const mat4 I = entity->import_basis;
  const mat4 FINAL = STACK * I;

  const int num_meshes = entity->model.size();
  for (int i = 0; i < num_meshes; ++i)
  {
    Mesh *mesh_ptr = &entity->model[i].first;
    Material *material_ptr = &entity->model[i].second;
    while (lock->test_and_set())
    { /*spin*/
    }
    if (entity->visible)
      accumulator->emplace_back(mesh_ptr, material_ptr, FINAL);

    lock->clear();
  }

  for (auto i = entity->unowned_children.begin(); i != entity->unowned_children.end();)
  {
    weak_ptr<Scene_Graph_Node> child = *i;
    if (!child.lock())
    {
      i = entity->unowned_children.erase(i);
    }
    else
    {
      visit_nodes_locked_accumulator(child, STACK, accumulator, lock);
      ++i;
    }
  }
  for (auto i = entity->owned_children.begin(); i != entity->owned_children.end(); ++i)
  {
    weak_ptr<Scene_Graph_Node> child = *i;
    visit_nodes_locked_accumulator(child, STACK, accumulator, lock);
  }
}

void Scene_Graph::visit_root_node_base_index(
    uint32 node_index, uint32 count, vector<Render_Entity> *accumulator, atomic_flag *lock)
{
  const Scene_Graph_Node *const entity = root.get();
  ASSERT(entity->name == "SCENE_GRAPH_ROOT");
  ASSERT(entity->position == vec3(0));
  ASSERT(entity->orientation == quat());
  ASSERT(entity->import_basis == mat4(1));
  ASSERT(entity->unowned_children.size() == 0);
  for (uint32 i = node_index; i < node_index + count; ++i)
  {
    weak_ptr<Scene_Graph_Node> child = entity->owned_children[i];
    visit_nodes_locked_accumulator(child, mat4(1), accumulator, lock);
  }
}

vector<Render_Entity> Scene_Graph::visit_nodes_async_start()
{
  vector<Render_Entity> accumulator;
  accumulator.reserve(uint32(last_accumulator_size * 1.5));

  const uint32 root_children_count = root->owned_children.size();
  const uint32 thread_count = 4;
  const uint32 nodes_per_thread = root_children_count / thread_count;
  const uint32 leftover_nodes = root_children_count % thread_count;

  PERF_TIMER.start();
  // todo the refactoring into class member function(or regression to W7 from
  // W10)
  // or other change killed the performance here
  thread threads[thread_count];
  atomic_flag lock = ATOMIC_FLAG_INIT;
  for (uint32 i = 0; i < thread_count; ++i)
  {
    const uint32 base_index = i * nodes_per_thread;
    auto fp = [this, base_index, nodes_per_thread, &accumulator, &lock] {
      this->visit_root_node_base_index(base_index, nodes_per_thread, &accumulator, &lock);
    };
    threads[i] = thread(fp);
  }
  PERF_TIMER.stop();
  uint32 leftover_index = (thread_count * nodes_per_thread);
  ASSERT(leftover_index + leftover_nodes == root_children_count);
  visit_root_node_base_index(leftover_index, leftover_nodes, &accumulator, &lock);
  for (int i = 0; i < thread_count; ++i)
  {
    threads[i].join();
  }

  last_accumulator_size = accumulator.size();
  return accumulator;
}
vector<Render_Entity> Scene_Graph::visit_nodes_st_start()
{
  vector<Render_Entity> accumulator;
  accumulator.reserve(uint32(last_accumulator_size * 1.5));
  visit_nodes(root, mat4(1), accumulator);
  last_accumulator_size = accumulator.size();
  return accumulator;
}

shared_ptr<Scene_Graph_Node> Scene_Graph::add_primitive_mesh(
    Mesh_Primitive p, string name, Material_Descriptor m, const mat4 *import_basis)
{
  shared_ptr<Scene_Graph_Node> new_node = make_shared<Scene_Graph_Node>();
  new_node->name = name;
  if (import_basis)
    new_node->import_basis = *import_basis;
  Mesh mesh(p, name);
  Material material(m);
  new_node->model.push_back({mesh, material});
  set_parent(new_node, root, false);
  return new_node;
}
