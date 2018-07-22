#include "stdafx.h"
#include "Scene_Graph.h"
#include "Globals.h"
#include "Render.h"
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

Node_Index Flat_Scene_Graph::add_mesh(std::string name, Mesh_Descriptor *d, Material_Descriptor *md)
{
  Mesh_Index mesh_index = resource_manager->push_custom_mesh(d);
  Material_Index material_index = resource_manager->push_custom_material(md);

  Node_Index node_index = new_node();
  Flat_Scene_Graph_Node *node = &nodes[node_index];
  node->model[0] = {mesh_index, material_index};
  node->name = name;
  return node_index;
}

Node_Index Flat_Scene_Graph::add_mesh(Mesh_Primitive p, std::string name, Material_Descriptor *md)
{
  Mesh_Descriptor d;
  d.name = s(p);
  d.primitive = p;
  return add_mesh(name, &d, md);
}

Material_Index Flat_Scene_Graph::copy_material_to_new_pool_slot(
    Node_Index i, Model_Index model_index, Material_Descriptor *m, bool modify_or_overwrite)
{
  Flat_Scene_Graph_Node *node = &nodes[i];
  ASSERT(model_index < MAX_MESHES_PER_NODE);
  if (node->model[model_index].second.i == NODE_NULL)
  {
    ASSERT(0);
  }

  Material_Index current_index = node->model[model_index].second;
  Material_Index new_index;
  if (current_index.use_material_index_for_material_pool)
  {
    // if its already in the pool, we need to copy because pushing can invalidate the resource
    Material_Descriptor valid = *resource_manager->retrieve_pool_material_descriptor(current_index);
    new_index = resource_manager->push_custom_material(&valid);
  }
  else
  {
    Material_Descriptor *current =
        resource_manager->retrieve_assimp_material_descriptor(s(node->filename_of_import), current_index);
    new_index = resource_manager->push_custom_material(current);
  }

  Material_Descriptor *new_descriptor = resource_manager->retrieve_pool_material_descriptor(new_index);

  if (m)
  {
    modify_material(new_index, m, modify_or_overwrite);
  }
  resource_manager->update_material_pool_index(new_index);
  node->model[model_index].second = new_index;
  return new_index;
}

void Flat_Scene_Graph::modify_material(
    Material_Index material_index, Material_Descriptor *material, bool modify_or_overwrite)
{
  ASSERT(material_index.use_material_index_for_material_pool); // modify_material cannot be used on an assimp import
                                                               // index - copy to pool first

  Material_Descriptor *current_descriptor = resource_manager->retrieve_pool_material_descriptor(material_index);
  ASSERT(current_descriptor);
  if (material)
  {
    if (modify_or_overwrite)
      current_descriptor->mod_by(material);
    else
      *current_descriptor = *material;
  }
  else
  {
    *current_descriptor = Material_Descriptor();
  }
  resource_manager->update_material_pool_index(material_index);
}

void Flat_Scene_Graph::modify_all_materials(
    Node_Index node_index, Material_Descriptor *m, bool modify_or_overwrite, bool children_too)
{
  if (node_index == NODE_NULL)
    return;
  Flat_Scene_Graph_Node *node = &nodes[node_index];

  for (uint32 i = 0; i < node->model.size(); ++i)
  {
    Material_Index material_index = node->model[i].second;
    modify_material(material_index, m, modify_or_overwrite);
  }
  if (children_too)
  {
    for (uint32 i = 0; i < node->children.size(); ++i)
    {
      Node_Index child_index = node->children[i];
      modify_all_materials(child_index, m, modify_or_overwrite, true);
    }
  }
}

// uses assimp's defined material if assimp import, else a default-constructed material

void Flat_Scene_Graph::reset_material(Node_Index node_index, Model_Index model_index)
{
  if (node_index == NODE_NULL)
    return;
  ASSERT(model_index < MAX_MESHES_PER_NODE);
  Flat_Scene_Graph_Node *node = &nodes[node_index];

  if (node->model[model_index].second.assimp_original != NODE_NULL)
  {
    node->model[model_index].second.i = node->model[model_index].second.assimp_original;
    node->model[model_index].second.use_material_index_for_material_pool = false;
    return;
  }
  else
  {
    node->model[model_index].second.i = NODE_NULL;
    node->model[model_index].second.use_material_index_for_material_pool = true;
  }
}

void Flat_Scene_Graph::reset_all_materials(Node_Index node_index, bool children_too)
{
  Flat_Scene_Graph_Node *node = &nodes[node_index];
  for (uint32 i = 0; i < node->model.size(); ++i)
  {
    reset_material(node_index, i);
  }

  if (children_too)
  {
    for (uint32 i = 0; i < node->children.size(); ++i)
    {
      reset_all_materials(node->children[i], true);
    }
  }
}

void Flat_Scene_Graph::copy_all_materials_to_new_pool_slots(
    Node_Index i, Material_Descriptor *material, bool modify_or_overwrite)
{
  if (i == NODE_NULL)
    return;
  Flat_Scene_Graph_Node *node = &nodes[i];

  for (uint32 c = 0; c < node->model.size(); ++c)
  {
    if (node->model[c].second.i == NODE_NULL)
      continue;
    copy_material_to_new_pool_slot(i, c, material, modify_or_overwrite);
  }
  for (uint32 c = 0; c < node->children.size(); ++c)
  {
    copy_all_materials_to_new_pool_slots(node->children[c], material, modify_or_overwrite);
  }
}

void Flat_Scene_Graph::clear()
{
  for (uint32 i = 0; i < nodes.size(); ++i)
  {
    nodes[i] = Flat_Scene_Graph_Node();
  }
  lights = Light_Array();
}

Node_Index Flat_Scene_Graph::add_aiscene(std::string name, std::string scene_file_path, bool wait_on_resource)
{
  scene_file_path = BASE_MODEL_PATH + scene_file_path;
  Imported_Scene_Data *resource = resource_manager->request_valid_resource(scene_file_path, wait_on_resource);

  if (!resource)
  {
    ASSERT(!wait_on_resource);
    return NODE_NULL;
  }

  Node_Index root_for_import = new_node();
  Flat_Scene_Graph_Node *root_node = &nodes[root_for_import];
  root_node->filename_of_import = scene_file_path;
  root_node->name = name;

  const uint32 number_of_children = resource->children.size();
  for (uint32 i = 0; i < number_of_children; ++i)
  {
    Node_Index child_index = add_import_node(&resource->children[i], root_for_import, scene_file_path);
  }
  return root_for_import;
}

Node_Index Flat_Scene_Graph::add_aiscene(std::string name, std::string scene_file_path,
    Material_Descriptor *material_override, bool modify_or_overwrite, bool wait_on_resource)
{
  scene_file_path = BASE_MODEL_PATH + scene_file_path;
  Imported_Scene_Data *resource = resource_manager->request_valid_resource(scene_file_path, wait_on_resource);

  if (!resource)
  {
    ASSERT(!wait_on_resource);
    return NODE_NULL;
  }

  Node_Index root_for_import = new_node();
  Flat_Scene_Graph_Node *root_node = &nodes[root_for_import];
  root_node->filename_of_import = scene_file_path;
  root_node->name = name;

  const uint32 number_of_children = resource->children.size();
  for (uint32 i = 0; i < number_of_children; ++i)
  {
    Node_Index child_index = add_import_node(
        &resource->children[i], root_for_import, scene_file_path, material_override, modify_or_overwrite);
  }

  copy_all_materials_to_new_pool_slots(root_for_import, material_override, modify_or_overwrite);
  return root_for_import;
}

Node_Index Flat_Scene_Graph::new_node()
{
  for (uint32 i = 0; i < nodes.size(); ++i)
  {
    if (nodes[i].exists == false)
    {
      nodes[i] = Flat_Scene_Graph_Node();
      nodes[i].exists = true;
      set_message("Scene_Graph allocating new node:", s(i), 1.0f);
      return i;
    }
  }
  ASSERT(0); // graph full
  return NODE_NULL;
}
void Flat_Scene_Graph::grab(Node_Index grabber, Node_Index grabee)
{
  if (nodes[grabee].parent != NODE_NULL)
    drop(grabee);
  mat4 M = build_transformation(grabber, false);
  mat4 IM = inverse(M);
  mat4 Mchild = build_transformation(grabee, false);
  mat4 M_to_Mchild = IM * Mchild;
  vec3 scale;
  quat orientation;
  vec3 translation;
  decompose(M_to_Mchild, scale, orientation, translation, vec3(), vec4());
  orientation = conjugate(orientation);
  set_parent(grabee, grabber);
  nodes[grabee].position = translation;
  nodes[grabee].scale = scale;
  nodes[grabee].orientation = orientation;
  nodes[grabee].import_basis = mat4(1);
}
void Flat_Scene_Graph::drop(Node_Index child)
{
  mat4 M = build_transformation(child);
  vec3 scale;
  quat orientation;
  vec3 translation;
  decompose(M, scale, orientation, translation, vec3(), vec4());
  orientation = conjugate(orientation);

  set_parent(child);
  nodes[child].position = translation;
  nodes[child].scale = scale;
  nodes[child].orientation = orientation;
  nodes[child].import_basis = mat4(1);
}
Node_Index Flat_Scene_Graph::add_import_node(
    Imported_Scene_Node *import_node, Node_Index parent, std::string assimp_filename)
{
  Node_Index node_index = new_node();
  Flat_Scene_Graph_Node *node = &nodes[node_index];
  node->import_basis = import_node->transform;

  node->filename_of_import = assimp_filename;
  node->name = import_node->name;
  set_parent(node_index, parent);

  uint32 number_of_mesh_indices = import_node->mesh_indices.size();
  uint32 number_of_material_indices = import_node->material_indices.size();
  ASSERT(number_of_mesh_indices == number_of_material_indices);
  for (uint32 i = 0; i < number_of_mesh_indices; ++i)
  {
    node->model[i] = {import_node->mesh_indices[i], import_node->material_indices[i]};
  }

  const uint32 number_of_children = import_node->children.size();
  for (uint32 i = 0; i < number_of_children; ++i)
  {
    Node_Index child_index = add_import_node(&import_node->children[i], node_index, assimp_filename);
  }
  return node_index;
}

Node_Index Flat_Scene_Graph::add_import_node(Imported_Scene_Node *import_node, Node_Index parent,
    std::string assimp_filename, Material_Descriptor *material_override, bool modify_or_overwrite)
{
  Node_Index node_index = new_node();
  Flat_Scene_Graph_Node *node = &nodes[node_index];
  node->import_basis = import_node->transform;

  node->filename_of_import = assimp_filename;
  node->name = import_node->name;
  set_parent(node_index, parent);

  uint32 number_of_mesh_indices = import_node->mesh_indices.size();
  uint32 number_of_material_indices = import_node->material_indices.size();
  ASSERT(number_of_mesh_indices == number_of_material_indices);
  for (uint32 i = 0; i < number_of_mesh_indices; ++i)
  {
    node->model[i] = {import_node->mesh_indices[i], import_node->material_indices[i]};
  }

  const uint32 number_of_children = import_node->children.size();
  for (uint32 i = 0; i < number_of_children; ++i)
  {
    Node_Index child_index = add_import_node(&import_node->children[i], node_index, assimp_filename);
  }
  return node_index;
}

// if you made a change to a Material_Descriptor* you got from a Material_Index
// you will need to call this after making changes to it

void Flat_Scene_Graph::push_material_change(Material_Index i)
{
  ASSERT(i.use_material_index_for_material_pool); // only pool indices are modifiable
  resource_manager->update_material_pool_index(i);
}

void Flat_Scene_Graph::delete_node(Node_Index i)
{
  if (i == NODE_NULL)
    return;
  nodes[i] = Flat_Scene_Graph_Node();
  for (Node_Index &child : nodes[i].children)
  {
    delete_node(child);
  }
}

void Flat_Scene_Graph::set_parent(Node_Index i, Node_Index desired_parent)
{
  ASSERT(i != NODE_NULL);
  Flat_Scene_Graph_Node *node = &nodes[i];
  Node_Index current_parent_index = node->parent;
  if (current_parent_index != NODE_NULL)
  {
    Flat_Scene_Graph_Node *current_parent = &nodes[current_parent_index];
    for (auto &child : current_parent->children)
    {
      if (child == i)
      {
        child = NODE_NULL;
        break;
      }
    }
  }
  if (desired_parent != NODE_NULL)
  {
    bool found_new_slot = false;
    Flat_Scene_Graph_Node *new_parent = &nodes[desired_parent];
    for (auto &child : new_parent->children)
    {
      if (child == NODE_NULL)
      {
        found_new_slot = true;
        child = i;
        break;
      }
    }
    if (!found_new_slot)
    {
      ASSERT(0); // out of children
    }
  }

  set_message(s("Setting parent:child:", i, "to parent:", desired_parent), "", 30.f);
  node->parent = desired_parent;
}

std::vector<Render_Entity> Flat_Scene_Graph::visit_nodes_client_start()
{
  accumulator.clear();
  for (uint32 i = 0; i < nodes.size(); ++i)
  {
    Flat_Scene_Graph_Node *node = &nodes[i];
    if (!node->exists)
      continue;
    if (node->parent == NODE_NULL)
    { // node is visible and its parent is the null node, add its branch
      visit_nodes(i, mat4(1), accumulator);
    }
  }
  return accumulator;
}
std::vector<World_Object> Flat_Scene_Graph::visit_nodes_server_start()
{
  accumulator1.clear();
  for (uint32 i = 0; i < nodes.size(); ++i)
  {
    Flat_Scene_Graph_Node *node = &nodes[i];
    if (!node->exists)
      continue;
    if (node->parent == NODE_NULL) // there isnt an index for the 'root' node
    {
      visit_nodes(i, mat4(1), accumulator1);
    }
  }
  return accumulator1;
}

glm::mat4 Flat_Scene_Graph::__build_transformation(Node_Index node_index)
{
  if (node_index == NODE_NULL)
    return mat4(1);

  Flat_Scene_Graph_Node *node = &nodes[node_index];
  Node_Index parent = node->parent;

  mat4 M = __build_transformation(parent);
  const mat4 T = translate(node->position);
  const mat4 S_prop = scale(node->scale);
  const mat4 S_non = scale(node->scale_vertex);
  const mat4 R = toMat4(node->orientation);
  const mat4 B = node->import_basis;
  const mat4 STACK = M * T * R * S_prop * B;
  return STACK;
}

// use visit nodes if you need the transforms of all objects in the graph
glm::mat4 Flat_Scene_Graph::build_transformation(Node_Index node_index, bool use_vertex_scale)
{
  ASSERT(node_index != NODE_NULL);

  Flat_Scene_Graph_Node *node = &nodes[node_index];
  Node_Index parent = node->parent;

  mat4 M = __build_transformation(parent);
  const mat4 T = translate(node->position);
  const mat4 S_prop = scale(node->scale);
  const mat4 S_non = scale(node->scale_vertex);
  const mat4 R = toMat4(node->orientation);
  const mat4 B = node->import_basis;
  // this node specifically
  mat4 BASIS = M * T * R * S_prop * B;
  if (use_vertex_scale)
  {
    BASIS = BASIS * S_non;
  }
  return BASIS;
}
std::string Flat_Scene_Graph::serialize() const
{
  std::string result;
  const uint32 bytes = sizeof(nodes);
  result.resize(bytes);
  memcpy(&result[0], &nodes[0], bytes);
  return result;
}
void Flat_Scene_Graph::deserialize(std::string src)
{
  const uint32 bytes = sizeof(nodes);
  ASSERT(src.size() == bytes);
  memcpy(&nodes[0], &src[0], bytes);
}

void Flat_Scene_Graph::visit_nodes(Node_Index node_index, const mat4 &M, std::vector<Render_Entity> &accumulator)
{
  if (node_index == NODE_NULL)
    return;
  Flat_Scene_Graph_Node *entity = &nodes[node_index];
  if (!entity->exists)
    return;
  if ((!entity->visible) && entity->propagate_visibility)
    return;
  assert_valid_parent_ptr(node_index);

  const mat4 T = translate(entity->position);
  const mat4 S_prop = scale(entity->scale);
  const mat4 S_non = scale(entity->scale_vertex);
  const mat4 R = toMat4(entity->orientation);
  const mat4 B = entity->import_basis;

  // what the nodes below inherit
  const mat4 STACK = M * T * R * S_prop * B;

  // this node specifically
  mat4 BASIS = M * T * R * S_prop * S_non * B;

  // mat4 STACK = mat4(1);

  // if (entity->propagate_scale)
  //  STACK = Spre;
  // if (entity->propagate_rotation)
  //  STACK = R * STACK;
  // if (entity->propagate_scale)
  //  STACK = Spost;
  // if (entity->propagate_translation)
  //  STACK = T * STACK;
  // STACK = M * STACK;

  const uint32 num_meshes = entity->model.size();
  for (uint32 i = 0; i < num_meshes; ++i)
  {
    Mesh_Index mesh_index = entity->model[i].first;
    Material_Index material_index = entity->model[i].second;
    if (mesh_index.i == NODE_NULL)
      continue;
    Mesh *mesh_ptr = nullptr;
    Material *material_ptr = nullptr;
    SCRATCH_STRING = s(entity->filename_of_import);
    string &assimp_filename = SCRATCH_STRING;

    // did you mean to call visit_nodes_server()?
    ASSERT(!resource_manager->opengl_disabled);
    // mesh:
    if (mesh_index.use_mesh_index_for_mesh_pool)
    { // all non-assimp meshes
      mesh_ptr = resource_manager->retrieve_pool_mesh(mesh_index);
    }
    else
    { // assimp defined
      resource_manager->request_valid_resource(assimp_filename);
      mesh_ptr = resource_manager->retrieve_assimp_mesh_resource(assimp_filename, mesh_index);
    }

    // material:
    if (material_index.use_material_index_for_material_pool)
    { // all non-assimp meshes
      material_ptr = resource_manager->retrieve_pool_material(material_index);
    }
    else
    { // assimp defined
      resource_manager->request_valid_resource(assimp_filename);
      material_ptr = resource_manager->retrieve_assimp_material_resource(assimp_filename, material_index);
    }
    if (entity->visible)
      accumulator.emplace_back(entity->name, mesh_ptr, material_ptr, BASIS);
  }
  for (uint32 i = 0; i < entity->children.size(); ++i)
  {
    Node_Index child = entity->children[i];
    visit_nodes(child, STACK, accumulator);
  }
}

void Flat_Scene_Graph::assert_valid_parent_ptr(Node_Index node_index)
{
  Flat_Scene_Graph_Node *entity = &nodes[node_index];
  if (entity->parent == NODE_NULL)
    return;
  Flat_Scene_Graph_Node *parent = &nodes[entity->parent];
  for (uint32 i = 0; i < parent->children.size(); ++i)
  {
    if (parent->children[i] == node_index)
    {
      return;
    }
  }
  ASSERT(0); // invalid parent pointer - you changed this parents children but not the children's parent
             // ptr - call delete_node() to recursively delete
}

void Flat_Scene_Graph::visit_nodes(Node_Index node_index, const mat4 &M, std::vector<World_Object> &accumulator)
{
  if (node_index == NODE_NULL)
    return;
  Flat_Scene_Graph_Node *entity = &nodes[node_index];
  if (!entity->exists)
    return;

  assert_valid_parent_ptr(node_index);

  // const mat4 T = translate(entity->position);
  // const mat4 Spre = scale(entity->scale);
  // const mat4 Spost = scale(entity->scale_vertex);
  // const mat4 R = toMat4(entity->orientation);
  const mat4 B = entity->import_basis;

  // const mat4 WORLD_SCALE = SCALE_STACK * Spre;
  // const mat4 BASIS = M * T * R * Spre * B;

  const mat4 T = translate(entity->position);
  const mat4 S = scale(entity->scale);
  const mat4 R = toMat4(entity->orientation);
  // const mat4 B = entity->basis;
  const mat4 STACK = M * T * R * S * B;
  const mat4 FINAL = STACK;

  // mat4 STACK = mat4(1);

  // if (entity->propagate_scale)
  //  STACK = Spre;
  // if (entity->propagate_rotation)
  //  STACK = R * STACK;
  // if (entity->propagate_scale)
  //  STACK = Spost;
  // if (entity->propagate_translation)
  //  STACK = T * STACK;
  // STACK = M * STACK;

  const uint32 num_meshes = entity->model.size();
  for (uint32 i = 0; i < num_meshes; ++i)
  {
    Mesh_Index mesh_index = entity->model[i].first;
    Material_Index material_index = entity->model[i].second;
    if (mesh_index.i == NODE_NULL)
      continue;
    if (material_index.i == NODE_NULL)
      continue;

    SCRATCH_STRING = s(entity->filename_of_import);
    string &assimp_filename = SCRATCH_STRING;

    World_Object object;

    // ASSERT(resource_manager->opengl_disabled);
    // mesh:
    if (mesh_index.use_mesh_index_for_mesh_pool)
    { // all non-assimp meshes
      object.mesh_descriptor = resource_manager->retrieve_pool_mesh_descriptor(mesh_index);
    }
    else
    { // assimp defined
      resource_manager->request_valid_resource(assimp_filename);
      object.mesh_descriptor = resource_manager->retrieve_assimp_mesh_descriptor(assimp_filename, mesh_index);
    }

    // material:
    if (material_index.use_material_index_for_material_pool)
    { // all non-assimp meshes
      object.material_descriptor = resource_manager->retrieve_pool_material_descriptor(material_index);
    }
    else
    { // assimp defined
      resource_manager->request_valid_resource(assimp_filename);
      object.material_descriptor =
          resource_manager->retrieve_assimp_material_descriptor(assimp_filename, material_index);
    }
    object.transformation = FINAL;
    object.name = entity->name;
    accumulator.push_back(object);
  }
  for (uint32 i = 0; i < entity->children.size(); ++i)
  {
    Node_Index child = entity->children[i];
    visit_nodes(child, STACK, accumulator);
  }
}

std::unique_ptr<Imported_Scene_Data> Resource_Manager::import_aiscene_async(std::string path, uint32 assimp_flags)
{
  std::unique_ptr<Imported_Scene_Data> result = nullptr;
  Imported_Scene_Data *eventual_dst = &import_data[path];
  if (eventual_dst->thread_working_on_import)
  {
    bool thread_has_finished_work = true;
    if (thread_has_finished_work)
    {
      *result = import_aiscene(path, assimp_flags); // should just retrieve the finished async data
      return result;
    }

    return nullptr;
  }

  result = std::make_unique<Imported_Scene_Data>();

  eventual_dst->thread_working_on_import = true;

  // down here should be the actual threaded code, above must be main thread
  *result = import_aiscene(path, assimp_flags);
  return result;
}

Imported_Scene_Node Resource_Manager::_import_aiscene_node(
    std::string assimp_filename, const aiScene *scene, const aiNode *ainode)
{
  Imported_Scene_Node node;
  node.name = copy(&ainode->mName);
  for (uint32 i = 0; i < ainode->mNumMeshes; ++i)
  {
    uint32 mesh_index = ainode->mMeshes[i];
    const aiMesh *aimesh = scene->mMeshes[mesh_index];
    uint32 material_index = aimesh->mMaterialIndex;
    Mesh_Index mesh_result{mesh_index, mesh_index, false};
    Material_Index material_result = {material_index, material_index, false};
    node.mesh_indices.push_back(mesh_result);
    node.material_indices.push_back(material_result);
  }
  node.transform = copy(ainode->mTransformation);

  for (uint32 i = 0; i < ainode->mNumChildren; ++i)
  {
    const aiNode *n = ainode->mChildren[i];
    Imported_Scene_Node child = _import_aiscene_node(assimp_filename, scene, n);
    node.children.push_back(child);
  }
  return node;
}

std::string name_from_ai_type(aiMaterial *ai_material, aiTextureType type)
{
  ASSERT(ai_material);
  const int count = ai_material->GetTextureCount(type);
  aiString name;
  ai_material->GetTexture(type, 0, &name);
  return fix_filename(copy(&name));
}

void gather_all_assimp_materials(aiMaterial *mat)
{
  // all ai supported types
  string diffuse = name_from_ai_type(mat, aiTextureType_DIFFUSE);
  string emissive = name_from_ai_type(mat, aiTextureType_EMISSIVE);
  string normals = name_from_ai_type(mat, aiTextureType_NORMALS);
  string shininess = name_from_ai_type(mat, aiTextureType_SHININESS);
  string specular = name_from_ai_type(mat, aiTextureType_SPECULAR);
  string ambient = name_from_ai_type(mat, aiTextureType_AMBIENT);
  string height = name_from_ai_type(mat, aiTextureType_HEIGHT);
  string opacity = name_from_ai_type(mat, aiTextureType_OPACITY);
  string displacement = name_from_ai_type(mat, aiTextureType_DISPLACEMENT);
  string lightmap = name_from_ai_type(mat, aiTextureType_LIGHTMAP);
  string reflection = name_from_ai_type(mat, aiTextureType_REFLECTION);
  const uint32 count = mat->GetTextureCount(aiTextureType_UNKNOWN);
  vector<string> unknowns;
  for (uint32 i = 0; i < count; ++i)
  {
    unknowns.push_back("");
    aiString name;
    mat->GetTexture(aiTextureType_UNKNOWN, 0, &name);
    unknowns.back() = copy(&name);
  }
}

Material_Descriptor build_material_descriptor(const aiScene *scene, uint32 i, std::string path)
{
  Material_Descriptor d;
  // find the file extension
  size_t slice = path.find_last_of("/\\");
  string dir = path.substr(0, slice) + '/';
  slice = path.find_last_of(".");
  string extension = path.substr(slice, path.size() - slice);

  aiMaterial *mat = scene->mMaterials[i];
  gather_all_assimp_materials(mat);
  if (extension == ".FBX" || extension == ".fbx")
  {
    d.albedo = dir + name_from_ai_type(mat, aiTextureType_DIFFUSE);
    d.emissive = dir + name_from_ai_type(mat, aiTextureType_EMISSIVE);
    d.normal = dir + name_from_ai_type(mat, aiTextureType_NORMALS);
    d.roughness = dir + name_from_ai_type(mat, aiTextureType_SHININESS);
    d.metalness = dir + name_from_ai_type(mat, aiTextureType_SPECULAR);
    d.ambient_occlusion = dir + name_from_ai_type(mat, aiTextureType_REFLECTION);
  }
  else
  {
    ASSERT(0);
    // only .fbx supported for now
    // helper function for material assignments:
    // void gather_all_assimp_materials(aiMaterial* m);
  }

  Material_Descriptor defaults;
  // assimp may set 0 length strings, so reset them to default
  if (d.albedo.name == dir)
    d.albedo = defaults.albedo;
  if (d.emissive.name == dir)
    d.emissive = defaults.emissive;
  if (d.normal.name == dir)
    d.normal = defaults.normal;
  if (d.roughness.name == dir)
    d.roughness = defaults.roughness;
  if (d.metalness.name == dir)
    d.metalness = defaults.metalness;
  if (d.ambient_occlusion.name == dir)
    d.ambient_occlusion = defaults.ambient_occlusion;
  return d;
}

Imported_Scene_Data Resource_Manager::import_aiscene(std::string path, uint32 assimp_flags)
{
  const aiScene *scene = IMPORTER.ReadFile(path.c_str(), assimp_flags);
  if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
  {
    set_message("ERROR::ASSIMP::", IMPORTER.GetErrorString());
    ASSERT(0);
  }

  Imported_Scene_Data dst;
  dst.assimp_filename = path;
  for (uint32 i = 0; i < scene->mNumMeshes; ++i)
  {
    dst.meshes.emplace_back(build_mesh_descriptor(scene, i, path));
  }
  for (uint32 i = 0; i < scene->mNumMaterials; ++i)
  {
    dst.materials.emplace_back(build_material_descriptor(scene, i, path));
  }

  // root node always has nothing.. right?
  const aiNode *root = scene->mRootNode;
  ASSERT(root->mNumMeshes == 0);
  const uint32 number_of_children = scene->mRootNode->mNumChildren;
  for (uint32 i = 0; i < number_of_children; ++i)
  {
    const aiNode *node = scene->mRootNode->mChildren[i];
    Imported_Scene_Node child = _import_aiscene_node(path, scene, node);
    dst.children.push_back(child);
  }
  dst.valid = true;
  return dst;
}

void Resource_Manager::init()
{
  null_material_descriptor = std::make_unique<Material_Descriptor>();
  if (!opengl_disabled)
  {
    null_material = std::make_unique<Material>(*null_material_descriptor.get());
  }
}

std::string Resource_Manager::serialize_mesh_descriptor_pool()
{
  json j;
  for (Mesh_Descriptor &mesh : mesh_descriptor_pool)
  {
    json m = mesh;
    j.push_back(m);
  }
  return pretty_dump(j);
}

std::string Resource_Manager::serialize_material__descriptor_pool()
{
  json j;
  for (Material_Descriptor &material : material_descriptor_pool)
  {
    json m = material;
    j.push_back(m);
  }
  return pretty_dump(j);
}

// immediately allocates resource if opengl_enabled
// invalidates all pool material_descriptor pointers
Material_Index Resource_Manager::push_custom_material(Material_Descriptor *d)
{
  // careful, if ptr d points back into the pool, we will read garbage data if pushback reallocates
  ASSERT(d);
  material_descriptor_pool.push_back(*d);
  if (!opengl_disabled)
  {
    material_pool.emplace_back(Material(*d));
  }
  return {material_descriptor_pool.size() - 1, true};
}

// immediately allocates resource if opengl_enabled
// invalidates all pool mesh_descriptor pointers
Mesh_Index Resource_Manager::push_custom_mesh(Mesh_Descriptor *d)
{
  ASSERT(d);
  mesh_descriptor_pool.push_back(*d);
  if (!opengl_disabled)
  {
    mesh_pool.emplace_back(Mesh(*d));
  }
  return {mesh_descriptor_pool.size() - 1, true};
}

void Resource_Manager::deserialize_mesh_descriptor_pool(json data)
{
  // hold references to the gpu resources so we dont destroy
  std::vector<Mesh> current = move(mesh_pool);
  mesh_descriptor_pool.clear();
  mesh_pool.clear();
  for (uint32 i = 0; i < data.size(); ++i)
  {
    Mesh_Descriptor update = data[i];
    mesh_descriptor_pool.push_back(update);
    mesh_pool.emplace_back(Mesh());
    // if this is default constructed, visitnodes will get a nullptr
    // and not create the entity

    // technically, its pointless to have this filled out - just turn off
    // the mesh pool pointer in the node that references this
    // but we can do it anyway...
    if (update.assimp_filename != "")
    { // if its an assimp import, it might be available in import_data[filename]
      Imported_Scene_Data *entry = request_valid_resource(update.assimp_filename, false);
      if (entry->valid)
      { // success - this import exists and can be copied
        if (!opengl_disabled)
          mesh_pool.back() = Mesh(entry->meshes[update.assimp_index]);
      }
      else
      {
        // it's loading - the mesh will be invisible
        // refresh this function at a later time to check again
      }
    }
    else if (update.primitive != null)
    { // primitives can be created immediately, locally
      // could start an async job for this
      if (!opengl_disabled)
        mesh_pool.back() = Mesh(update);
    }
    else if (update.is_custom_mesh_data)
    { // custom data can be immediately uploaded
      // could start an async job for this
      if (!opengl_disabled)
        mesh_pool.back() = Mesh(update);
    }
  }
}

void Resource_Manager::deserialize_material_descriptor_pool(json data)
{
  // hold references to the gpu resources so we dont destroy
  std::vector<Material> current = move(material_pool);
  material_descriptor_pool.clear();
  material_pool.clear();
  for (uint32 i = 0; i < data.size(); ++i)
  {
    Material_Descriptor update = data[i];
    material_descriptor_pool.push_back(update);
    if (!opengl_disabled)
    {
      material_pool.emplace_back(Material(update));
    }
  }
}

Imported_Scene_Data *Resource_Manager::request_valid_resource(std::string path, bool wait_for_valid)
{
  Imported_Scene_Data *import = &import_data[path];
  bool requires_importing = !import->valid;
  if (requires_importing)
  {
    if (wait_for_valid)
    {
      if (import->thread_working_on_import)
      { // there was a previous call for async, and that load has started, but now we demand it
        std::shared_ptr<Imported_Scene_Data> ptr = import_aiscene_async(path, default_assimp_flags);
        while (!ptr)
        {
          ptr = import_aiscene_async(path, default_assimp_flags);
        }
        *import = *ptr;
        ASSERT(import->valid);
        ASSERT(import->assimp_filename == path);
        if (!opengl_disabled)
        {
          import->allocate_resources();
        }
        return import;
      }

      *import = import_aiscene(path, default_assimp_flags);
      ASSERT(import->valid);
      ASSERT(import->assimp_filename == path);
      if (!opengl_disabled)
      {
        import->allocate_resources();
      }
      return import;
    }
    else
    {
      std::shared_ptr<Imported_Scene_Data> ptr = import_aiscene_async(path, default_assimp_flags);
      if (ptr)
      {
        *import = *ptr;
        ASSERT(import->valid);
        ASSERT(import->assimp_filename == path);
        if (!opengl_disabled)
        {
          import->allocate_resources();
        }
        return import;
      }
      return nullptr;
    }
  }
  return import;
}

Mesh *Resource_Manager::retrieve_assimp_mesh_resource(std::string assimp_path, Mesh_Index i)
{
  ASSERT(!opengl_disabled);
  Imported_Scene_Data *import = &import_data[assimp_path];
  if (!import->valid)
    return nullptr;

  ASSERT(!i.use_mesh_index_for_mesh_pool);
  ASSERT(import->allocated);
  ASSERT(i.i < import->allocated_meshes.size());
  return &import->allocated_meshes[i.i];
}

Material *Resource_Manager::retrieve_assimp_material_resource(std::string assimp_path, Material_Index i)
{
  ASSERT(!opengl_disabled);
  Imported_Scene_Data *import = &import_data[assimp_path];
  if (!import->valid)
    return nullptr;
  ASSERT(!i.use_material_index_for_material_pool);
  ASSERT(i.i < import->allocated_materials.size());
  ASSERT(import->allocated);
  return &import->allocated_materials[i.i];
}

Material_Descriptor *Resource_Manager::retrieve_assimp_material_descriptor(std::string assimp_path, Material_Index i)
{
  Imported_Scene_Data *import = &import_data[assimp_path];
  if (!import->valid)
    return nullptr;
  return &import->materials[i.i];
}

Mesh_Descriptor *Resource_Manager::retrieve_assimp_mesh_descriptor(std::string assimp_path, Mesh_Index i)
{
  Imported_Scene_Data *import = &import_data[assimp_path];
  if (!import->valid)
    return nullptr;
  return &import->meshes[i.i];
}
Mesh *Resource_Manager::retrieve_pool_mesh(Mesh_Index i)
{
  ASSERT(!opengl_disabled);
  ASSERT(i.i < mesh_pool.size());
  if (mesh_pool[i.i].mesh == nullptr)
  { // mesh doesnt exist
    return nullptr;
  }
  ASSERT(i.use_mesh_index_for_mesh_pool);
  return &mesh_pool[i.i];
}
Material *Resource_Manager::retrieve_pool_material(Material_Index i)
{
  ASSERT(!opengl_disabled);
  ASSERT(i.use_material_index_for_material_pool);
  if (i.i == NODE_NULL)
    return null_material.get();
  ASSERT(i.i < material_descriptor_pool.size());
  ASSERT(i.i < material_pool.size());
  return &material_pool[i.i];
}
Material_Descriptor *Resource_Manager::retrieve_pool_material_descriptor(Material_Index i)
{
  ASSERT(i.i < material_descriptor_pool.size());
  ASSERT(i.use_material_index_for_material_pool);
  return &material_descriptor_pool[i.i];
}
Mesh_Descriptor *Resource_Manager::retrieve_pool_mesh_descriptor(Mesh_Index i)
{
  ASSERT(i.i < mesh_descriptor_pool.size());
  ASSERT(i.use_mesh_index_for_mesh_pool);
  return &mesh_descriptor_pool[i.i];
}
void Imported_Scene_Data::allocate_resources()
{
  ASSERT(!allocated);
  ASSERT(allocated_meshes.size() == 0);
  ASSERT(allocated_materials.size() == 0);
  for (Mesh_Descriptor &d : meshes)
  {
    allocated_meshes.emplace_back(Mesh(d));
  }
  for (Material_Descriptor &d : materials)
  {
    allocated_materials.emplace_back(Material(d));
  }
  allocated = true;
}

//
///*
// OLD SCENE GRAPH:
//*/
//
// Scene_Graph_Node::Scene_Graph_Node(string name, const aiNode *node, const mat4 *import_basis_, const aiScene *scene,
//  string scene_file_path, Uint32 *mesh_num, Material_Descriptor *material_override)
//{
//  ASSERT(node);
//  ASSERT(scene);
//  this->name = name;
//  basis = copy(node->mTransformation);
//  if (import_basis_)
//    import_basis = *import_basis_;
//
//  for (uint32 i = 0; i < node->mNumMeshes; ++i)
//  {
//    auto ai_i = node->mMeshes[i];
//    const aiMesh *aimesh = scene->mMeshes[ai_i];
//    (*mesh_num) += 1;
//    string unique_id = scene_file_path + to_string(*mesh_num);
//    Mesh mesh(aimesh, name, unique_id, scene);
//    aiMaterial *ptr = scene->mMaterials[aimesh->mMaterialIndex];
//    Material material(ptr, scene_file_path, material_override);
//    model.push_back({ mesh, material });
//  }
//  owned_children.reserve(node->mNumChildren);
//}
//
// Scene_Graph::Scene_Graph()
//{
//  Node_Ptr ptr = make_shared<Scene_Graph_Node>();
//  ptr->name = "SCENE_GRAPH_ROOT";
//  root = ptr;
//}
//
// void Scene_Graph::add_graph_node(const aiNode *node, weak_ptr<Scene_Graph_Node> parent, const mat4 *import_basis,
//  const aiScene *aiscene, string scene_file_path, Uint32 *mesh_num, Material_Descriptor *material_override)
//{
//  ASSERT(node);
//  ASSERT(aiscene);
//  string name = copy(&node->mName);
//  //// construct just this node in the main node array
//  // auto thing = Scene_Graph_Node(name, node, import_basis, aiscene,
//  //    scene_file_path, mesh_num, material_override);
//
//  shared_ptr<Scene_Graph_Node> new_node =
//    make_shared<Scene_Graph_Node>(name, node, import_basis, aiscene, scene_file_path, mesh_num, material_override);
//
//  set_parent(new_node, parent, true);
//
//  // construct all the new node's children, because its constructor doesn't
//  for (uint32 i = 0; i < node->mNumChildren; ++i)
//  {
//    const aiNode *child = node->mChildren[i];
//    add_graph_node(child, new_node, import_basis, aiscene, scene_file_path, mesh_num, material_override);
//  }
//}
//
// shared_ptr<Scene_Graph_Node> Scene_Graph::add_mesh(
//  Mesh_Data m, Material_Descriptor md, string name, const mat4 *import_basis)
//{
//  Node_Ptr new_node = make_shared<Scene_Graph_Node>();
//
//  new_node->name = name;
//  if (import_basis)
//    new_node->import_basis = *import_basis;
//
//  Mesh mesh(m, name);
//  Material material(md);
//  new_node->model.push_back({ mesh, material });
//  set_parent(new_node, root, false);
//  return new_node;
//}
// void Scene_Graph::set_parent(weak_ptr<Scene_Graph_Node> p, weak_ptr<Scene_Graph_Node> desired_parent, bool
// parent_owned)
//{
//  shared_ptr<Scene_Graph_Node> child = p.lock();
//  shared_ptr<Scene_Graph_Node> current_parent_ptr = child->parent.lock();
//
//  if (current_parent_ptr)
//  {
//    for (auto i = current_parent_ptr->owned_children.begin(); i != current_parent_ptr->owned_children.end(); ++i)
//    {
//      if (*i == p.lock())
//      {
//        current_parent_ptr->owned_children.erase(i);
//        break;
//      }
//    }
//    for (auto i = current_parent_ptr->unowned_children.begin(); i != current_parent_ptr->unowned_children.end(); ++i)
//    {
//      if (i->lock() == p.lock())
//      {
//        current_parent_ptr->unowned_children.erase(i);
//        break;
//      }
//    }
//  }
//
//  shared_ptr<Scene_Graph_Node> desired_parent_ptr = desired_parent.lock();
//  if (desired_parent_ptr)
//  {
//    child->parent = desired_parent;
//    if (parent_owned)
//    {
//      desired_parent_ptr->owned_children.push_back(Node_Ptr(p));
//    }
//    else
//    {
//      desired_parent_ptr->unowned_children.push_back(Node_Ptr(p));
//    }
//  }
//  else
//  {
//    set_message("ERROR: set_parent parent doesnt exist.");
//    ASSERT(0);
//  }
//}
//
// shared_ptr<Scene_Graph_Node> Scene_Graph::add_aiscene(
//  string scene_file_path, const mat4 *import_basis, Material_Descriptor *material_override)
//{
//  return add_aiscene(load_aiscene(scene_file_path), scene_file_path, import_basis, material_override);
//}
//
// shared_ptr<Scene_Graph_Node> Scene_Graph::add_aiscene(string scene_file_path, Material_Descriptor *material_override)
//{
//  return add_aiscene(load_aiscene(scene_file_path), scene_file_path, nullptr, material_override);
//}
//
// shared_ptr<Scene_Graph_Node> Scene_Graph::add_aiscene(
//  const aiScene *scene, string scene_file_path, const mat4 *import_basis, Material_Descriptor *material_override)
//{
//
//  // accumulates as meshes are imported, used along with the scene file path
//  // to create a unique_id for the mesh
//  Uint32 mesh_num = 0;
//  const aiNode *root = scene->mRootNode;
//
//  // create the root node for this scene
//  string name = copy(&root->mName);
//  shared_ptr<Scene_Graph_Node> new_node = make_shared<Scene_Graph_Node>(
//    name, root, import_basis, scene, BASE_MODEL_PATH + scene_file_path, &mesh_num, material_override);
//  set_parent(new_node, this->root, false);
//  new_node->filename_of_import = scene_file_path;
//  new_node->is_root_of_import = true;
//
//  // add every aiscene child to the new node
//  const uint32 num_children = scene->mRootNode->mNumChildren;
//  for (uint32 i = 0; i < num_children; ++i)
//  {
//    const aiNode *node = scene->mRootNode->mChildren[i];
//    add_graph_node(
//      node, new_node, import_basis, scene, BASE_MODEL_PATH + scene_file_path, &mesh_num, material_override);
//  }
//  return new_node;
//}
// void Scene_Graph::visit_nodes(
//  const weak_ptr<Scene_Graph_Node> node_ptr, const mat4 &M, vector<Render_Entity> &accumulator)
//{
//  shared_ptr<Scene_Graph_Node> entity = node_ptr.lock();
//  ASSERT(entity);
//  if ((!entity->visible) && entity->propagate_visibility)
//    return;
//
//  const mat4 T = translate(entity->position);
//  const mat4 S = scale(entity->scale);
//  const mat4 R = toMat4(entity->orientation);
//  const mat4 B = entity->basis;
//  const mat4 I = entity->import_basis;
//
//  mat4 STACK = mat4(1);
//
//  if (entity->propagate_scale)
//  {
//    STACK = S;
//  }
//  if (entity->propagate_rotation)
//  {
//    STACK = R * STACK;
//  }
//  if (entity->propagate_translation)
//  {
//    STACK = T * STACK;
//  }
//  STACK = M * B * STACK;
//
//  const mat4 final_transformation = M * T * R * S * B * I;
//
//  const uint32 num_meshes = entity->model.size();
//  for (uint32 i = 0; i < num_meshes; ++i)
//  {
//    Mesh *mesh_ptr = &entity->model[i].first;
//    Material *material_ptr = &entity->model[i].second;
//
//    if (entity->visible)
//      accumulator.emplace_back(mesh_ptr, material_ptr, final_transformation);
//  }
//  for (auto i = entity->unowned_children.begin(); i != entity->unowned_children.end();)
//  {
//    weak_ptr<Scene_Graph_Node> child = *i;
//    if (!child.lock())
//    {
//      i = entity->unowned_children.erase(i);
//    }
//    else
//    {
//      visit_nodes(child, STACK, accumulator);
//      ++i;
//    }
//  }
//  for (auto i = entity->owned_children.begin(); i != entity->owned_children.end(); ++i)
//  {
//    weak_ptr<Scene_Graph_Node> child = *i;
//    visit_nodes(child, STACK, accumulator);
//  }
//}
// void Scene_Graph::visit_nodes_locked_accumulator(
//  weak_ptr<Scene_Graph_Node> node_ptr, const mat4 &M, vector<Render_Entity> *accumulator, atomic_flag *lock)
//{
//  shared_ptr<Scene_Graph_Node> entity = node_ptr.lock();
//  if ((!entity->visible) && entity->propagate_visibility)
//    return;
//  const mat4 T = translate(entity->position);
//  const mat4 S = scale(entity->scale);
//  const mat4 R = toMat4(entity->orientation);
//  const mat4 B = entity->basis;
//  const mat4 STACK = M * B * T * R * S;
//  const mat4 I = entity->import_basis;
//  const mat4 BASIS = STACK * I;
//
//  const int num_meshes = entity->model.size();
//  for (int i = 0; i < num_meshes; ++i)
//  {
//    Mesh *mesh_ptr = &entity->model[i].first;
//    Material *material_ptr = &entity->model[i].second;
//    while (lock->test_and_set())
//    { /*spin*/
//    }
//    if (entity->visible)
//      accumulator->emplace_back(mesh_ptr, material_ptr, BASIS);
//
//    lock->clear();
//  }
//
//  for (auto i = entity->unowned_children.begin(); i != entity->unowned_children.end();)
//  {
//    weak_ptr<Scene_Graph_Node> child = *i;
//    if (!child.lock())
//    {
//      i = entity->unowned_children.erase(i);
//    }
//    else
//    {
//      visit_nodes_locked_accumulator(child, STACK, accumulator, lock);
//      ++i;
//    }
//  }
//  for (auto i = entity->owned_children.begin(); i != entity->owned_children.end(); ++i)
//  {
//    weak_ptr<Scene_Graph_Node> child = *i;
//    visit_nodes_locked_accumulator(child, STACK, accumulator, lock);
//  }
//}
//
// void Scene_Graph::visit_root_node_base_index(
//  uint32 node_index, uint32 count, vector<Render_Entity> *accumulator, atomic_flag *lock)
//{
//  const Scene_Graph_Node *const entity = root.get();
//  ASSERT(entity->name == "SCENE_GRAPH_ROOT");
//  ASSERT(entity->position == vec3(0));
//  ASSERT(entity->orientation == quat());
//  ASSERT(entity->import_basis == mat4(1));
//  ASSERT(entity->unowned_children.size() == 0);
//  for (uint32 i = node_index; i < node_index + count; ++i)
//  {
//    weak_ptr<Scene_Graph_Node> child = entity->owned_children[i];
//    visit_nodes_locked_accumulator(child, mat4(1), accumulator, lock);
//  }
//}
//
// vector<Render_Entity> Scene_Graph::visit_nodes_async_start()
//{
//  vector<Render_Entity> accumulator;
//  accumulator.reserve(uint32(last_accumulator_size * 1.5));
//
//  const uint32 root_children_count = root->owned_children.size();
//  const uint32 thread_count = 4;
//  const uint32 nodes_per_thread = root_children_count / thread_count;
//  const uint32 leftover_nodes = root_children_count % thread_count;
//
//  PERF_TIMER.start();
//  // todo the refactoring into class member function(or regression to W7 from
//  // W10)
//  // or other change killed the performance here
//  thread threads[thread_count];
//  atomic_flag lock = ATOMIC_FLAG_INIT;
//  for (uint32 i = 0; i < thread_count; ++i)
//  {
//    const uint32 base_index = i * nodes_per_thread;
//    auto fp = [this, base_index, nodes_per_thread, &accumulator, &lock] {
//      this->visit_root_node_base_index(base_index, nodes_per_thread, &accumulator, &lock);
//    };
//    threads[i] = thread(fp);
//  }
//  PERF_TIMER.stop();
//  uint32 leftover_index = (thread_count * nodes_per_thread);
//  ASSERT(leftover_index + leftover_nodes == root_children_count);
//  visit_root_node_base_index(leftover_index, leftover_nodes, &accumulator, &lock);
//  for (int i = 0; i < thread_count; ++i)
//  {
//    threads[i].join();
//  }
//
//  last_accumulator_size = accumulator.size();
//  return accumulator;
//}
// vector<Render_Entity> Scene_Graph::visit_nodes_st_start()
//{
//  vector<Render_Entity> accumulator;
//  accumulator.reserve(uint32(last_accumulator_size * 1.5));
//  visit_nodes(root, mat4(1), accumulator);
//  last_accumulator_size = accumulator.size();
//  return accumulator;
//}
//
// shared_ptr<Scene_Graph_Node> Scene_Graph::add_primitive_mesh(
//  Mesh_Primitive p, string name, Material_Descriptor m, const mat4 *import_basis)
//{
//  shared_ptr<Scene_Graph_Node> new_node = make_shared<Scene_Graph_Node>();
//  new_node->name = name;
//  if (import_basis)
//    new_node->import_basis = *import_basis;
//  Mesh mesh(p, name);
//  Material material(m);
//  new_node->model.push_back({ mesh, material });
//  set_parent(new_node, root, false);
//  return new_node;
//}

inline Flat_Scene_Graph_Node::Flat_Scene_Graph_Node()
{
  for (uint32 i = 0; i < children.size(); ++i)
  {
    children[i] = NODE_NULL;
  }
  for (uint32 i = 0; i < model.size(); ++i)
  {
    model[i] = {{NODE_NULL, false}, {NODE_NULL, false}};
  }
}
