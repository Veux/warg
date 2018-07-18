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
typedef uint32 Node_Index;
typedef uint32 Model_Index;
struct Mesh_Index
{
  uint32 i = NODE_NULL;
  uint32 assimp_original = NODE_NULL;
  // if true, the index will point to an item in the resource manager mesh pool instead of the
  // import piece so that it can be changed at runtime to an arbitrary mesh
  bool use_mesh_index_for_mesh_pool = true;
};
struct Material_Index
{
  uint32 i = NODE_NULL;
  uint32 assimp_original = NODE_NULL;
  // if true, the index will point to an item in the resource manager material pool instead of the
  // import piece so that it can be changed at runtime to an arbitrary material
  bool use_material_index_for_material_pool = true;
};

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
  void update_material_pool_index(Material_Index i)
  {
    if (opengl_disabled)
      return;
    ASSERT(i.i < material_pool.size());
    material_pool[i.i] = material_descriptor_pool[i.i];
  }
  void push_mesh_pool_change(Mesh_Index i)
  {
    if (opengl_disabled)
      return;
    ASSERT(i.i < mesh_pool.size());
    mesh_pool[i.i] = mesh_descriptor_pool[i.i];
  }

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
      Node_Index node_index, Material_Descriptor *m, bool modify_or_overwrite = true, bool children_too = true);
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  // imgui data:
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

  void draw_imgui_specific_mesh(Mesh_Index mesh_index)
  {
    std::unordered_map<std::string, Imported_Scene_Data> *import_data = &resource_manager->import_data;
    std::vector<Mesh_Descriptor> *mesh_descriptor_pool = &resource_manager->mesh_descriptor_pool;

    if (mesh_index.use_mesh_index_for_mesh_pool)
    {
      if (!(mesh_index.i < mesh_descriptor_pool->size()))
      {
        ImGui::Text("No mesh selected");
        return;
      }
      Mesh_Descriptor *ptr = &(*mesh_descriptor_pool)[mesh_index.i];
      ImGui::Text(s("Name:", ptr->name).c_str());
      ImGui::Text(s("assimp_filename:", ptr->assimp_filename).c_str());
      ImGui::Text(s("assimp_index:", ptr->assimp_index).c_str());
      ImGui::Text(s("primitive:", ptr->primitive).c_str());
      ImGui::Text(s("is_custom_mesh_data:", ptr->is_custom_mesh_data).c_str());
      ImGui::Text(s("build_unique_identifier():", ptr->build_unique_identifier()).c_str());

      if (showing_mesh_data)
      {
        if (ImGui::Button("Hide Mesh Data"))
          showing_mesh_data = false;

        ImGui::Text(s("Name: ", ptr->mesh_data.name).c_str());
        ImGui::Text(s("Vertex count: ", ptr->mesh_data.positions.size()).c_str());
        ImGui::Text(s("Indices count: ", ptr->mesh_data.indices.size()).c_str());
        ImGui::Text(s("Unique Identifier: ", ptr->mesh_data.build_unique_identifier()).c_str());

        // todo: render wireframe
      }
      else
      {

        if (ImGui::Button("Show Mesh Data"))
          showing_mesh_data = true;
      }
    }
    else
    {
      ImGui::Text("Assimp mesh");
      ImGui::Text(s(imgui_selected_import_filename).c_str());
    }
  }

  void draw_imgui_specific_material(Material_Index material_index)
  {
    std::unordered_map<std::string, Imported_Scene_Data> *import_data = &resource_manager->import_data;
    std::vector<Material_Descriptor> *material_descriptor_pool = &resource_manager->material_descriptor_pool;

    if (material_index.use_material_index_for_material_pool)
    {
      if (!(material_index.i < material_descriptor_pool->size()))
      {
        ImGui::Text("No material selected");
        return;
      }
      Material_Descriptor *ptr = &(*material_descriptor_pool)[material_index.i];
      draw_imgui_texture_element("Albedo", &ptr->albedo, 0);
      ImGui::Separator();
      draw_imgui_texture_element("Emissive", &ptr->emissive, 1);
      ImGui::Separator();
      draw_imgui_texture_element("Roughness", &ptr->roughness, 2);
      ImGui::Separator();
      draw_imgui_texture_element("Normal", &ptr->normal, 3);
      ImGui::Separator();
      draw_imgui_texture_element("Metalness", &ptr->metalness, 4);
      ImGui::Separator();
      draw_imgui_texture_element("Ambient Occlusion", &ptr->ambient_occlusion, 5);
      ImGui::Separator();
      ImGui::PushItemWidth(200);
      Array_String str = ptr->vertex_shader;
      ImGui::InputText("Vertex Shader", &str.str[0], str.str.size());
      ptr->vertex_shader = s(str);
      Array_String str2 = ptr->frag_shader;
      ImGui::InputText("Fragment Shader", &str2.str[0], str2.str.size());
      ptr->frag_shader = s(str2);
      ImGui::DragFloat2("UV Scale", &ptr->uv_scale[0]);
      ImGui::DragFloat2("Normal UV Scale", &ptr->normal_uv_scale[0]);
      ImGui::DragFloat("Albedo Alpha Override", &ptr->albedo_alpha_override);
      ImGui::Checkbox("Backface Culling", &ptr->backface_culling);
      ImGui::Checkbox("Uses Transparency", &ptr->uses_transparency);
      ImGui::Checkbox("Discard On Alpha", &ptr->discard_on_alpha);
      ImGui::Checkbox("Casts Shadows", &ptr->casts_shadows);
      ImGui::PopItemWidth();
      resource_manager->update_material_pool_index(material_index);
    }
    else
    {
      ImGui::Text("Asimp Import Material:");
      ImGui::Text(s(imgui_selected_import_filename).c_str());
      Material_Descriptor *ptr =
          resource_manager->retrieve_assimp_material_descriptor(s(imgui_selected_import_filename), material_index);
      if (ptr)
      {
        draw_imgui_const_texture_element("Albedo", &ptr->albedo);
        ImGui::Separator();
        draw_imgui_const_texture_element("Emissive", &ptr->emissive);
        ImGui::Separator();
        draw_imgui_const_texture_element("Roughness", &ptr->roughness);
        ImGui::Separator();
        draw_imgui_const_texture_element("Normal", &ptr->normal);
        ImGui::Separator();
        draw_imgui_const_texture_element("Metalness", &ptr->metalness);
        ImGui::Separator();
        draw_imgui_const_texture_element("Ambient Occlusion", &ptr->ambient_occlusion);
        ImGui::Separator();
        ImGui::PushItemWidth(200);
        ImGui::Text(s("Vertex Shader: ", ptr->vertex_shader).c_str());
        ImGui::Text(s("Fragment Shader: ", ptr->frag_shader).c_str());
        // ImGui::Text(s("UV Scale", ptr->uv_scale).c_str());
        // ImGui::Text(s("Normal UV Scale", ptr->normal_uv_scale).c_str());
        ImGui::Text(s("Albedo Alpha Override: ", ptr->albedo_alpha_override).c_str());
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "These should be editable, but needs a way to apply:");
        ImGui::Checkbox("Backface Culling", &ptr->backface_culling);
        ImGui::Checkbox("Uses Transparency", &ptr->uses_transparency);
        ImGui::Checkbox("Discard On Alpha", &ptr->discard_on_alpha);
        ImGui::Checkbox("Casts Shadows", &ptr->casts_shadows);
      }
      else
      {
        ImGui::TextColored(ImVec4(0.5f + 0.5f * sin(get_real_time() * 4.f), 0.0f, 0.0f, 1.0f), "Loading import...");
      }
    }
  }

  void draw_imgui_light_array()
  {
    static bool open = false;
    const uint32 initial_height = 130;
    const uint32 height_per_inactive_light = 25;
    const uint32 max_height = 600;
    uint32 width = 243;

    uint32 height = initial_height + height_per_inactive_light * lights.light_count;
    check_gl_error();
    // ImGui::Begin("lighting adjustment", &open, ImGuiWindowFlags_NoResize);
    {
      check_gl_error();
      static auto picker = File_Picker(".");
      static bool browsing = false;
      static bool type = false; // true for radiance, false for irradiance
      std::string radiance_map_result = lights.environment.radiance.handle->peek_filename();
      std::string irradiance_map_result = lights.environment.irradiance.handle->peek_filename();
      check_gl_error();
      bool updated = false;
      ImGui::Separator();
      if (ImGui::Button("Radiance Map"))
      {
        type = true;
        browsing = true;
      }

      ImGui::SameLine();
      ImGui::TextWrapped(s("Radiance map: ", radiance_map_result).c_str());

      ImGui::Separator();
      if (ImGui::Button("Irradiance Map"))
      {
        type = false;
        browsing = true;
      }
      ImGui::SameLine();
      ImGui::TextWrapped(s("Irradiance map: ", irradiance_map_result).c_str());

      ImGui::Separator();
      if (browsing)
      {
        if (picker.run())
        {
          browsing = false;
          if (type)
            radiance_map_result = picker.get_result();
          else
            irradiance_map_result = picker.get_result();
          updated = true;
        }
        else if (picker.get_closed())
        {
          browsing = false;
        }
      }
      check_gl_error();
      if (updated)
      {
        Environment_Map_Descriptor d;
        d.radiance = radiance_map_result;
        d.irradiance = irradiance_map_result;
        lights.environment = d;
      }
    }

    // ImGui::SetWindowSize(ImVec2((float)width, (float)height));
    if (ImGui::Button("Push Light"))
    {
      lights.light_count++;

      if (lights.light_count > MAX_LIGHTS)
      {
        lights.light_count = MAX_LIGHTS;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Pop Light"))
    {
      if (!lights.light_count == 0)
      {
        lights.light_count--;
      }
    }
    ImGui::SameLine();
    bool want_collapse = false;
    if (ImGui::Button("Collapse"))
    {
      want_collapse = true;
    }
    ImGui::BeginChild("ScrollingRegion");
    for (uint32 i = 0; i < lights.light_count; ++i)
    {
      ImGui::PushID(s(i).c_str());
      ImGui::PushItemWidth(150);
      if (want_collapse)
      {
        ImGui::SetNextTreeNodeOpen(false);
      }
      if (!ImGui::CollapsingHeader(s("Light ", i).c_str()))
      {
        ImGui::PopID();
        continue;
      }
      width = 270;
      height = max_height;
      Light *light = &lights.lights[i];
      ImGui::Indent(5);
      ImGui::LabelText("Option", "Setting %u", i);
      ImGui::ColorPicker3("Radiance", &light->color[0], ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_RGB);
      ImGui::DragFloat3("Position", &light->position[0]);
      ImGui::DragFloat("Brightness", &light->brightness, 0.1f);
      light->brightness = glm::max(light->brightness, 0.0000001f);
      ImGui::DragFloat3("Attenuation", &light->attenuation[0], 0.01f);
      ImGui::DragFloat("Ambient", &light->ambient, 0.001f);
      light->ambient = glm::max(light->ambient, 0.f);
      ImGui::DragFloat("radius", &light->radius, 0.01f);
      std::string current_type = s(light->type);
      if (ImGui::BeginCombo("Light Type", current_type.c_str()))
      {
        const uint light_type_count = 3;
        for (int n = 0; n < light_type_count; n++)
        {
          std::string list_type_n = s(Light_Type(n));
          bool is_selected = (current_type == list_type_n);
          if (ImGui::Selectable(list_type_n.c_str(), is_selected))
            light->type = Light_Type(n);
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      if (light->type == Light_Type::parallel)
      {
        ImGui::DragFloat3("Direction", &light->direction[0]);
      }
      if (light->type == Light_Type::omnidirectional)
      {
      }
      if (light->type == Light_Type::spot)
      {
        ImGui::DragFloat3("Direction", &light->direction[0]);
        ImGui::DragFloat("Focus", &light->cone_angle, 0.0001f, 0.0000001, 0.5);
        ImGui::Checkbox("Casts Shadows", &light->casts_shadows);
        if (light->casts_shadows)
        {
          if (ImGui::TreeNode("Shadow settings"))
          {
            width = 320;
            ImGui::InputInt2("Shadow Map Resolution", &light->shadow_map_resolution[0]);
            ImGui::InputInt("Blur Iterations", &light->shadow_blur_iterations, 1, 0);
            ImGui::DragFloat("Blur Radius", &light->shadow_blur_radius, 0.1f, 0.00001f, 0.0f);
            ImGui::DragFloat("Near Plane", &light->shadow_near_plane, 1.0f, 0.00001f);
            ImGui::DragFloat("Far Plane", &light->shadow_far_plane, 1.0f, 1.0f);
            ImGui::InputFloat("Max Variance", &light->max_variance, 0.0000001f, 0.0001f, 12);
            ImGui::DragFloat("FoV", &light->shadow_fov, 1.0f, 0.0000001f, 90.f);
            ImGui::TreePop();

            light->shadow_blur_iterations = glm::max(light->shadow_blur_iterations, 0);
            light->max_variance = glm::max(light->max_variance, 0.f);
            light->shadow_blur_radius = glm::max(light->shadow_blur_radius, 0.f);
          }
        }
      }
      ImGui::Unindent(5.f);
      ImGui::PopItemWidth();
      ImGui::PopID();
    }
    ImGui::EndChild();
    // ImGui::SetWindowSize(ImVec2((float32)width, (float32)height));
    // ImGui::End();
  }
  void draw_imgui_command_interface()
  {
    if (ImGui::Button("Console"))
    {
      command_window_open = true;
    }

    if (command_window_open)
    {
      ImGui::Begin("Console", &command_window_open);
      ImGui::Text("test");
      ImGui::End();
    }
  }
  void draw_imgui_specific_node(Node_Index node_index)
  {
    // vec3 velocity = { 0, 0, 0 };
    // std::array<std::pair<Mesh_Index, Material_Index>, MAX_MESHES_PER_NODE> model;
    // std::array<Node_Index, MAX_CHILDREN> children;
    // Array_String filename_of_import; // is blank for non-assimp imports
    // bool exists = false;
    // bool visible = true;              // only controls rendering
    // bool propagate_visibility = true; // only controls rendering
    // bool propagate_translation = true;
    // bool propagate_rotation = true;
    // bool propagate_scale = true;
    // bool wait_on_resource = true; // require the mesh and materials to be available immediately
    if (node_index == NODE_NULL)
    {
      ImGui::Text("No node selected");
      return;
    }

    Flat_Scene_Graph_Node *node = &nodes[node_index];
    ImGui::Text("Node:[");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(node_index).c_str());
    ImGui::SameLine();
    ImGui::Text("]:");
    ImGui::SameLine();
    std::string name = s(node->name);
    if (name == "")
    {
      name = "Unnamed Node";
    }
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), name.c_str());
    ImGui::DragFloat3("Position", &node->position[0], 0.01f);
    ImGui::DragFloat3("Scale", &node->scale[0], 0.01f);
    ImGui::DragFloat3("Vertex Scale", &node->scale_vertex[0], 0.01f);
    ImGui::DragFloat3("Velocity", &node->velocity[0], 0.01f);
    ImGui::DragFloat4("Orientation", &node->orientation[0], 0.01f);

    bool non_default_basis = node->import_basis != mat4(1);
    if (non_default_basis)
    {
      ImGui::PushStyleColor(0, ImVec4(0.75 + 0.5 * sin(4.0f * get_real_time()), 0.f, 0.f, 1.f));
      if (ImGui::TreeNode("Import Basis:"))
      {
        ImGui::Text(s(node->import_basis).c_str());
        ImGui::TreePop();
      }
      if (non_default_basis)
        ImGui::PopStyleColor();
    }
    else
    {
      ImGui::Text("Import Basis: Identity");
    }

    ImGui::Text("Parent:[");
    ImGui::SameLine();
    if (node->parent == NODE_NULL)
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
    else
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(node->parent).c_str());
    ImGui::SameLine();
    ImGui::Text("]");

    if (!showing_model)
    {
      if (ImGui::Button("Show Model"))
      {
        showing_model = !showing_model;
      }
    }
    else
    {

      if (ImGui::Button("Hide Model"))
      {
        showing_model = !showing_model;
      }
    }

    if (showing_model)
    {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Filename:");
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), s(node->filename_of_import).c_str());
      for (uint32 i = 0; i < node->model.size(); ++i)
      {
        Model_Index model_index = i;

        std::pair<Mesh_Index, Material_Index> *piece = &node->model[i];

        Mesh_Index mesh_index = piece->first;
        Material_Index material_index = piece->second;

        ImGui::Text(s("[", i, "]:").c_str());
        ImGui::SameLine();
        ImGui::Text("[");
        ImGui::SameLine();

        // mesh
        if (mesh_index.i == NODE_NULL)
        {
          ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
        }
        else
        {
          if (mesh_index.use_mesh_index_for_mesh_pool)
          {
            if (ImGui::Button(s("Mesh Pool[", mesh_index.i, "]").c_str()))
            {
              favor_resource_mesh = true;
              imgui_selected_mesh = mesh_index;
            }
          }
          else
          {
            if (ImGui::Button(s("Assimp Mesh[", mesh_index.i, "]").c_str()))
            {
              favor_resource_mesh = true;
              imgui_selected_mesh = mesh_index;
              imgui_selected_import_filename = node->filename_of_import;
            }
          }
        }

        ImGui::SameLine();
        ImGui::Text(",");
        ImGui::SameLine();

        // mat
        if (material_index.i == NODE_NULL)
        {
          ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
        }
        else
        {
          if (material_index.use_material_index_for_material_pool)
          {
            if (ImGui::Button(s("Material Pool[", material_index.i, "]").c_str()))
            {
              imgui_selected_material = material_index;
              favor_resource_mesh = false;
            }
          }
          else
          {
            if (ImGui::Button(s("Assimp Material[", material_index.i, "]").c_str()))
            {
              favor_resource_mesh = false;
              imgui_selected_material = material_index;
              imgui_selected_import_filename = node->filename_of_import;
            }
          }
        }
        ImGui::SameLine();
        ImGui::Text("]");

        // if (node->parent == NODE_NULL)
        // ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
        // else
        // ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(node->parent).c_str());
      }
    }

    ImGui::Checkbox("Visible", &node->visible);
    ImGui::Checkbox("Propagate Visibility", &node->propagate_visibility);

    ImGui::Text("Final Transformation:");
    mat4 m = build_transformation(node_index, false);
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(m).c_str());
  }
  void draw_imgui_tree_node(Node_Index node_index)
  {
    if (node_index == NODE_NULL)
    {
      return;
    }

    Flat_Scene_Graph_Node *node = &nodes[node_index];
    if (!node->exists)
      return;

    ImGuiTreeNodeFlags flags;
    if (imgui_selected_node == node_index)
      flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Selected;
    else
      flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    std::string name = s(node->name);
    if (name == "")
    {
      name = "Unnamed Node";
    }
    bool open = ImGui::TreeNodeEx(s("[", node_index, "] ", name).c_str(), flags);
    if (ImGui::IsItemClicked())
    {
      if (imgui_selected_node != node_index)
        imgui_selected_node = node_index;
      else
        imgui_selected_node = NODE_NULL;
    }

    if (open)
    {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(name, "'s ", "Children:").c_str());
      ImGui::Separator();
      for (uint32 i = 0; i < node->children.size(); ++i)
      {
        if (node->children[i] != NODE_NULL)
        {
          ImGui::Text("[");
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(i).c_str());
          ImGui::SameLine();
          ImGui::Text("]:");
          ImGui::SameLine();
          draw_imgui_tree_node(node->children[i]);
        }
      }
      ImGui::Separator();
      ImGui::TreePop();
    }
  }
  void draw_imgui_texture_element(const char *name, Texture_Descriptor *ptr, uint32 slot)
  {
    if (ImGui::TreeNode(name))
    {
      ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse;
      ImGui::BeginChild("Texture Stuff", ImVec2(250, 100), false, flags);
      Array_String str = ptr->name;
      ImGui::InputText("Name", &str.str[0], str.str.size());
      ImGui::SameLine();
      if (ImGui::Button("Browse"))
        if (!texture_picker_in_use)
        {
          texture_picker_in_use = true;
          texture_picking_slot = slot;
        }
      if (texture_picker_in_use)
      {
        if (slot == texture_picking_slot)
        { // don't consume the result if it isnt for this texture
          if (texture_picker.run())
          {
            texture_picker_in_use = false;
            str = texture_picker.get_result();
          }
          else if (texture_picker.get_closed())
            texture_picker_in_use = false;
        }
      }
      ptr->name = s(str);
      ImGui::DragFloat4("mod", &ptr->mod[0], 0.001f, 0.0f);
      ImGui::EndChild();
      ImGui::SameLine();
      put_imgui_texture(ptr, vec2(90, 90));
      ImGui::TreePop();
    }
  }

  void draw_imgui_const_texture_element(const char *name, Texture_Descriptor *ptr)
  {
    if (ImGui::TreeNode(name))
    {
      ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse;
      ImGui::BeginChild("Texture Stuff", ImVec2(250, 100), false, flags);
      ImGui::Text(s("Name: ", ptr->name).c_str());
      ImGui::SameLine();
      ImGui::Text(s("mod:", ptr->mod).c_str());
      ImGui::EndChild();

      ImGui::SameLine();
      put_imgui_texture(ptr, vec2(90, 90));
      ImGui::TreePop();
    }
  }
  void draw_imgui_resource_manager()
  {
    std::unordered_map<std::string, Imported_Scene_Data> *import_data = &resource_manager->import_data;
    std::vector<Mesh_Descriptor> *mesh_descriptor_pool = &resource_manager->mesh_descriptor_pool;
    std::vector<Material_Descriptor> *material_descriptor_pool = &resource_manager->material_descriptor_pool;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (ImGui::TreeNodeEx("Assimp Imports", flags))
    {
      for (auto &scene : *import_data)
      {
        bool open = ImGui::TreeNodeEx(s("[", scene.first, "] ").c_str(), flags);

        if (open)
        {
          ImGui::Text(scene.first.c_str());
          // imgui_selected_import_filename = node->filename_of_import;
          ImGui::TreePop();
        }
      }
      ImGui::TreePop();
    }

    // draw and handle mesh selection
    if (ImGui::TreeNodeEx("Mesh Descriptor Pool", flags))
    {
      for (uint32 i = 0; i < mesh_descriptor_pool->size(); ++i)
      {
        ImGui::PushID(i);
        ImGuiTreeNodeFlags flags;
        const bool this_mesh_matches_selected =
            (imgui_selected_mesh.i == i) && (imgui_selected_mesh.use_mesh_index_for_mesh_pool);

        // highlight the node if its selected
        if (imgui_selected_mesh.i == i)
          flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Selected;
        else
          flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        // check if we clicked on it to set the index
        bool open = ImGui::TreeNodeEx(s("[", i, "] ").c_str(), flags);
        if (ImGui::IsItemClicked())
        {
          if (!this_mesh_matches_selected)
          {
            imgui_selected_mesh.i = i;
            imgui_selected_mesh.use_mesh_index_for_mesh_pool = true;
            favor_resource_mesh = true;
          }
          else
            imgui_selected_mesh = {};
        }

        // if its open, uh, do nothing - maybe you dont want a tree for material descriptor pool
        // todo: maybe there are other parameters you could list here, like a list of nodes that used it last frame
        if (open)
        {
          ImGui::Text("todo: material pool slot stuff");
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    }

    // draw and handle material selection
    if (ImGui::TreeNodeEx("Material Descriptor Pool", flags))
    {
      for (uint32 i = 0; i < material_descriptor_pool->size(); ++i)
      {
        ImGui::PushID(i);
        ImGuiTreeNodeFlags flags;

        const bool this_material_matches_selected =
            (imgui_selected_material.i == i) && (imgui_selected_material.use_material_index_for_material_pool);

        // highlight the node if its selected
        if (imgui_selected_material.i == i)
          flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Selected;
        else
          flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        // check if we clicked on it to set the index
        bool open = ImGui::TreeNodeEx(s("[", i, "] ").c_str(), flags);
        if (ImGui::IsItemClicked())
        {
          if (!this_material_matches_selected)
          {
            imgui_selected_material.i = i;
            imgui_selected_material.use_material_index_for_material_pool = true;
            favor_resource_mesh = false;
          }
          else
            imgui_selected_material = {};
        }

        // if its open, uh, do nothing - maybe you dont want a tree for material descriptor pool
        // todo: maybe there are other parameters you could list here, like a list of nodes that used it last frame
        if (open)
        {
          ImGui::Text("todo: material pool slot stuff");
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    }
  }
  void draw_active_nodes()
  {
    for (uint32 i = 0; i < MAX_NODES; ++i)
    {
      Flat_Scene_Graph_Node *node = &nodes[i];

      if (node->parent == NODE_NULL)
      {
        if (node->exists)
        {
          draw_imgui_tree_node(i);
        }
      }
    }
  }
  void draw_imgui()
  {
    const float32 selected_node_draw_height = 340;
    const float32 default_window_height = 800;
    const float32 horizontal_split_size = 450;
    static float32 last_seen_height = 600;
    ImGui::Begin("Scene Graph", &imgui_open);
    float32 fudge = 50.f;
    float32 width = ImGui::GetWindowWidth();
    if (!ImGui::IsWindowCollapsed())
    {
      last_seen_height = ImGui::GetWindowHeight();
    }
    ImGui::SetWindowSize(ImVec2(3 * horizontal_split_size + fudge, last_seen_height));
    const float32 line_height = ImGui::GetTextLineHeight();
    float32 upper_child_height = ImGui::GetContentRegionMax().y - selected_node_draw_height - fudge;

    // parent
    ImGui::BeginChild("parent", ImVec2(horizontal_split_size * 2 + 15, 0), false);
    {
      // nodes
      ImGui::BeginChild("Active Scene Graph Nodes_title", ImVec2(horizontal_split_size, upper_child_height), true);
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Active Scene Graph Nodes:");
      ImGui::BeginChild("Active Nodes:");
      draw_active_nodes();
      ImGui::EndChild();
      ImGui::EndChild();

      // resource manager
      ImGui::SameLine();
      ImGui::BeginChild("resource_manager_title", ImVec2(horizontal_split_size, upper_child_height), true);
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Resource Manager:");
      ImGui::BeginChild("Resource Manager:");
      draw_imgui_resource_manager();
      ImGui::EndChild();
      ImGui::EndChild();

      // selected node
      ImGui::BeginChild("Selected Scene Graph_title", ImVec2(horizontal_split_size, selected_node_draw_height), true);
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected Scene Graph Node:");
      ImGui::BeginChild("Selected Node:");
      draw_imgui_specific_node(imgui_selected_node);
      ImGui::EndChild();
      ImGui::EndChild();

      // selected resource
      ImGui::SameLine();
      ImGui::BeginChild("Selected Resource title", ImVec2(horizontal_split_size, selected_node_draw_height), true);
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected Resource:");
      ImGui::BeginChild("Selected Resource:");
      if (favor_resource_mesh)
        draw_imgui_specific_mesh(imgui_selected_mesh);
      else
        draw_imgui_specific_material(imgui_selected_material);
      ImGui::EndChild();
      ImGui::EndChild();
    }
    // parent
    ImGui::EndChild();

    // light array
    ImGui::SameLine();
    ImGui::BeginChild("LightArrayTitle", ImVec2(horizontal_split_size, last_seen_height - 54), true);
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Light Array:");
    ImGui::BeginChild("Light Array");
    draw_imgui_light_array();
    ImGui::EndChild();
    ImGui::EndChild();

    // draw_imgui_command_interface();
    // todo: UI for set parent, grab, drop, material changes,
    // todo: clickable node indices take u to selected, clickable material and mesh indices, same
    // todo: automatic sphere creation and updating for push light and pop light
    // what about deserialization? light array should hold node indices to its spheres - if these are included in the
    // json then it just works
    // cleanup all the little windows - make some sort of ui to select them or combine
    // todo: configurable length scale setting for bloom high pass filter

    ImGui::End();
  }

  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  // uses assimp's defined material if assimp import, else a default-constructed material
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

  Resource_Manager *resource_manager;

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
