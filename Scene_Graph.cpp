#include "stdafx.h"
#include "Scene_Graph.h"
#include "Globals.h"
#include "Render.h"
#include "Physics.h"
#include "assimp/metadata.h"
#include <errno.h>
using json = nlohmann::json;
using namespace std;
using namespace ImGui;

Assimp::Importer IMPORTER;
mat4 get_wow_asset_import_transformation();
bool TriangleAABB(const Triangle_Normal &triangle, const AABB &aabb);
uint32 new_ID()
{
  static uint32 last = 0;
  return last += 1;
}
bool push_color_text_if_tree_label_open(const char *label, ImVec4 color_true, ImVec4 color_false)
{
  ImGuiWindow *window = ImGui::GetCurrentWindow();
  ImGuiID id = window->GetID(label);
  bool node_is_open = ImGui::TreeNodeBehaviorIsOpen(id, ImGuiTreeNodeFlags_Framed);
  if (node_is_open)
  {
    ImGui::PushStyleColor(ImGuiCol_Text, color_true);
  }
  else
  {
    ImGui::PushStyleColor(ImGuiCol_Text, color_false);
  }
  return node_is_open;
}

Node_Index Scene_Graph::add_mesh(std::string name, Mesh_Descriptor *d, Material_Descriptor *md)
{
  Mesh_Index mesh_index = resource_manager->push_mesh(d);
  Material_Index material_index = resource_manager->push_material(md);

  Node_Index node_index = new_node();
  Scene_Graph_Node *node = &nodes[node_index];
  node->model[0] = {mesh_index, material_index};
  node->name = name;
  return node_index;
}

Node_Index Scene_Graph::add_mesh(Mesh_Primitive p, std::string name, Material_Descriptor *md)
{
  Mesh_Descriptor d(p, name);
  return add_mesh(name, &d, md);
}

// Material_Index Scene_Graph::copy_material_to_new_pool_slot(
//    Node_Index i, Model_Index model_index, Material_Descriptor *m, bool modify_or_overwrite)
//{
//  Scene_Graph_Node *node = &nodes[i];
//  ASSERT(model_index < MAX_MESHES_PER_NODE);
//  if (node->model[model_index].second.i == NODE_NULL)
//  {
//    ASSERT(0);
//  }
//
//  Material_Index current_index = node->model[model_index].second;
//  Material_Index new_index;
//  Material_Descriptor valid = *resource_manager->retrieve_pool_material(current_index)->get_descriptor();
//  new_index = resource_manager->push_custom_material(&valid);
//
//  Material_Descriptor *new_descriptor = resource_manager->retrieve_pool_material_descriptor(new_index);
//
//  if (m)
//  {
//    modify_material(new_index, m, modify_or_overwrite);
//  }
//  node->model[model_index].second = new_index;
//  return new_index;
//}

void Scene_Graph::modify_material(
    Material_Index material_index, Material_Descriptor *material, bool modify_or_overwrite)
{
  Material_Descriptor *current_descriptor = resource_manager->material_pool[material_index].get_modifiable_descriptor();
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
}

void Scene_Graph::modify_all_materials(
    Node_Index node_index, Material_Descriptor *m, bool modify_or_overwrite, bool children_too)
{
  if (node_index == NODE_NULL)
    return;
  Scene_Graph_Node *node = &nodes[node_index];

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

void Scene_Graph::draw_imgui_specific_mesh(Mesh_Index mesh_index)
{
  std::unordered_map<std::string, Imported_Scene_Data> *import_data = &resource_manager->import_data;
  std::array<Mesh, MAX_POOL_SIZE> *mesh_pool = &resource_manager->mesh_pool;
  // could you actually just... put an entirely new state object in here and render it but not to screen
  // just to the renderers buffer and then use that as a imgui texture
  if (!(mesh_index < resource_manager->current_mesh_pool_size))
  {
    ImGui::Text("No mesh selected");
    return;
  }
  Mesh_Descriptor *ptr = &(resource_manager->mesh_pool[mesh_index].mesh->descriptor);
  ImGui::Text(s("Name:", ptr->name).c_str());
  ImGui::Text(s("unique_identifier:", ptr->unique_identifier).c_str());

  if (showing_mesh_data)
  {
    if (ImGui::Button("Hide Mesh Data"))
      showing_mesh_data = false;

    ImGui::Text(s("Vertex count: ", ptr->mesh_data.positions.size()).c_str());
    ImGui::Text(s("Indices count: ", ptr->mesh_data.indices.size()).c_str());

    // todo: render wireframe
  }
  else
  {

    if (ImGui::Button("Show Mesh Data"))
      showing_mesh_data = true;
  }
}

void Scene_Graph::draw_imgui_specific_material(Material_Index material_index)
{
  std::unordered_map<std::string, Imported_Scene_Data> *import_data = &resource_manager->import_data;

  if (!(material_index < resource_manager->current_material_pool_size))
  {
    ImGui::Text("No material selected");
    return;
  }

  Material *ptr = &resource_manager->material_pool[material_index];
  draw_imgui_texture_element("Albedo", &ptr->albedo, 0);
  // ImGui::Separator();
  draw_imgui_texture_element("Emissive", &ptr->emissive, 1);
  // ImGui::Separator();
  draw_imgui_texture_element("Roughness", &ptr->roughness, 2);
  // ImGui::Separator();
  bool new_normal = draw_imgui_texture_element("Normal", &ptr->normal, 3);
  if (new_normal)
  {
    if (ptr->normal.t.name == "default" || ptr->normal.t.name == "white" || ptr->normal.t.name == "white.png")
    {
      ptr->normal.t.mod = vec4(0.5, 0.5, 1.0, 0.0);
    }
    else
    {
      ptr->normal.t.mod = vec4(1, 1, 1, 0);
    }
  }
  // ImGui::Separator();
  draw_imgui_texture_element("Metalness", &ptr->metalness, 4);
  // ImGui::Separator();
  draw_imgui_texture_element("Ambient Occlusion", &ptr->ambient_occlusion, 5);
  draw_imgui_texture_element("Displacement", &ptr->displacement, 6);
  // ImGui::Separator();
  ImGui::PushItemWidth(200);
  Array_String str = ptr->descriptor.vertex_shader;
  bool reload_shader = ImGui::InputText("Vertex Shader", &str.str[0], str.str.size());
  ptr->descriptor.vertex_shader = s(str);
  Array_String str2 = ptr->descriptor.frag_shader;
  reload_shader = reload_shader || ImGui::InputText("Fragment Shader", &str2.str[0], str2.str.size());
  ptr->descriptor.frag_shader = s(str2);

  if (reload_shader)
  {
    Shader temp = ptr->shader;
    ptr->shader = Shader(ptr->descriptor.vertex_shader, ptr->descriptor.frag_shader);
  }

  ImGui::DragFloat2("UV Scale", &ptr->descriptor.uv_scale[0]);
  ImGui::DragFloat2("Normal UV Scale", &ptr->descriptor.normal_uv_scale[0]);
  ImGui::DragFloat("Dielectric Reflectivity", &ptr->descriptor.dielectric_reflectivity);
  ImGui::Checkbox("Backface Culling", &ptr->descriptor.backface_culling);
  ImGui::Checkbox("Wireframe", &ptr->descriptor.wireframe);
  ImGui::Checkbox("Discard On Alpha", &ptr->descriptor.discard_on_alpha);
  ImGui::DragFloat("Discard Threshold", &ptr->descriptor.discard_threshold, 0.01f, 0.f, 1.f);
  ImGui::Checkbox("include_AO_in_uv_scale", &ptr->descriptor.include_ao_in_uv_scale);
  ImGui::SliderFloat("index of refraction", &ptr->descriptor.ior, 0.0, 2.0, "%.4f");
  ImGui::Checkbox("require_self_depth", &ptr->descriptor.require_self_depth);
  ImGui::Checkbox("multiply_albedo_by_alpha", &ptr->descriptor.multiply_albedo_by_alpha);
  ImGui::Checkbox("multiply_result_by_alpha", &ptr->descriptor.multiply_result_by_alpha);
  ImGui::Checkbox("multiply_pixelalpha_by_moda", &ptr->descriptor.multiply_pixelalpha_by_moda);

  ImGui::Checkbox("depth_test", &ptr->descriptor.depth_test);
  ImGui::Checkbox("depth_mask", &ptr->descriptor.depth_mask);

  ImGui::SliderFloat("Derivative offset", &ptr->descriptor.derivative_offset, 0.001f, 0.5f);
  ImGui::Checkbox("Casts Shadows", &ptr->descriptor.casts_shadows);

  ImGui::Checkbox("Translucent Pass", &ptr->descriptor.translucent_pass);
  if (ptr->descriptor.translucent_pass)
  {
    if (ImGui::BeginCombo("Blend Mode", s(ptr->descriptor.blendmode).c_str()))
    {
      const uint count = uint(Material_Blend_Mode::end);
      for (int i = 0; i < count; i++)
      {
        std::string list_type_n = s(Material_Blend_Mode(i));
        bool is_selected = (ptr->descriptor.blendmode == Material_Blend_Mode(i));
        if (ImGui::Selectable(list_type_n.c_str(), is_selected))
          ptr->descriptor.blendmode = Material_Blend_Mode(i);
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
  }
  ImGui::PopItemWidth();
}

void Scene_Graph::draw_imgui_light_array()
{
  static bool open = false;
  const uint32 initial_height = 130;
  const uint32 height_per_inactive_light = 25;
  const uint32 max_height = 600;
  uint32 width = 243;

  uint32 height = initial_height + height_per_inactive_light * lights.light_count;
  // ImGui::Begin("lighting adjustment", &open, ImGuiWindowFlags_NoResize);
  {
    std::string radiance_map_result = lights.environment.radiance;
    std::string irradiance_map_result = lights.environment.irradiance;
    bool updated = false;
    ImGui::Separator();
    if (ImGui::Button("Radiance Map"))
    {
      file_type = true;
      texture_picker.window_open = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Irradiance Map"))
    {
      file_type = false;
      texture_picker.window_open = true;
    }
    if (texture_picker.run())
    {
      if (file_type)
        radiance_map_result = texture_picker.get_result();
      else
        irradiance_map_result = texture_picker.get_result();
      updated = true;
    }

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
    // ImGui::PushItemWidth(150);
    if (want_collapse)
    {
      ImGui::SetNextTreeNodeOpen(false);
    }
    if (!ImGui::CollapsingHeader(s("Light ", i).c_str()))
    {
      ImGui::PopID();
      continue;
    }
    // width = 270;
    height = max_height;
    Light *light = &lights.lights[i];
    ImGui::Indent(5);
    ImGui::LabelText("Option", "Setting %u", i);
    ImGui::ColorPicker3("Radiance", &light->color[0], ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_RGB);
    ImGui::DragFloat3("Position", &light->position[0]);
    ImGui::DragFloat("Brightness", &light->brightness, 0.1f, -1000000, 100000000, "%.3f", 3.f);
    light->brightness = glm::max(light->brightness, 0.0000001f);
    ImGui::DragFloat3("Attenuation", &light->attenuation[0], 0.01f);
    ImGui::DragFloat("Ambient", &light->ambient, 0.001f, 0.0, 100, "%0.8f", 3.0f);
    // light->ambient = glm::max(light->ambient, 0.f);
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
          // width = 320;
          ImGui::InputInt2("Shadow Map Resolution", &light->shadow_map_resolution[0]);
          ImGui::InputInt("Blur Iterations", &light->shadow_blur_iterations, 1, 0);
          ImGui::DragFloat("Blur Radius", &light->shadow_blur_radius, 0.01f, 0.00001f, 0.0f, "%.5f", 2.0f);
          ImGui::DragFloat("Near Plane", &light->shadow_near_plane, 1.0f, 0.00001f);
          ImGui::DragFloat("Far Plane", &light->shadow_far_plane, 1.0f, 1.0f);
          ImGui::DragFloat("Max Variance", &light->max_variance, 0.001f, 0.0f, 12, "%.8f", 3.0f);
          ImGui::DragFloat("FoV", &light->shadow_fov, 0.001f, 0.0f, 12, "%.8f", 3.0f);
          ImGui::TreePop();

          light->shadow_blur_iterations = glm::max(light->shadow_blur_iterations, 0);
          light->max_variance = glm::max(light->max_variance, 0.f);
          light->shadow_blur_radius = glm::max(light->shadow_blur_radius, 0.f);
        }
      }
    }

    if (ImGui::Button("Save Light"))
    {
      std::stringstream ss;
      ss.precision(numeric_limits<float32>::digits10 - 1);
      ss << fixed;
      ss << "Light* light" << i << " = &scene.lights.lights[" << i << "];\n";
      ss << "light" << i << "->position = vec3(" << light->position.x << "," << light->position.y << ","
         << light->position.z << ");\n";
      ss << "light" << i << "->direction = vec3(" << light->direction.x << "," << light->direction.y << ","
         << light->direction.z << ");\n";
      ss << "light" << i << "->brightness = " << light->brightness << ";\n";
      ss << "light" << i << "->color = vec3(" << light->color.x << "," << light->color.y << "," << light->color.z
         << ");\n";
      ss << "light" << i << "->attenuation = vec3(" << light->attenuation.x << "," << light->attenuation.y << ","
         << light->attenuation.z << ");\n";
      ss << "light" << i << "->ambient = " << light->ambient << ";\n";
      ss << "light" << i << "->radius = " << light->radius << ";\n";
      ss << "light" << i << "->cone_angle = " << light->cone_angle << ";\n";
      ss << "light" << i << "->type = Light_Type::";

      if (light->type == Light_Type::parallel)
      {
        ss << "parallel;\n";
      }
      if (light->type == Light_Type::omnidirectional)
      {
        ss << "omnidirectional;\n";
      }
      if (light->type == Light_Type::spot)
      {
        ss << "spot;\n";
      }
      ss << setprecision(12);
      ss << "light" << i << "->casts_shadows = " << light->casts_shadows << ";\n";
      ss << "light" << i << "->shadow_blur_iterations = " << light->shadow_blur_iterations << ";\n";
      ss << "light" << i << "->shadow_blur_radius = " << light->shadow_blur_radius << ";\n";
      ss << "light" << i << "->shadow_near_plane = " << light->shadow_near_plane << ";\n";
      ss << "light" << i << "->shadow_far_plane = " << light->shadow_far_plane << ";\n";
      ss << "light" << i << "->max_variance = " << light->max_variance << ";\n";
      ss << "light" << i << "->shadow_fov = " << light->shadow_fov << ";\n";
      ss << "light" << i << "->shadow_map_resolution = ivec2(" << light->shadow_map_resolution.x << ","
         << light->shadow_map_resolution.y << ");\n";

      int result = SDL_SetClipboardText(ss.str().c_str());
      if (result == 0)
      {
        set_message("Copied to clipboard:", ss.str(), 1.0f);
      }
      else
      {
        set_message("Copied to clipboard failed.", "", 1.0f);
      }
    }
    ImGui::Unindent(5.f);
    // ImGui::PopItemWidth();
    ImGui::PopID();
  }
  ImGui::EndChild();
  // ImGui::SetWindowSize(ImVec2((float32)width, (float32)height));
  // ImGui::End();
}

void Scene_Graph::draw_imgui_command_interface()
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

void Scene_Graph::draw_imgui_specific_node(Node_Index node_index)
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

  Scene_Graph_Node *node = &nodes[node_index];
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
  ImGui::DragFloat3("Oriented Scale", &node->oriented_scale[0], 0.01f);
  ImGui::DragFloat3("Vertex Scale", &node->scale_vertex[0], 0.01f);
  ImGui::DragFloat3("Velocity", &node->velocity[0], 0.01f);
  ImGui::DragFloat4("Orientation", &node->orientation[0], 0.01f);

  // bool non_default_basis = node->import_basis != mat4(1);
  // if (non_default_basis)
  //{
  //  ImGui::PushStyleColor(0, ImVec4(0.75f + 0.5f * (float32)sin(4.0f * get_real_time()), 0.f, 0.f, 1.f));
  //  if (ImGui::TreeNode("Import Basis:"))
  //  {
  //    //ImGui::Text(s(node->import_basis).c_str());
  //    ImGui::TreePop();
  //  }
  //  if (non_default_basis)
  //    ImGui::PopStyleColor();
  //}
  // else
  //{
  //  ImGui::Text("Import Basis: Identity");
  //}

  ImGui::Text("Parent:[");
  ImGui::SameLine();
  if (node->parent == NODE_NULL)
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
  else
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(node->parent).c_str());
  ImGui::SameLine();
  ImGui::Text("]");

  ImGui::Text("Collider:[");
  ImGui::SameLine();
  if (node->collider == NODE_NULL)
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
  else
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(node->collider).c_str());
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
    ImGui::TextWrapped(s(node->filename_of_import).c_str());
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
      if (mesh_index == NODE_NULL)
      {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
      }
      else
      {
        if (ImGui::Button(s("Mesh Pool[", mesh_index, "]").c_str()))
        {
          imgui_selected_mesh = mesh_index;
        }
      }

      ImGui::SameLine();
      ImGui::Text(",");
      ImGui::SameLine();

      // mat
      if (material_index == NODE_NULL)
      {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
      }
      else
      {
        if (ImGui::Button(s("Material Pool[", material_index, "]").c_str()))
        {
          imgui_selected_material = material_index;
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
  if (ImGui::Button(s("Delete Node (dangerous)").c_str()))
  {
    delete_node(node_index);
  }
}

void Scene_Graph::draw_imgui_tree_node(Node_Index node_index)
{
  if (node_index == NODE_NULL)
  {
    return;
  }

  Scene_Graph_Node *node = &nodes[node_index];
  if (!node->exists)
    return;

  ImGuiTreeNodeFlags flags;
  bool selected = imgui_selected_node == node_index;
  if (selected)
    flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Selected;
  else
    flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

  std::string name = s(node->name);
  if (name == "")
  {
    name = "Unnamed Node";
  }

  std::string label = s("[", node_index, "] ", name);

  push_color_text_if_tree_label_open(label.c_str(), {0, 1, 0, 1}, {1, 1, 1, 1});

  if (selected)
  {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 1, 1));
  }
  bool open = ImGui::TreeNodeEx(label.c_str(), flags);
  if (selected)
  {
    ImGui::PopStyleColor();
  }

  ImGui::PopStyleColor();

  if (ImGui::IsItemClicked())
  {
    if (imgui_selected_node != node_index)
      imgui_selected_node = node_index;
    else
      imgui_selected_node = NODE_NULL;
  }

  if (open)
  {
    // ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(name, "'s ", "Children:").c_str());
    // ImGui::Separator();

    bool empty = true;
    for (uint32 i = 0; i < node->children.size(); ++i)
    {
      if (node->children[i] != NODE_NULL)
      {
        empty = false;
        ImGui::Text("[");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(i).c_str());
        ImGui::SameLine();
        ImGui::Text("]:");
        ImGui::SameLine();
        draw_imgui_tree_node(node->children[i]);
      }
    }
    if (empty)
    {
      ImGui::TextColored(ImVec4(1.05f, 1.05f, 1.05f, 1.0f), "---");
    }
    // ImGui::Separator();
    ImGui::TreePop();
  }
}

bool Scene_Graph::draw_imgui_texture_element(const char *name, Texture *ptr, uint32 slot)
{
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  Array_String str = ptr->t.name;
  ImGui::PushID(int(name));
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse;
  ImGui::BeginChild("Texture Stuff", ImVec2(160, 160), false, flags);
  ImGui::Text(name);
  ImGui::SameLine();
  if (ImGui::Button("Browse"))
  {
    if (!texture_picker.window_open)
    {
      texture_picker.window_open = true;
      texture_picking_slot = slot;
    }
  }
  if (texture_picker.window_open && slot == texture_picking_slot)
  {
    if (texture_picker.run())
    {
      str = texture_picker.get_result();
    }
  }

  ImGui::InputText("Name", &str.str[0], str.str.size());

  bool reload = s(str) != ptr->t.name;
  if (reload)
  {
    ptr->t.name = s(str);
    ptr->t.source = ptr->t.name;
    Texture nu(ptr->t);
    *ptr = nu;
  }

  ImGui::DragFloat("mod_r", &ptr->t.mod[0], 0.001f, 0.0f);
  ImGui::DragFloat("mod_g", &ptr->t.mod[1], 0.001f, 0.0f);
  ImGui::DragFloat("mod_b", &ptr->t.mod[2], 0.001f, 0.0f);
  ImGui::DragFloat("mod_a", &ptr->t.mod[3], 0.001f, 0.0f);
  ImGui::EndChild();
  ImGui::SameLine();
  put_imgui_texture(ptr, vec2(140, 140));

  ImGui::PopID();
  ImGui::NewLine();
  return reload;
}

void Scene_Graph::draw_imgui_const_texture_element(const char *name, Texture_Descriptor *ptr)
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

void Scene_Graph::draw_imgui_resource_manager()
{
  std::unordered_map<std::string, Imported_Scene_Data> *import_data = &resource_manager->import_data;
  std::array<Mesh, MAX_POOL_SIZE> *mesh_pool = &resource_manager->mesh_pool;
  std::array<Material, MAX_POOL_SIZE> *material_pool = &resource_manager->material_pool;
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
    for (uint32 i = 0; i < resource_manager->current_mesh_pool_size; ++i)
    {
      ImGui::PushID(i);
      ImGuiTreeNodeFlags flags;
      const bool this_mesh_matches_selected = (imgui_selected_mesh == i);

      // highlight the node if its selected
      if (imgui_selected_mesh == i)
        flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Selected;
      else
        flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

      // check if we clicked on it to set the index
      bool open = ImGui::TreeNodeEx(s("[", i, "] ").c_str(), flags);
      if (ImGui::IsItemClicked())
      {
        if (!this_mesh_matches_selected)
        {
          imgui_selected_mesh = i;
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
    for (uint32 i = 0; i < resource_manager->current_material_pool_size; ++i)
    {
      ImGui::PushID(i);
      ImGuiTreeNodeFlags flags;

      const bool this_material_matches_selected = (imgui_selected_material == i);

      // highlight the node if its selected
      if (imgui_selected_material == i)
        flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Selected;
      else
        flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

      // check if we clicked on it to set the index
      bool open = ImGui::TreeNodeEx(s("[", i, "] ").c_str(), flags);
      if (ImGui::IsItemClicked())
      {
        if (!this_material_matches_selected)
        {
          imgui_selected_material = i;
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

void Scene_Graph::draw_active_nodes()
{
  for (uint32 i = 0; i < MAX_NODES; ++i)
  {
    Scene_Graph_Node *node = &nodes[i];

    if (node->parent == NODE_NULL)
    {
      if (node->exists)
      {
        draw_imgui_tree_node(i);
      }
    }
  }
}

const char *imgui_pane_to_string(imgui_pane p)
{
  if (p == imgui_pane::node_tree)
  {
    return "Nodes";
  }
  if (p == imgui_pane::resource_man)
  {
    return "Resource Manager";
  }
  if (p == imgui_pane::light_array)
  {
    return "Light Array";
  }
  if (p == imgui_pane::selected_node)
  {
    return "Selected Node";
  }
  if (p == imgui_pane::selected_mes)
  {
    return "Selected Mesh";
  }
  if (p == imgui_pane::selected_mat)
  {
    return "Selected Material";
  }
  if (p == imgui_pane::particle_emit)
  {
    return "Particle Emitter";
  }
  if (p == imgui_pane::octree)
  {
    return "Octree";
  }
  if (p == imgui_pane::console)
  {
    return "Console";
  }
  if (p == imgui_pane::blank)
  {
    return "Blank";
  }
  return "Unknown";
}

void Scene_Graph::draw_imgui_pane_selection_button(imgui_pane *modifying)
{
  PushID(uint32(modifying));

  // ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.8, 0, .8, 1));

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.18, .180, .368, 1));
  // ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1., 0, 1., 1));
  // ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1., .3, 1., 1));
  if (Button("Pane Selection"))
  {
    OpenPopup("popperup");
  }
  ImGui::PopStyleColor();
  // ImGui::PopStyleColor();
  // ImGui::PopStyleColor();

  SameLine();
  TextColored(ImVec4(0, 1, 1, 1), imgui_pane_to_string(*modifying));
  if (BeginPopup("popperup"))
  {
    TextColored(ImVec4(0, 1, 1, 1), "Pane:");
    ImGui::Separator();

    for (uint32 i = 0; i < (uint32)imgui_pane::end; ++i)
    {
      if (ImGui::Selectable(imgui_pane_to_string(imgui_pane(i))))
      {
        *modifying = imgui_pane(i);
      }
    }

    ImGui::EndPopup();
  }
  PopID();
}

void imgui_node_element(Node_Index node)
{
  ImGui::Text("Node_Index:[");
  ImGui::SameLine();
  if (node == NODE_NULL)
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
  else
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(node).c_str());
  ImGui::SameLine();
  ImGui::Text("]");
}

void Scene_Graph::draw_imgui_particle_emitter()
{

  if (ImGui::Button("Push Emitter"))
  {
    particle_emitters.push_back({});
  }
  ImGui::SameLine();
  if (ImGui::Button("Pop Emitter"))
  {
    particle_emitters.pop_back();
  }
  ImGui::SameLine();
  bool want_collapse = false;
  if (ImGui::Button("Collapse"))
  {
    want_collapse = true;
  }
  ImGui::BeginChild("ScrollingRegion");
  for (uint32 i = 0; i < particle_emitters.size(); ++i)
  {
    ImGui::PushID(s("emitter", i).c_str());
    if (want_collapse)
    {
      ImGui::SetNextTreeNodeOpen(false);
    }
    if (!ImGui::CollapsingHeader(s("Particle Emitter ", i).c_str()))
    {
      ImGui::PopID();
      continue;
    }
    Particle_Emitter *pe = &particle_emitters[i];
    Particle_Emission_Method_Descriptor *pemd = &pe->descriptor.emission_descriptor;
    Particle_Physics_Method_Descriptor *ppmd = &pe->descriptor.physics_descriptor;

    ImGui::Indent(5);
    ImGui::Text("Mesh_Index:[");
    ImGui::SameLine();
    if (pe->mesh_index == NODE_NULL)
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
    else
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(pe->mesh_index).c_str());
    ImGui::SameLine();
    ImGui::Text("]");

    ImGui::Text("Material_Index:[");
    ImGui::SameLine();
    if (pe->material_index == NODE_NULL)
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
    else
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), s(pe->material_index).c_str());
    ImGui::SameLine();
    ImGui::Text("]");

    ImGui::Unindent(5);

    Checkbox("static_geometry_collision", &pe->descriptor.static_geometry_collision);
    Checkbox("dynamic_geometry_collision", &pe->descriptor.dynamic_geometry_collision);
    DragFloat("maximum_octree_probe_size", &pe->descriptor.maximum_octree_probe_size);
    InputFloat("simulate_for_n_secs_on_init - todo", &pe->descriptor.simulate_for_n_secs_on_init);

    const char *emission_label = "Emission Method";
    bool node_open = push_color_text_if_tree_label_open(emission_label, ImVec4(0, 255, 0, 1), ImVec4(255, 0, 0, 1));
    if (ImGui::TreeNode(emission_label))
    {
      ImGui::PopStyleColor();
      // ImGui::Indent(5);
      std::string current_type = s(pemd->type);
      if (ImGui::BeginCombo("Physics Type", current_type.c_str()))
      {
        const uint emitter_type_count = 2;
        for (int n = 0; n < emitter_type_count; n++)
        {
          std::string list_type_n = s(Particle_Emission_Type(n));
          bool is_selected = (current_type == list_type_n);
          if (ImGui::Selectable(list_type_n.c_str(), is_selected))
            pemd->type = Particle_Emission_Type(n);
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      // slider* is the range-clamped with the scrollbar thing
      // drag* is the unbounded dragger
      // input* is text-entry
      PushItemWidth(-1);
      Checkbox("generate_particles", &pemd->generate_particles);
      Checkbox("allow_colliding_spawns", &pemd->allow_colliding_spawns);
      TextColored(imgui_purple, "Position:");
      Text("initial_position_variance:");
      DragFloat3("initial_position_variance", &pemd->initial_position_variance[0]);
      Separator();
      TextColored(imgui_teal, "Velocity:");
      Text("initial_velocity:");
      DragFloat3("initial_velocity", &pemd->initial_velocity[0]);
      Text("initial_velocity_variance:");
      DragFloat3("initial_velocity_variance", &pemd->initial_velocity_variance[0]);
      Text("initial_angular_velocity:");
      DragFloat3("initial_angular_velocity", &pemd->initial_angular_velocity[0]);
      Text("initial_angular_velocity_variance:");
      DragFloat3("initial_angular_velocity_variance", &pemd->initial_angular_velocity_variance[0]);

      Separator();
      TextColored(imgui_purple, "Orientation:");
      Text("randomized_orientation_axis:");
      DragFloat4("randomized_orientation_axis", &pemd->randomized_orientation_axis[0]);
      Text("randomized_orientation_angle_variance:");
      DragFloat("randomized_orientation_angle_variance", &pemd->randomized_orientation_angle_variance);
      Text("intitial_orientation_axis:");
      DragFloat3("intitial_orientation_axis", &pemd->intitial_orientation_axis[0]);
      Text("initial_orientation_angle:");
      DragFloat("initial_orientation_angle", &pemd->initial_orientation_angle);
      Separator();

      Text("billboard_rotation_velocity:");
      DragFloat("billboard_rotation_velocity", &pemd->billboard_rotation_velocity);
      Separator();
      Text("initial_billboard_rotation_velocity_variance:");
      DragFloat("initial_billboard_rotation_velocity_variance", &pemd->initial_billboard_rotation_velocity_variance);
      Separator();

      TextColored(imgui_teal, "Scale:");
      Text("initial_scale:");
      DragFloat3("initial_scale", &pemd->initial_scale[0]);
      Text("initial_extra_scale_variance:");
      DragFloat3("initial_extra_scale_variance", &pemd->initial_extra_scale_variance[0]);
      Text("initial_extra_scale_uniform_variance:");
      DragFloat("initial_extra_scale_uniform_variance", &pemd->initial_extra_scale_uniform_variance);
      Separator();

      TextColored(imgui_teal, "Type Specific settings:");
      TextColored(imgui_teal, "Stream settings:");
      Text("particles_per_second:");
      DragFloat("particles_per_second", &pemd->particles_per_second);
      Separator();

      TextColored(imgui_teal, "Explosion:");
      Text("explosion_particle_count:");
      DragInt("explosion_particle_count", (int32 *)&pemd->explosion_particle_count, 1.f, 0);
      Text("boom_t:");
      DragFloat("boom_t", &pemd->boom_t);
      Text("power:");
      DragFloat("power", &pemd->power);
      Text("impulse_center_offset_min:");
      DragFloat3("impulse_center_offset_min", &pemd->impulse_center_offset_min[0]);
      Text("impulse_center_offset_max:");
      DragFloat3("impulse_center_offset_max", &pemd->impulse_center_offset_max[0]);
      Separator();
      if (pemd->impulse_center_offset_min.x > pemd->impulse_center_offset_max.x)
      {
        pemd->impulse_center_offset_min.x = pemd->impulse_center_offset_max.x;
      }
      if (pemd->impulse_center_offset_min.y > pemd->impulse_center_offset_max.y)
      {
        pemd->impulse_center_offset_min.y = pemd->impulse_center_offset_max.y;
      }
      if (pemd->impulse_center_offset_min.z > pemd->impulse_center_offset_max.z)
      {
        pemd->impulse_center_offset_min.z = pemd->impulse_center_offset_max.z;
      }

      TextColored(imgui_purple, "Misc:");
      Text("inherit_velocity:");
      DragFloat("inherit_velocity", &pemd->inherit_velocity, 0.05f);
      Text("minimum_time_to_live:");
      DragFloat("minimum_time_to_live", &pemd->minimum_time_to_live);
      Text("extra_time_to_live_variance:");
      DragFloat("extra_time_to_live_variance", &pemd->extra_time_to_live_variance);

      Separator();

      TextColored(imgui_purple, "May need more implmenentation work:");
      Text("hammersley_radius:");
      DragFloat("hammersley_radius", &pemd->hammersley_radius);
      Text("enforce_velocity_position_offset_match:");
      Checkbox("enforce_velocity_position_offset_match", &pemd->enforce_velocity_position_offset_match);
      Text("low_discrepency_position_variance:");
      Checkbox("low_discrepency_position_variance", &pemd->low_discrepency_position_variance);
      Text("hammersley_sphere:");
      Checkbox("hammersley_sphere", &pemd->hammersley_sphere);
      Separator();
      Separator();
      TextColored(imgui_purple, "May crash or not work at all:");
      Text("particles_per_spawn:");
      DragInt("particles_per_spawn - todo", (int32 *)&pemd->particles_per_spawn, 1.f, 0);
      Text("billboarding - todo:");
      Checkbox("billboarding - todo", &pemd->snap_to_basis);
      Text("billboard_lock_z - todo:");
      Checkbox("billboard_lock_z - todo", &pemd->snap_to_basis);
      Text("snap_to_basis - todo:");
      Checkbox("snap_to_basis - todo", &pemd->snap_to_basis);
      Text("simulate_for_n_secs_on_init - todo:");
      ImGui::PopItemWidth();

      // ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "NODE_NULL");
      // ImGui::InputInt2("Shadow Map Resolution", &light->shadow_map_resolution[0]);
      //  ImGui::DragFloat3("Velocity", &node->velocity[0], 0.01f);
      // ImGui::DragFloat4("Orientation", &node->orientation[0], 0.01f);

      //  bool draw = Checkbox("Draw Octree", &draw_collision_octree);
      // ImGui::Text("testtext");

      // ImGui::Unindent(5);
      ImGui::TreePop();
    }
    else
    {
      ImGui::PopStyleColor();
    }

    const char *physics_label = "Physics Method";
    node_open = push_color_text_if_tree_label_open(physics_label, imgui_green, imgui_red);
    if (ImGui::TreeNode(physics_label))
    {
      ImGui::PopStyleColor();
      // ImGui::Indent(5);
      std::string current_type = s(ppmd->type);
      if (ImGui::BeginCombo("Physics Type", current_type.c_str()))
      {
        const uint32 physics_type_count = 2;
        for (uint32 n = 0; n < physics_type_count; n++)
        {
          std::string list_type_n = s(Particle_Physics_Type(n));
          bool is_selected = (current_type == list_type_n);
          if (ImGui::Selectable(list_type_n.c_str(), is_selected))
            ppmd->type = Particle_Physics_Type(n);
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      TextColored(imgui_purple, "Misc:");
      Checkbox("abort_when_late", &ppmd->abort_when_late);
      SliderInt("collision_binary_search_iterations", (int32 *)&ppmd->collision_binary_search_iterations, 0, 25);
      DragFloat("mass", &ppmd->mass);
      DragFloat3("gravity", &ppmd->gravity[0]);
      DragFloat("drag", &ppmd->drag, 0.01f, 0.f, 1.0f);
      DragFloat("bounce_min", &ppmd->bounce_min);
      DragFloat("bounce_max", &ppmd->bounce_max);
      if (ppmd->bounce_min > ppmd->bounce_max)
      {
        ppmd->bounce_min = ppmd->bounce_max;
      }
      DragFloat("size_multiply_uniform_min", &ppmd->size_multiply_uniform_min);
      DragFloat("size_multiply_uniform_max", &ppmd->size_multiply_uniform_max);
      if (ppmd->size_multiply_uniform_min > ppmd->size_multiply_uniform_max)
      {
        ppmd->size_multiply_uniform_min = ppmd->size_multiply_uniform_max;
      }
      DragFloat3("size_multiply_min", &ppmd->size_multiply_min[0]);
      DragFloat3("size_multiply_max", &ppmd->size_multiply_max[0]);
      if (ppmd->size_multiply_min.x > ppmd->size_multiply_max.x)
      {
        ppmd->size_multiply_min.x = ppmd->size_multiply_max.x;
      }
      if (ppmd->size_multiply_min.y > ppmd->size_multiply_max.y)
      {
        ppmd->size_multiply_min.y = ppmd->size_multiply_max.y;
      }
      if (ppmd->size_multiply_min.z > ppmd->size_multiply_max.z)
      {
        ppmd->size_multiply_min.z = ppmd->size_multiply_max.z;
      }
      DragFloat3("die_when_size_smaller_than", &ppmd->die_when_size_smaller_than[0]);
      DragFloat3("friction", &ppmd->friction[0]);

      DragFloat("stiction_velocity", &ppmd->stiction_velocity);
      DragFloat("billboard_rotation_velocity_multiply", &ppmd->billboard_rotation_velocity_multiply);
      DragFloat("billboard_rotation_time_factor", &ppmd->billboard_rotation_time_factor);

      TextColored(imgui_teal, "Type Specific settings:");
      TextColored(imgui_teal, "Simple Settings:");

      TextColored(imgui_teal, "Wind Settings:");
      DragFloat3("direction", &ppmd->direction[0]);
      DragFloat("wind_intensity", &ppmd->wind_intensity);

      TextColored(imgui_purple, "May need more implmenentation work:");

      TextColored(imgui_purple, "May crash or not work at all:");

      ImGui::TreePop();
    }
    else
    {
      ImGui::PopStyleColor();
    }

    const char *perf_label = "Performance Statistics";
    node_open = push_color_text_if_tree_label_open(perf_label, imgui_green, imgui_red);
    if (ImGui::TreeNode(perf_label))
    {
      ImGui::PopStyleColor();

      if (!pe->descriptor.physics_descriptor.dynamic_geometry_collision ||
          !pe->descriptor.physics_descriptor.static_octree)
      {
        ImGui::TextColored(imgui_red, "Static octree collision is off.");
      }
      else
      {
        ImGui::Text("Static Octree:");
        ImGui::Indent(10);
        ImGui::TextColored(imgui_red, pe->per_static_octree_test.string_report().c_str());
        ImGui::Text("Max collider count: ");
        ImGui::SameLine();
        ImGui::TextColored(imgui_red, s(pe->static_collider_count_max).c_str());
        if (pe->static_collider_count_samples != 0)
        {
          ImGui::Text("Collider count sum: ");
          ImGui::Text(s(pe->static_collider_count_sum).c_str());
          ImGui::Text("Collider count samples: ");
          ImGui::Text(s(float64(pe->static_collider_count_samples)).c_str());
          ImGui::Text("Average collider count: ");
          ImGui::SameLine();
          std::stringstream ss;
          ss << std::setprecision(4) << std::scientific
             << float64(pe->static_collider_count_sum) / float64(pe->static_collider_count_samples);
          ImGui::TextColored(imgui_red, ss.str().c_str());
        }

        ImGui::Unindent(10);
      }
      if (!pe->descriptor.physics_descriptor.dynamic_geometry_collision ||
          !pe->descriptor.physics_descriptor.dynamic_octree)
      {
        ImGui::TextColored(imgui_red, "Dynamic octree collision is off.");
      }
      else
      {
        ImGui::Text("Dynamic octree:");
        ImGui::Indent(10);
        ImGui::TextColored(imgui_red, pe->per_dynamic_octree_test.string_report().c_str());
        ImGui::Text("Max collider count: ");
        ImGui::SameLine();
        ImGui::TextColored(imgui_red, s(pe->dynamic_collider_count_max).c_str());

        if (pe->dynamic_collider_count_samples != 0)
        {
          ImGui::Text("Collider count sum: ");
          ImGui::Text(s(pe->dynamic_collider_count_sum).c_str());
          ImGui::Text("Collider count samples: ");
          ImGui::Text(s(float64(pe->dynamic_collider_count_samples)).c_str());
          ImGui::Text("Average collider count: ");
          ImGui::SameLine();
          std::stringstream ss;
          ss << std::setprecision(4) << std::scientific
             << float64(pe->dynamic_collider_count_sum) / float64(pe->dynamic_collider_count_samples);
          ImGui::TextColored(imgui_red, ss.str().c_str());
        }
        ImGui::Unindent(10);
      }
      ImGui::TreePop();
    }

    else
    {
      ImGui::PopStyleColor();
    }

    if (ImGui::Button("Save Emitter"))
    {
      std::stringstream ss;
      ss.precision(numeric_limits<float32>::digits10 - 1);
      ss << fixed;
      ss << "Light* light" << i << " = &scene.lights.lights[" << i << "];\n";

      if (ppmd->type == Particle_Physics_Type::simple)
      {
        ss << "parallel;\n";
      }

      int32 result = SDL_SetClipboardText(ss.str().c_str());
      if (result == 0)
      {
        set_message("Copied to clipboard:", ss.str(), 1.0f);
      }
      else
      {
        set_message("Copied to clipboard failed.", "", 1.0f);
      }
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void Scene_Graph::draw_imgui_octree()
{

  ImGui::Text(s("Pushed Triangles: ", collision_octree.pushed_triangle_count).c_str());
  ImGui::Text(s("Stored Triangles: ", collision_octree.stored_triangle_count).c_str());
  ImGui::Text(
      s("Ratio:", float32(collision_octree.stored_triangle_count) / float32(collision_octree.pushed_triangle_count))
          .c_str());

  bool draw = Checkbox("Draw Octree", &draw_collision_octree);
  if (draw_collision_octree)
  {
    bool b = Checkbox("Draw Nodes", &collision_octree.draw_nodes);
    if (b && collision_octree.draw_nodes)
    {
      // collision_octree.update_render_entities = true;
    }
    b = b || Checkbox("Draw Triangles", &collision_octree.draw_triangles);
    if (b && collision_octree.draw_triangles)
    {
      // collision_octree.update_render_entities = true;
    }
    b = b || Checkbox("Draw Normals", &collision_octree.draw_normals);
    if (b && collision_octree.draw_normals)
    {
      // collision_octree.update_render_entities = true;
    }
    b = b || Checkbox("Draw Velocity", &collision_octree.draw_velocity);
    if (b && collision_octree.draw_velocity)
    {
      // collision_octree.update_render_entities = true;
    }
    b = SliderInt("Draw Depth", &collision_octree.depth_to_render, -1, MAX_OCTREE_DEPTH);
    if (b && (collision_octree.draw_nodes || collision_octree.draw_velocity || collision_octree.draw_normals ||
                 collision_octree.draw_triangles))
    {
      collision_octree.update_render_entities = true;
    }
  }
}

std::string graph_console_log = "Warg Console v0.1 :^)\n";
int want_scroll_down = 2; // has lag and doesnt scroll down enough if we do it once
bool want_refocus = false;
int console_callback(ImGuiInputTextCallbackData *data)
{
  // set_message("current textbox state",data->Buf,5.);
  return 1;
}

bool match(std::string_view func, std::string_view str)
{
  return str.substr(0, func.length()).compare(func) == 0;
}

std::string_view extract_between(char a, char b, std::string_view str)
{
  uint32 a_count = 0;

  int32 a_pos = -1;
  int32 b_pos = -1;

  // a = (
  // b = )
  // asijdhfi(s,a,fg,hg(),asdf)()  -> s,a,fg,hg(),asdf

  for (uint32 i = 0; i < str.size(); ++i)
  {
    const char ci = str[i];
    if (ci == a)
    {
      a_count += 1; // push
      if (a_pos == -1)
      {
        a_pos = i + 1;
        continue;
      }
    }

    if (ci == b)
    {
      a_count -= 1; // pop
      if (a_count == 0)
      {
        b_pos = i;
        break;
      }
      if (a_pos == -1)
      {
        return "";
      }
    }
  }
  if (a_pos == -1 || b_pos == -1)
  {
    return "";
  }

  return str.substr(a_pos, b_pos - a_pos);
}

void extract_args(std::string_view str, vector<string> *args)
{
  std::string_view args_view = extract_between('(', ')', str);

  // asd,etfg,asd(),sgd -> [[asd],[etfg],[asd()],[sgd]]

  size_t end = args_view.size();
  uint32 cursor = 0;
  while (true)
  {
    args_view.remove_prefix(cursor);
    size_t i = args_view.find_first_of(',');

    if (i == -1)
    {
      args->push_back(string(args_view));
      break;
    }
    std::string_view this_arg = args_view.substr(cursor, i);
    cursor = i + 1;
    args->push_back(string(this_arg));
  }
}

void Scene_Graph::handle_console_command(std::string_view cmd)
{

  graph_console_log.append(s(">>", string(cmd), "\n"));
  set_message("console command:", std::string(cmd), 15.0f);

  static vector<string> args;
  args.clear();
  extract_args(cmd, &args);
  if (match("add_aiscene", cmd))
  {
    if (args.size())
    {
      string *filename = &args[0];
      string object_name = "unnamed_node";
      if (args.size() > 1)
      {
        object_name = args[1];
      }
      add_aiscene_new(*filename, object_name);
    }
  }

  if (match("delete_node", cmd))
  {
    if (args.size())
    {
      if (args[0] != "")
      {
        errno = 0;
        Node_Index i = strtol(args[0].c_str(), nullptr, 10);
        if (!errno)
          delete_node(i);
      }
    }
  }

  if (match("help", cmd))
  {
    graph_console_log.append("Available functions:\ndelete_node(Node_Index)\nadd_aiscene(filename,name)\n\n");
  }
}

void Scene_Graph::draw_imgui_console(ImVec2 section_size)
{
  ImGuiWindowFlags childflags = ImGuiWindowFlags_None;
  ImGui::BeginChild("Console_Log:", ImVec2(section_size.x, 260), true, childflags);

  ImGui::TextWrapped(graph_console_log.c_str());
  if (want_scroll_down > 0)
  {
    ImGui::SetScrollY(ImGui::GetScrollMaxY());
    want_scroll_down = want_scroll_down - 1;
  }
  ImGui::EndChild();

  ImGui::BeginChild("Console_Cmd:");
  static std::string buf;
  buf.resize(512);
  size_t size = buf.size();
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_EnterReturnsTrue;
  // ImGui::InputTextMultiline("blah", &buf[0], size, section_size, flags);
  ImGui::SetNextItemWidth(section_size.x);
  if (ImGui::InputText("", &buf[0], int(size), flags, console_callback))
  {
    string_view sv = {buf.c_str(), strlen(buf.c_str())};
    handle_console_command(sv);
    buf.clear();
    buf.resize(512);
    want_scroll_down = 3;
    want_refocus = true;
  }

  if (want_refocus)
  {
    ImGui::SetKeyboardFocusHere(-1);
    want_refocus = false;
  }

  ImGui::IsItemEdited();
  ImGui::EndChild();
}

void Scene_Graph::draw_imgui_selected_pane(imgui_pane p)
{
  const float32 horizontal_tile_size = 350;
  const float32 vertical_tile_size = 400;
  if (p == imgui_pane::node_tree)
  {
    // ImGui::TextColored(ImVec4(1, 0, 0, 1), "Active Scene Graph Nodes:");
    ImGui::BeginChild("Active Nodes:");
    draw_active_nodes();
    ImGui::EndChild();
  }

  if (p == imgui_pane::resource_man)
  {
    // ImGui::TextColored(ImVec4(1, 0, 0, 1), "Resource Manager:");
    ImGui::BeginChild("Resource Manager:");
    draw_imgui_resource_manager();
    ImGui::EndChild();
  }
  if (p == imgui_pane::light_array)
  {
    // ImGui::TextColored(ImVec4(1, 0, 0, 1), "Light Array:");
    ImGui::BeginChild("Light Array");
    draw_imgui_light_array();
    ImGui::EndChild();
  }
  if (p == imgui_pane::selected_node)
  {
    // ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected Scene Graph Node:");
    ImGui::BeginChild("Selected Node:");
    draw_imgui_specific_node(imgui_selected_node);
    ImGui::EndChild();
  }
  if (p == imgui_pane::selected_mes)
  {
    // ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected Mesh:");
    ImGui::BeginChild("Selected Mesh:");
    draw_imgui_specific_mesh(imgui_selected_mesh);
    ImGui::EndChild();
  }
  if (p == imgui_pane::selected_mat)
  {
    // ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected Material:");
    ImGui::BeginChild("Selected Material:");
    draw_imgui_specific_material(imgui_selected_material);
    ImGui::EndChild();
  }
  if (p == imgui_pane::particle_emit)
  {
    // ImGui::TextColored(ImVec4(1, 0, 0, 1), "Particle Emitters:");
    ImGui::BeginChild("Particle Emitters:");
    draw_imgui_particle_emitter();
    ImGui::EndChild();
  }
  if (p == imgui_pane::octree)
  {
    // ImGui::TextColored(ImVec4(1, 0, 0, 1), "Octree:");
    ImGui::BeginChild("Octree:");
    draw_imgui_octree();
    ImGui::EndChild();
  }
  if (p == imgui_pane::console)
  {
    ImGui::BeginChild("Console:");
    draw_imgui_console(ImVec2(horizontal_tile_size, vertical_tile_size));
    ImGui::EndChild();
  }

  if (p == imgui_pane::blank)
  {
    ImGui::BeginChild("Blank:");
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Blank Pane:");
    ImGui::EndChild();
  }
}
// uses assimp's defined material if assimp import, else a default-constructed material

void Scene_Graph::draw_imgui(std::string name)
{
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  const float32 selected_node_draw_height = 340;
  const float32 horizontal_tile_size = 350;
  const float32 vertical_tile_size = 400;
  // ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar;
  ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;

  ImGui::Begin(s("Scene Graph:", name).c_str(), &imgui_open, flags);

  // const float32 line_height = ImGui::GetTextLineHeight();

  ImVec2 before = ImGui::GetCursorPos();

  ImGui::BeginGroup();
  ImGui::Dummy(ImVec2(15, 24));

  if (ArrowButton("left", ImGuiDir_Left))
  { // pop a pane on the right for every row
    imgui_col_count -= 1;
  }
  ImGui::EndGroup();

  ImGui::SameLine();
  ImGui::BeginGroup();
  // remove entire row
  if (ArrowButton("up", ImGuiDir_Up))
  {
    if (imgui_rows.size() > 1)
      imgui_rows_count -= 1;
  }
  // add entire row
  if (ArrowButton("down", ImGuiDir_Down))
  {
    if (imgui_rows.size() < imgui_rows_count + 1)
    { // need to add an entire row
      imgui_rows.push_back({imgui_pane::blank});
    }
    // now lets make sure it has enough columns
    while (imgui_rows.back().size() < imgui_col_count)
    {
      imgui_rows.back().push_back(imgui_pane::blank);
    }
    imgui_rows_count += 1;
  }
  ImGui::EndGroup();

  ImGui::SameLine();

  ImGui::BeginGroup();
  ImGui::Dummy(ImVec2(15, 24));
  if (ArrowButton("right", ImGuiDir_Right))
  { // push a pane on the right for every row
    for (uint32 i = 0; i < imgui_rows.size(); ++i)
    {
      if (imgui_rows[i].size() < imgui_col_count + 1)
      {
        imgui_rows[i].push_back(imgui_pane::blank);
      }
    }
    imgui_col_count += 1;
  }
  ImGui::EndGroup();

  if (imgui_rows_count < 1)
    imgui_rows_count = 1;
  if (imgui_col_count < 1)
    imgui_col_count = 1;

  ImGui::SameLine();
  ImVec2 after = ImGui::GetCursorPos();
  float32 width_of_arrow_group = after.x - before.x;

  size_t emitter_count = particle_emitters.size();
  float32 window_width = GetWindowWidth();
  std::mt19937 generator2 = generator;
  generator.seed(0);
  vec3 color = abs(random_3D_unit_vector());
  generator = generator2;

  float32 available_size = (imgui_col_count * (horizontal_tile_size)) - width_of_arrow_group;

  bool icarus = true;
  if (icarus)
  {

    available_size -= 120;
    available_size = available_size - 5 + ((imgui_col_count - 1) * 7);
  }

  ImVec2 each_emitter_size = ImVec2(floor(available_size / emitter_count), 110.0f);
  for (uint32 i = 0; i < emitter_count; ++i)
  {
    // std::vector<float64> idle64 = particle_emitters[i].idle.get_ordered_times();
    // std::vector<float64> active64 = particle_emitters[i].active.get_ordered_times();
    std::vector<float64> idle64 = particle_emitters[i].idle.get_times();
    std::vector<float64> active64 = particle_emitters[i].active.get_times();
    std::vector<float64> time_allocs64 = particle_emitters[i].time_allocations.get_times();
    std::vector<float64> attribute_times64 = particle_emitters[i].attribute_times.get_times();

    size_t size = idle64.size();
    std::vector<float32> idle(size);
    std::vector<float32> active(size);
    std::vector<float32> load(size);
    std::vector<float32> time_allocs(size);
    std::vector<float32> attribute_times(size);
    float64 inv_dt = 1.0 / dt;
    for (size_t j = 0; j < size; ++j)
    {
      size_t dst_index = j;
      // dst_index = (size - 1) - j; //??
      idle[j] = float32(idle64[j]);
      active[j] = float32(active64[j]);
      time_allocs[j] = float32(inv_dt * time_allocs64[j]);
      attribute_times[j] = float32(inv_dt * attribute_times64[j]);
      // load[dst_index] = active[j] / (active[j] + idle[j]);
      load[dst_index] = active[j] / float32(time_allocs64[j]);
    }
    if (size != 0)
    {

      PushID(s("histogram", i).c_str());
      ImVec2 cursor_pos_for_this_graph = ImGui::GetCursorPos();
      ImGui::PlotHistogram("", &load[0], int(size), 0, NULL, 0.0f, 1.0f, each_emitter_size);
      PopID();
      // ImGui::PushStyleColor(ImGuiCol_Text, color_true);
      PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

      PushStyleColor(ImGuiCol_PlotLines, imgui_red);
      PushID(s("histogram2", i).c_str());
      ImGui::SetCursorPos(cursor_pos_for_this_graph);
      ImGui::PlotLines("", &time_allocs[0], int(size), 0, NULL, 0.0f, 1.0f, each_emitter_size);
      PopStyleColor();
      PopID();

      PushStyleColor(ImGuiCol_PlotLines, imgui_blue);
      PushID(s("histogram3", i).c_str());
      ImGui::SetCursorPos(cursor_pos_for_this_graph);
      ImGui::PlotLines("", &attribute_times[0], int(size), 0, NULL, 0.0f, 1.0f, each_emitter_size);
      PopStyleColor();
      PopID();

      PopStyleColor();
    }
    else
    {
      Dummy(each_emitter_size);
    }
    if (i != emitter_count - 1)
    {
      ImGui::SameLine();
    }
  }

  if (icarus)
  {
    ImGui::SameLine();
    static Texture_Descriptor td("io.jpg");
    static Texture io = Texture(td);
    put_imgui_texture(&io, vec2(110));
  }

  for (uint32 i = 0; i < imgui_rows_count; ++i)
  { // rows
    for (uint32 j = 0; j < imgui_col_count; ++j)
    { // cols
      if (j != 0)
      {
        ImGui::SameLine();
      }

      ImGui::BeginChild(s("pane", i, j).c_str(), ImVec2(horizontal_tile_size, vertical_tile_size), true);
      draw_imgui_pane_selection_button(&imgui_rows[i][j]);
      draw_imgui_selected_pane(imgui_rows[i][j]);
      ImGui::EndChild();
    }

    // ImGui::SameLine();
  }

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

Scene_Graph::Scene_Graph(Resource_Manager *manager) : resource_manager(manager) {}

void Scene_Graph::clear()
{
  for (uint32 i = 0; i < nodes.size(); ++i)
  {
    nodes[i] = Scene_Graph_Node();
  }
  lights = Light_Array();
  particle_emitters.clear();
}

unordered_map<string, pair<Mesh_Index, Material_Index>> create_import_pool_data(
    Imported_Scene_Data *resource, Resource_Manager *resource_manager, Octree *collision_octree)
{
  // all meshes with the same name will get the same mesh_index and material_index
  unordered_map<string, pair<Mesh_Index, Material_Index>> indices;

  for (size_t i = 0; i < resource->meshes.size(); ++i)
  {
    string &name = resource->meshes[i].name;
    size_t last_dot = name.find_last_of('.');
    std::string name_before_dot = name.substr(0, last_dot);

    bool contains = false;
    for (auto &elem : indices)
    {
      if (elem.first == name_before_dot)
      {
        contains = true;
        break;
      }
    }

    if (!contains)
    {
      Mesh_Index mesh_i = resource_manager->push_mesh(&resource->meshes[i]);
      std::string path = resource->assimp_filename;
      Material_Descriptor material;
      path = path.substr(0, path.find_last_of("/\\")) + "/Textures/";
      if (name_before_dot.find("collide_") != std::string::npos && collision_octree)
      {
        material.emissive.mod = vec4(1.0f, 2.0f, 4.0f, 1.0f);
        material.frag_shader = "emission.frag";
        material.wireframe = true;
        material.backface_culling = false;
        collision_octree->push(&resource->meshes[i]);
      }
      else
      {
        material.albedo = path + name_before_dot + "_albedo.png";
        material.normal = path + name_before_dot + "_normal.png";
        material.roughness = path + name_before_dot + "_roughness.png";
        material.metalness = path + name_before_dot + "_metalness.png";
        material.emissive = path + name_before_dot + "_emissive.png";
        material.ambient_occlusion = path + name_before_dot + "_ao.png";
        material.vertex_shader = "vertex_shader.vert";
        material.frag_shader = "fragment_shader.frag";
      }
      Material_Index mat_i = resource_manager->push_material(&material);
      indices[name_before_dot] = {mesh_i, mat_i};
    }
  }
  return indices;
}

Node_Index Scene_Graph::add_aiscene_new(std::string scene_file_path, std::string name, bool wait_on_resource)
{
  scene_file_path = BASE_MODEL_PATH + scene_file_path;
  Imported_Scene_Data *resource = resource_manager->request_valid_resource(scene_file_path, wait_on_resource);
  if (!resource)
  {
    ASSERT(!wait_on_resource);
    return NODE_NULL;
  }

  pair<Mesh_Index, Material_Index> base_indices = {
      resource_manager->current_mesh_pool_size, resource_manager->current_material_pool_size};

  for (uint32 i = 0; i < resource->materials.size(); ++i)
  {
    Material_Index mat_i = resource_manager->push_material(&resource->materials[i]);
  }
  for (uint32 i = 0; i < resource->meshes.size(); ++i)
  {
    Mesh_Index mesh_i = resource_manager->push_mesh(&resource->meshes[i]);
  }

  // these indices should be the same for all nodes of this import
  uint32 animation_set_index = NODE_NULL;
  uint32 model_bone_set_index = NODE_NULL;
  uint32 animation_state_pool_index = NODE_NULL;
  if (resource->animations.size())
  {
    animation_set_index = resource_manager->push_animation_set(&resource->animations);
    model_bone_set_index = resource_manager->push_bone_set(&resource->all_imported_bones);
    animation_state_pool_index = resource_manager->push_animation_state();
    Skeletal_Animation_State *anim = &resource_manager->animation_state_pool[animation_state_pool_index];
    anim->animation_set_index = animation_set_index;
    anim->model_bone_set_index = model_bone_set_index;
  }

  Node_Index root_for_import =
      add_import_node(resource, &resource->root_node, base_indices, animation_state_pool_index);
  Scene_Graph_Node *root_node = &nodes[root_for_import];

  root_node->scale = float32(resource->scale_factor) * root_node->scale;
  root_node->filename_of_import = scene_file_path;

  // still needed or did we just miss this the whole time in the root node itself?
  const float32 cm_to_meters = 0.01f;
  root_node->scale = cm_to_meters * root_node->scale;

  return root_for_import;
}

Node_Index Scene_Graph::add_import_node(Imported_Scene_Data *scene, Imported_Scene_Node *import_node,
    const pair<Mesh_Index, Material_Index> &base_indices, uint32 animation_state_pool_index)
{
  Node_Index node_index = new_node();
  Scene_Graph_Node *node = &nodes[node_index];
  node->name = import_node->name;
  vec3 skew;
  vec4 perspective;
  bool b = decompose(import_node->transform, node->scale, node->orientation, node->position, skew, perspective);
  node->orientation = conjugate(node->orientation);

  const size_t number_of_mesh_indices = import_node->mesh_indices.size();
  const size_t number_of_material_indices = import_node->material_indices.size();
  const auto &[mesh_offset, material_offset] = base_indices;

  if (animation_state_pool_index != NODE_NULL)
  { // our import had some bones - theyre all in resource_manager->model_bone_set_pool
    node->animation_state_pool_index = animation_state_pool_index;

    uint32 bone_set_index = resource_manager->animation_state_pool[animation_state_pool_index].model_bone_set_index;

    // if we find our name in the pool, we're a bone
    if (resource_manager->model_bone_set_pool[bone_set_index].bones.contains(s(node->name)))
    {
      node->is_a_bone = true;
    }
  }

  // just gathering the mesh and material indices
  for (size_t i = 0; i < number_of_mesh_indices; ++i)
  {
    Mesh_Index mesh_index = mesh_offset + import_node->mesh_indices[i];
    Material_Index material_index = NODE_NULL;
    if (i < number_of_material_indices)
    { // some meshes might not have a material
      material_index = material_offset + import_node->material_indices[i];
    }

    // we dont really have anywhere to put the mesh name, do we need it?
    // std::string mesh_name = scene->meshes[import_node->mesh_indices[i]].name;
    node->model[i] = {mesh_index, material_index};
  }

  const size_t number_of_children = import_node->children.size();
  for (size_t i = 0; i < number_of_children; ++i)
  {
    Imported_Scene_Node *child_node = &import_node->children[i];
    Node_Index child_index = add_import_node(scene, child_node, base_indices, animation_state_pool_index);
    set_parent(child_index, node_index);
    Scene_Graph_Node *child_ptr = &nodes[child_index];
    bool child_is_collider = strncmp(&child_ptr->name.str[0], "collide_", 8) == 0;
    if (child_is_collider)
    {
      child_ptr->visible = false;
      child_ptr->propagate_visibility = false;

      // only working for a single colliding node
      node->collider = child_index;
    }
  }
  return node_index;
}

Node_Index Scene_Graph::new_node()
{
  for (uint32 i = 0; i < nodes.size(); ++i)
  {
    if (nodes[i].exists == false)
    {
      nodes[i] = Scene_Graph_Node();
      nodes[i].exists = true;
      set_message("Scene_Graph allocating new node:", s(i), 1.0f);
      if (highest_allocated_node < i)
      {
        highest_allocated_node = i;
      }
      return i;
    }
  }
  ASSERT(0); // graph full
  return NODE_NULL;
}

Node_Index Scene_Graph::new_node(std::string name, std::pair<Mesh_Index, Material_Index> model0, Node_Index parent)
{
  Node_Index node = new_node();
  nodes[node].name = name;
  nodes[node].model[0] = model0;
  set_parent(node, parent);
  return node;
}

// todo: accelerate these with spatial partitioning

Node_Index Scene_Graph::ray_intersects_node(vec3 p, vec3 dir, Node_Index node, vec3 &result)
{
  if (!ray_intersects_node_aabb(p, dir, node))
  {
    return NODE_NULL;
  }
  vec3 closest_intersection = vec3(99999999999);
  float32 length_of_closest_intersection = length(closest_intersection);
  Scene_Graph_Node *node_ptr = &nodes[node];
  mat4 model_to_world = build_transformation(node);
  mat4 world_to_node_model = inverse(model_to_world);
  p = world_to_node_model * vec4(p, 1);
  dir = normalize(world_to_node_model * vec4(dir, 0));

  for (auto &n : node_ptr->model)
  {
    Mesh_Index mesh_index = n.first;
    if (mesh_index != NODE_NULL)
    {
      Mesh_Descriptor *md_ptr = &resource_manager->mesh_pool[mesh_index].mesh->descriptor;
      ASSERT(md_ptr);
      Mesh_Data &mesh_data = md_ptr->mesh_data;
      for (size_t i = 0; i < mesh_data.indices.size(); i += 3)
      {
        vec3 &a = mesh_data.positions[mesh_data.indices[i]];
        vec3 &b = mesh_data.positions[mesh_data.indices[i + 1]];
        vec3 &c = mesh_data.positions[mesh_data.indices[i + 2]];

        vec3 ta = model_to_world * vec4(a, 1);
        vec3 tb = model_to_world * vec4(b, 1);
        vec3 tc = model_to_world * vec4(c, 1);

        vec3 intersection;
        bool intersects = ray_intersects_triangle(p, dir, a, b, c, &intersection);
        if (intersects)
        {
          float32 dist = length(intersection - p);
          if (dist < length_of_closest_intersection)
          {
            closest_intersection = intersection;
            length_of_closest_intersection = dist;
          }
        }
      }
    }
    result = model_to_world * vec4(closest_intersection, 1);
    if (closest_intersection != vec3(99999999999))
    {
      return node;
    }
    return NODE_NULL;
  }
}
Node_Index Scene_Graph::find_by_name(Node_Index parent, const char *name)
{
  bool warned = false;
  if (parent == NODE_NULL)
  {
    for (uint32 i = 0; i <= highest_allocated_node; ++i)
    {
      if ((i > 300) && (!warned))
      {
        warned = true;
        set_message("perf warning in find_by_name(), this gets slow with lots of nodes", "", 10.f);
      }

      Scene_Graph_Node *ptr = &nodes[i];

      if (ptr->parent != NODE_NULL)
        continue;

      if (ptr->name == name)
      {
        return Node_Index(i);
      }
    }
    return NODE_NULL;
  }

  Scene_Graph_Node *ptr = &nodes[parent];
  for (uint32 i = 0; i < ptr->children.size(); ++i)
  {
    Node_Index child = ptr->children[i];
    if (child != NODE_NULL)
    {
      Scene_Graph_Node *cptr = &nodes[child];
      if (cptr->name == Array_String(name))
      {
        return child;
      }
    }
  }
  return NODE_NULL;
}
void Scene_Graph::grab(Node_Index grabber, Node_Index grabee)
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
  vec3 skew;
  vec4 perspective;
  decompose(M_to_Mchild, scale, orientation, translation, skew, perspective);
  orientation = conjugate(orientation);
  set_parent(grabee, grabber);
  nodes[grabee].position = translation;
  nodes[grabee].scale = scale;
  nodes[grabee].orientation = orientation;
  // nodes[grabee].import_basis = mat4(1);
}
void Scene_Graph::drop(Node_Index child)
{
  mat4 M = build_transformation(child);
  vec3 scale;
  quat orientation;
  vec3 translation;
  vec3 skew;
  vec4 perspective;
  decompose(M, scale, orientation, translation, skew, perspective);
  orientation = conjugate(orientation);

  set_parent(child);
  nodes[child].position = translation;
  nodes[child].scale = scale;
  nodes[child].orientation = orientation;
  // nodes[child].import_basis = mat4(1);
}

void Scene_Graph::delete_node(Node_Index i)
{
  if (i == NODE_NULL)
    return;
  nodes[i] = Scene_Graph_Node();
  for (Node_Index &child : nodes[i].children)
  {
    delete_node(child);
  }
}

void Scene_Graph::set_parent(Node_Index i, Node_Index desired_parent)
{
  ASSERT(i != NODE_NULL);
  Scene_Graph_Node *node = &nodes[i];
  Node_Index current_parent_index = node->parent;

  if (current_parent_index != NODE_NULL)
  {
    bool found_i = false;
    Scene_Graph_Node *current_parent = &nodes[current_parent_index];
    for (auto &child : current_parent->children)
    {
      if (child == i)
      {
        found_i = true;
        child = NODE_NULL;
        break;
      }
    }
    ASSERT(found_i);
  }
  if (desired_parent != NODE_NULL)
  {
    bool found_new_slot = false;
    Scene_Graph_Node *new_parent = &nodes[desired_parent];
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
  node->parent = desired_parent;

  // iterate over all nodes and verify that only the desired parent has this node as a child
  bool found_the_valid_child_node = false;
  for (uint32 j = 0; j < nodes.size(); ++j)
  {
    bool is_desired_parent = (j == desired_parent);
    for (auto &child : nodes[j].children)
    {
      bool is_this_node = child == i;

      if (is_this_node)
      {
        if (found_the_valid_child_node)
        { // this is the valid parent but it has two of us
          ASSERT(0);
        }

        if (is_desired_parent)
          found_the_valid_child_node = true;

        if (!is_desired_parent)
        { // this parent should not have this child - unless this behavior is intended - it is useful
          ASSERT(0);
        }
      }
    }
  }
}

std::vector<Render_Entity> Scene_Graph::visit_nodes_start()
{
  accumulator.clear();
  for (uint32 i = 0; i < nodes.size(); ++i)
  {
    Scene_Graph_Node *node = &nodes[i];
    if (!node->exists)
      continue;
    if (node->parent == NODE_NULL)
    { // node is visible and its parent is the null node, add its branch

      // sponge clenaup
      mat4 inverse_root;
      mat4 global_parent;
      if (node->is_root_of_wow_import)
      {
        inverse_root = inverse(__build_transformation(i)); // *get_wow_asset_import_transformation());
        // global_parent = get_wow_asset_import_transformation();
        global_parent = mat4(1);
      }
      else
      {
        inverse_root = inverse(__build_transformation(i));
        global_parent = mat4(1);
      }

      visit_nodes(i, global_parent, inverse_root, accumulator);
    }
  }

  return accumulator;
}

glm::mat4 Scene_Graph::__build_transformation(Node_Index node_index)
{
  if (node_index == NODE_NULL)
    return mat4(1);

  Scene_Graph_Node *node = &nodes[node_index];
  Node_Index parent = node->parent;

  mat4 M = __build_transformation(parent);
  const mat4 T = translate(node->position);
  const mat4 S_prop = scale(node->scale);
  const mat4 S_non = scale(node->scale_vertex);
  const mat4 R = toMat4(node->orientation);
  // const mat4 B = node->import_basis;
  mat4 STACK = M * T * R * S_prop;

  if (node->is_root_of_wow_import)
  {
    // STACK = STACK * get_wow_asset_import_transformation();
  }
  return STACK;
}

// use visit nodes if you need the transforms of all objects in the graph
glm::mat4 Scene_Graph::build_transformation(Node_Index node_index, bool use_vertex_scale)
{
  ASSERT(node_index != NODE_NULL);

  Scene_Graph_Node *node = &nodes[node_index];
  Node_Index parent = node->parent;

  mat4 M = __build_transformation(parent);
  const mat4 T = translate(node->position);
  const mat4 S_o = scale(node->oriented_scale);
  const mat4 S_prop = scale(node->scale);
  const mat4 S_non = scale(node->scale_vertex);
  const mat4 R = toMat4(node->orientation);
  // const mat4 B = node->import_basis;
  // this node specifically
  mat4 BASIS = M * T * S_o * R * S_prop;

  if (use_vertex_scale)
  {
    BASIS = BASIS * S_non;
  }
  return BASIS;
}
std::string Scene_Graph::serialize() const
{
  std::string result;

  return result;
}
void Scene_Graph::deserialize(std::string src) {}

void Scene_Graph::initialize_lighting(std::string radiance, std::string irradiance, bool generate_light_spheres)
{
  if (generate_light_spheres)
  {
    Node_Index root_for_lights = new_node();
    Scene_Graph_Node *node_ptr = &nodes[root_for_lights];
    node_ptr->exists = true;
    node_ptr->name = "Root for lights";

    for (uint32 i = 0; i < MAX_LIGHTS; ++i)
    {
      Material_Descriptor material;
      material.casts_shadows = false;
      material.albedo.mod = vec4(0, 0, 0, 1);
      material.roughness.mod = vec4(1);
      Material_Index mi = resource_manager->push_material(&material);

      Node_Index temp = add_aiscene_new("sphere-2.fbx", s("Light", i));
      Node_Index actual_model = nodes[temp].children[0];
      set_parent(actual_model, root_for_lights);
      delete_node(temp);
      lights.light_spheres[i] = actual_model;
      nodes[actual_model].model[0].second = mi;
    }
  }
  lights.environment.radiance = radiance;
  lights.environment.irradiance = irradiance;
}

void Scene_Graph::set_lights_for_renderer(Renderer *r)
{
  // make a button that calculates the shadow settings using the distance to point and the angle

  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    Light *light = &lights.lights[i];
    Node_Index node = lights.light_spheres[i];
    glm::clamp(light->color, vec3(0.0), vec3(1.0));
    if (node == NODE_NULL)
      continue;

    Scene_Graph_Node *node_ptr = &nodes[node];
    node_ptr->position = light->position;
    node_ptr->scale = vec3(light->radius);
    if (!(i < lights.light_count))
    {
      node_ptr->visible = false;
    }
    else
    {
      node_ptr->visible = true;
    }

    Material_Descriptor *md = get_modifiable_material_pointer_for(node, 0);
    md->emissive.source = "white";
    vec3 c = light->brightness * light->color;
    float mod = 0.03f;
    md->emissive.mod = vec4(mod * c, 0);
  }
  r->lights = lights;

  if (r->environment.m.radiance != lights.environment.radiance ||
      r->environment.m.irradiance != lights.environment.irradiance)
  {
    r->environment = lights.environment;
    r->environment.load();
  }
}
void Scene_Graph::push_particle_emitters_for_renderer(Renderer *r)
{
  for (auto &emitter : particle_emitters)
  {
    emitter.init();
    if (emitter.prepare_instance(&r->render_instances))
    {
      Render_Instance *i = &r->render_instances.back();
      i->mesh = &resource_manager->mesh_pool[emitter.mesh_index];
      i->material = &resource_manager->material_pool[emitter.material_index];
    }
  }
}

// we should be guaranteed that all bones above us are already computed
// for this frame in the calculated_bone_data vector...
mat4 animation_resolve(Skeletal_Animation_State *anim_state, Skeletal_Animation_Set *anim_set, Bone *bone)
{
  const string_view bone_name = bone->name;
  Skeletal_Animation *selected_animation = &anim_set->animation_set[anim_state->currently_playing_animation];

  Bone_Animation const *bone_animation = nullptr;

  // find bone in animation by name of bone we want to compute

  const size_t num_of_animated_bones = selected_animation->bone_animations.size();
  for (size_t i = 0; i < num_of_animated_bones; ++i)
  {
    const Bone_Animation *bone_anim = &selected_animation->bone_animations[i];
    const string_view test_bone_name = bone_anim->bone_node_name;
    if (test_bone_name == bone_name)
    {
      bone_animation = bone_anim;
      break;
    }
  }

  // all the bone names ARE imported - just that some of them done have channels(animations)
  // so we traversed the node tree of the import anyway to gather their default transformations for the bones
  //(remember how we were completely overwriting the RST matrix with the transformation result?)
  // well, for the nodes that dont have channels, we also had no default transformation data, now we do
  // hopefully this fixes it

  // if it does, we should probably put the default transform into the bone animation struct
  // so that its a bit cleaner, each bone would have a bone animation struct mapped to it
  // just - some would have no keyframes, and just a pose to apply

  if (!bone_animation)
  {
    mat4 t = selected_animation->default_pose_for_nodes[string(bone_name)];
    return t;
    set_message(bone->name, "Bone not found in animation", 10.f);
    set_message(s("animation_resolve for bone:\n", bone->name, "\n"), "Bone not found in animation", 10.f);
  }

  float32 ticks_per_sec = selected_animation->ticks_per_sec != 0 ? selected_animation->ticks_per_sec : 30;
  float32 time_in_ticks = 1000.f * anim_state->time * ticks_per_sec;

  bool custom_pose = anim_state->bone_pose_overrides.contains(bone->name);
  if (custom_pose)
  {
    time_in_ticks = 1000.f * anim_state->bone_pose_overrides[bone->name] * ticks_per_sec;
  }

  float32 animation_time = fmod(time_in_ticks, selected_animation->duration);

  if (bone->name.substr(bone->name.size() - 3) == "_13")
  {
    /*
    bugged anim:
    knockdown[53]
    bone _13
    pose time  .654
    */

    if (animation_time > .65 && animation_time < .66)
    {
      int a = 3;
    }
  }

  size_t left_i = 0;
  size_t right_i = 0;

  for (size_t i = 0; i < bone_animation->timestamp.size() - 1; ++i)
  {
    left_i = i;
    float32 next_time = bone_animation->timestamp[i + 1];
    if (animation_time < next_time)
    {
      right_i = i + 1;
      break;
    }
  }
  const float64 left_time = bone_animation->timestamp[left_i];
  const float64 right_time = bone_animation->timestamp[right_i];

  float64 t = animation_time - left_time;
  const float64 max_time = right_time - left_time;
  t = t / max_time;

  if (selected_animation->duration == 0)
  {
    t = 0;
  }

  // ASSERT(t >= 0.f || t <= 1.0f);
  if (t < 0.f || t > 1.0f)
  {
    t = 0; // sponge: assert false
  }

  // component results
  const vec3 translation_result = mix(bone_animation->translations[left_i], bone_animation->translations[right_i], t);
  const vec3 scale_result = mix(bone_animation->scales[left_i], bone_animation->scales[right_i], t);
  const quat rotation_result = lerp(bone_animation->rotations[left_i], bone_animation->rotations[right_i], float32(t));

  // result in bone space
  mat4 transformed_bone = translate(translation_result) * toMat4(rotation_result) * scale(scale_result);

  return transformed_bone;
}

void Scene_Graph::set_wow_asset_import_transformation(Node_Index ni)
{
  Scene_Graph_Node *node = &nodes[ni];
  // node->is_root_of_wow_import = true;

  // sponge match better here or do it on import
  // cleanup the member
  node = &nodes[node->children[0]];

  quat a = angleAxis(half_pi<float32>(), vec3(1, 0, 0));
  quat b = angleAxis(half_pi<float32>(), vec3(0, 0, 1));

  node->orientation = b * a;
  node->scale = vec3(.017);
  // static mat4 result = toMat4(a) * toMat4(b) * scale(vec3(.13f));
}
mat4 get_wow_asset_import_transformation()
{
  static quat a = angleAxis(-half_pi<float32>(), vec3(1, 0, 0));
  static quat b = angleAxis(-half_pi<float32>(), vec3(0, 0, 1));
  static mat4 result = toMat4(a) * toMat4(b) * scale(vec3(.13f));
  return result;
}

// this should not be a problem for child node models of a bone, because we modify the stack
// as we go down the tree, so it will always be correct for children
void Scene_Graph::visit_nodes(
    Node_Index node_index, const mat4 &Parent, mat4 &INVERSE_ROOT, std::vector<Render_Entity> &accumulator)
{
  if (node_index == NODE_NULL)
    return;
  Scene_Graph_Node *entity = &nodes[node_index];
  if (!entity->exists)
    return;
  if ((!entity->visible) && entity->propagate_visibility && entity->animation_state_pool_index == NODE_NULL)
    return;
  assert_valid_parent_ptr(node_index);

  const mat4 T = translate(entity->position);
  const mat4 S_o = scale(entity->oriented_scale);
  const mat4 S_prop = scale(entity->scale);
  // const mat4 S_non = scale(entity->scale_vertex);
  const mat4 R = toMat4(entity->orientation);
  // const mat4 B = entity->import_basis;

  // this node's transform alone
  mat4 TSRS = T * S_o * R * S_prop;

  Skeletal_Animation_State *anim_state;
  Skeletal_Animation_Set *anim_set;
  Model_Bone_Set *bone_set;
  Bone *bone;

  if (entity->is_a_bone)
  {
    // the current user-set state of the animation
    anim_state = &resource_manager->animation_state_pool[entity->animation_state_pool_index];

    // the set of animations and their keyframe data
    anim_set = &resource_manager->animation_set_pool[anim_state->animation_set_index];

    // find our bone
    uint32 model_bone_set_pool_index = anim_state->model_bone_set_index;
    bone_set = &resource_manager->model_bone_set_pool[model_bone_set_pool_index];
    ASSERT(bone_set->bones.contains(s(entity->name)));
    bone = &bone_set->bones[s(entity->name)];

    // pose it in bone space
    const mat4 posed_bone = animation_resolve(anim_state, anim_set, bone);

    if (posed_bone != mat4(1))
    {
      // this node's bone, posed only in bone space
      TSRS = posed_bone;
    }
    else
    {
      // ASSERT(0);
    }
  }

  mat4 PTSRS = Parent * TSRS;

  if (entity->is_a_bone)
  {
    // data that is bound in the uniform array in the vertex shader
    // the shader expects no transforms by the root node, aka model matrix - but the PTSRS has it inside
    // the P for parent
    // so the inverse root must be the complete inverse of the transformation of the root node
    // to this node
    anim_state->final_bone_transforms[s(entity->name)] = INVERSE_ROOT * PTSRS * bone->offset;
  }

  // the matrix to be supplied to the 'Model' uniform in the vertex shader if this node has meshes
  const mat4 BASIS = PTSRS; // *S_non;

  const size_t num_meshes = entity->model.size();
  for (size_t i = 0; i < num_meshes; ++i)
  {
    Mesh_Index mesh_index = entity->model[i].first;
    Material_Index material_index = entity->model[i].second;
    if (mesh_index == NODE_NULL)
      continue;
    Mesh *mesh_ptr = nullptr;
    Material *material_ptr = nullptr;
    Skeletal_Animation_State *animation_ptr = nullptr;
    string assimp_filename = s(entity->filename_of_import);
    mesh_ptr = &resource_manager->mesh_pool[mesh_index];

    if (material_index == NODE_NULL)
    {
      material_ptr = &resource_manager->default_material;
    }
    else
    {
      material_ptr = &resource_manager->material_pool[material_index];
    }

    if (entity->animation_state_pool_index != NODE_NULL)
    {
      animation_ptr = &resource_manager->animation_state_pool[entity->animation_state_pool_index];
    }
    if (entity->visible)
      accumulator.emplace_back(entity->name, mesh_ptr, material_ptr, animation_ptr, BASIS, node_index);
  }

  if (false && entity->is_a_bone)
  {
    static Mesh bone_cube = Mesh_Descriptor(cube, "some bone");
    static Material bone_mat;

    accumulator.emplace_back(entity->name, &bone_cube, &bone_mat, nullptr, BASIS, node_index);
  }

  for (uint32 i = 0; i < entity->children.size(); ++i)
  {
    Node_Index child = entity->children[i];
    visit_nodes(child, PTSRS, INVERSE_ROOT, accumulator);
  }
}

void Scene_Graph::assert_valid_parent_ptr(Node_Index node_index)
{
  Scene_Graph_Node *entity = &nodes[node_index];
  if (entity->parent == NODE_NULL)
    return;
  Scene_Graph_Node *parent = &nodes[entity->parent];
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

static mutex import_lock;
static vector<Imported_Scene_Data *> import_queue;

static condition_variable import_thread_cv;
void import_thread_loop()
{
  while (true)
  {
    std::vector<Imported_Scene_Data *> arg_v;
    {
      unique_lock<mutex> hold(import_lock);
      import_thread_cv.wait(hold);
      if (import_queue.size() == 0)
      {
        continue;
      }
      arg_v.push_back(import_queue.back());
      import_queue.pop_back();
    }

    for (auto it = arg_v.rbegin(); it != arg_v.rend(); ++it)
    {
      Resource_Manager::import_aiscene_new(*it);
    }
  }
}

bool Resource_Manager::import_aiscene_async(Imported_Scene_Data *dst)
{
  ASSERT(0);

  //*@note One Importer instance is not thread - safe.If you use multiple
  //  * threads for loading, each thread should maintain its own Importer instance.

  ASSERT(dst->valid != true);
  if (!thread_active)
  {
    import_thread = thread(import_thread_loop);
    thread_active = true;
  }
  lock_guard<mutex> hold(import_lock);
  import_queue.emplace_back(dst);
  import_thread_cv.notify_one();
  return false;
}

Imported_Scene_Node Resource_Manager::_import_aiscene_node(
    Imported_Scene_Data *data, const aiScene *scene, const aiNode *ainode)
{
  Imported_Scene_Node node;
  node.name = copy(&ainode->mName);

  if (node.name == "" && ainode->mNumChildren == 1 && ainode->mNumMeshes == 0)
  { // blank node, skip it
    return _import_aiscene_node(data, scene, ainode->mChildren[0]);
  }

  for (uint32 i = 0; i < ainode->mNumMeshes; ++i)
  {
    uint32 mesh_index = ainode->mMeshes[i];
    const aiMesh *aimesh = scene->mMeshes[mesh_index];

    Mesh_Index mesh_result{mesh_index};
    node.mesh_indices.push_back(mesh_result);

    Material_Index mi = aimesh->mMaterialIndex;
    node.material_indices.push_back(mi);
  }

  node.transform = copy(ainode->mTransformation);

  for (uint32 i = 0; i < ainode->mNumChildren; ++i)
  {
    const aiNode *n = ainode->mChildren[i];
    Imported_Scene_Node child = _import_aiscene_node(data, scene, n);
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

  if (name.length > 10000)
    return {};
  return copy(&name);
}

bool set_texture_from_ai_type(Texture_Descriptor *t, string_view dir, aiMaterial *ai_material, aiTextureType type)
{
  ASSERT(ai_material);
  const int count = ai_material->GetTextureCount(type);
  aiString name;
  ai_material->GetTexture(type, 0, &name);
  std::string filename = fix_filename(copy(&name));
  if (filename == "." || filename == "")
  {
    return false;
  }
  t->source = s(dir, "/", filename, ".png");
  return true;
}

#if 0
/*
Algorithm: recurse depth first and work back up
if the node is empty but has children, the node concatenates the matrices and
sends the children to the parent's temp buffer and the parent appends them, causing the node to have no children
if the node is empty with no children, the parent deletes it
*/
void Resource_Manager::propagate_transformations_of_empty_nodes(
    Imported_Scene_Node *this_node, std::vector<Imported_Scene_Node> *temp)
{
  // if parent is empty
  // this child can apply the parent's transform to itself
  // and then move itself into the parent

  const bool this_node_has_no_meshes = this_node->mesh_indices.size() == 0;
  std::vector<Imported_Scene_Node> temp_new_children;
  for (uint32 i = 0; i < this_node->children.size(); ++i)
  {
    Imported_Scene_Node *child = &this_node->children[i];
    propagate_transformations_of_empty_nodes(child, &temp_new_children);

    // now this child should be in one of 3 states:
    // meshes and children
    // meshes and no children
    // no meshes and no childrn

    // deleting a node in the middle of a loop over them: 0 1 X 3 4 -> 0 1 4 3
    // if this child has no meshes and no children theres no reason to keep it
    if (child->mesh_indices.size() == 0)
    {
      ASSERT(child->children.size() == 0);
      uint32 last_i = this_node->children.size() - 1;
      bool last = (i == last_i);
      if (!last)
        this_node->children[i] = std::move(this_node->children[last]);
      this_node->children.pop_back();
      i -= 1;
    }
  }
  // all of the children have returned, we can now append any new children we've recieved
  std::move(std::begin(temp_new_children), std::end(temp_new_children), std::back_inserter(this_node->children));

  if (this_node_has_no_meshes)
  { // concatenate transformations and move all children into parent's children
    for (uint32 i = 0; i < this_node->children.size(); ++i)
    {
      Imported_Scene_Node *child = &this_node->children[i];
      child->transform = child->transform * this_node->transform;
      temp->push_back(std::move(*child));
    }
    this_node->children.clear();
    // this node should now have both no meshes and no children and will
    // get deleted by the higher level recursion
  }
}

#endif
//
// void gather_animation_node_mapping(Imported_Scene_Data* data)
//{
//  _gather_animation_node_mapping(&data->root_node, result);
//};
//
//
// std::sort(result.begin(), result.end(), [](animdatapack &l, animdatapack &r) {
//  string &a = l.name;
//
//  uint32 a_num = 0;
//  for (size_t i = 0; i < a.size(); ++i)
//  {
//    const char *c = &a[i];
//    if (!isdigit(*c))
//    {
//      continue;
//    }
//
//    a_num = atoi(c);
//    break;
//  }
//
//  string &b = r.name;
//
//  uint32 b_num = 0;
//  for (size_t i = 0; i < b.size(); ++i)
//  {
//    const char *c = &b[i];
//    if (!isdigit(*c))
//    {
//      continue;
//    }
//    b_num = atoi(c);
//    break;
//  }
//
//  return a_num < b_num;
//});
// return result;
//}

/*


we should gather all the node names in the animations
and then we should do a map for the name -> its default transformation

i bet the nodes that are missing from the transformation, have a transformation matrix that should be its default
we should always find and apply the nodes transform, even if the animation doesnt refer to/animate the node
//this is why the face is always screwy too, all the animations leave it alone




if the node isnt in the channels list




*/

void gather_animation_node_name_transform_mapping(Imported_Scene_Node *node, Skeletal_Animation *anim)
{
  anim->default_pose_for_nodes[node->name] = node->transform;
  for (auto &e : node->children)
  {
    gather_animation_node_name_transform_mapping(&e, anim);
  }
}

void gather_animations(const aiScene *scene, Imported_Scene_Data *dst)
{
  for (uint32 i = 0; i < scene->mNumAnimations; ++i)
  {
    aiAnimation *anim = scene->mAnimations[i];
    aiString *animation_name = &anim->mName;
    float64 duration = anim->mDuration;
    float64 tickspersec = anim->mTicksPerSecond;

    Skeletal_Animation animation;
    animation.animation_name = copy(animation_name);
    animation.duration = duration;
    animation.ticks_per_sec = tickspersec;
    gather_animation_node_name_transform_mapping(&dst->root_node, &animation);

    // a channel is a bone
    // bones not found here are not animated and simply use their mtransformation matrix
    uint32 bone_count = anim->mNumChannels;
    animation.bone_animations.reserve(bone_count);
    ASSERT(bone_count < MAX_BONES);

    set_message(s("Bone count for:", dst->assimp_filename), s(bone_count), 3000.f);
    for (uint32 i = 0; i < bone_count; ++i)
    {
      aiNodeAnim *nodeanim = anim->mChannels[i];

      aiString *bone_node_name = &nodeanim->mNodeName;
      uint32 num_pos_keys = nodeanim->mNumPositionKeys;
      aiVectorKey *pos_keys = nodeanim->mPositionKeys;

      uint32 num_rot_keys = nodeanim->mNumRotationKeys;
      aiQuatKey *quat_keys = nodeanim->mRotationKeys;

      uint32 num_scale_keys = nodeanim->mNumScalingKeys;
      aiVectorKey *scale_keys = nodeanim->mScalingKeys;

      // aiAnimBehaviour *prestate = &nodeanim->mPreState;
      // aiAnimBehaviour *poststate = &nodeanim->mPostState;

      ASSERT(num_pos_keys == num_rot_keys);
      ASSERT(num_pos_keys == num_scale_keys);

      uint32 keyframe_count = num_pos_keys;

      Bone_Animation bone_animation;
      bone_animation.bone_node_name = copy(bone_node_name);
      bone_animation.translations.reserve(keyframe_count);
      bone_animation.scales.reserve(keyframe_count);
      bone_animation.rotations.reserve(keyframe_count);
      bone_animation.timestamp.reserve(keyframe_count);
      for (uint32 j = 0; j < keyframe_count; ++j)
      {
        bone_animation.translations.push_back(copy(pos_keys[j].mValue));
        bone_animation.scales.push_back(copy(scale_keys[j].mValue));
        bone_animation.rotations.push_back(copy(quat_keys[j].mValue));
        float64 pos_time = pos_keys[j].mTime;
        float64 scale_time = scale_keys[j].mTime;
        float64 rot_time = quat_keys[j].mTime;
        ASSERT(scale_time == pos_time);
        ASSERT(rot_time == pos_time);
        bone_animation.timestamp.push_back(pos_time);
      }
      animation.bone_animations.push_back(bone_animation);
    }
    dst->animations.push_back(animation);
  }
}

void assert_valid_aimesh(const aiMesh *aimesh, const aiScene *scene)
{
  ASSERT(aimesh);
  ASSERT(aimesh->HasNormals());
  ASSERT(aimesh->HasPositions());
  ASSERT(aimesh->GetNumUVChannels() == 1);
  ASSERT(aimesh->HasTextureCoords(0));
  ASSERT(aimesh->HasTangentsAndBitangents());
  ASSERT(aimesh->HasVertexColors(0) == false); // TODO: add support for vertex colors
  ASSERT(aimesh->mNumUVComponents[0] == 2);
}

void gather_mesh_bones(const aiMesh *aimesh, Mesh_Descriptor *d)
{
  vector<Bone> *mesh_specific_bones = &d->mesh_specific_bones;
  const size_t vertex_count = d->mesh_data.positions.size();
  d->mesh_data.bone_weights = vector<Vertex_Bone_Data>(vertex_count);

  // ASSERT(aimesh->mNumBones < MAX_BONES);
  if (aimesh->mNumBones > MAX_BONES)
  {
    int a = 3;
  }

  // all the bones this mesh is affected by
  // actually no, this is all the bones in the entire model for
  // wow asset imports
  // likely exported by wmv like this, if we process the bones with assimp
  // it culls them, which we should be doing now

  // not all the bones in the scenegraph will show up here
  // there can exist bones that do not affect any
  // vertices directly - only transform other bones
  for (uint32 i = 0; i < aimesh->mNumBones; ++i)
  {
    const aiBone *aibone = aimesh->mBones[i];

    uint32 num_of_vertices_affected_by_this_bone = aibone->mNumWeights;
    if (num_of_vertices_affected_by_this_bone == 0)
      continue;

    const string bone_name = aibone->mName.data;
    const mat4 offsetmatrix = copy(aibone->mOffsetMatrix);

    mesh_specific_bones->emplace_back();
    mesh_specific_bones->back().name = aibone->mName.data;
    mesh_specific_bones->back().offset = copy(aibone->mOffsetMatrix);
    int32 mesh_specific_shader_bone_index = int32(mesh_specific_bones->size()) - 1;

    for (uint32 j = 0; j < num_of_vertices_affected_by_this_bone; ++j)
    {
      const aiVertexWeight weight = aibone->mWeights[j];
      const uint32 vertex_index = weight.mVertexId;

      for (uint32 k = 0; k < MAX_BONES_PER_VERTEX; ++k)
      {
        if (d->mesh_data.bone_weights[vertex_index].weights[k] == 0.0f)
        {
          d->mesh_data.bone_weights[vertex_index].indices[k] = mesh_specific_shader_bone_index;
          d->mesh_data.bone_weights[vertex_index].weights[k] = weight.mWeight;
          break;
        }
        ASSERT(k != 3);
      }
    }

#ifndef NDEBUG
    for (Vertex_Bone_Data &e : d->mesh_data.bone_weights)
    {
      float32 sum = 0.f;
      for (uint32 i = 0; i < 4; ++i)
      {
        sum += e.weights[i];
      }
      if (sum > 1.1f || sum < -0.1f)
      {
        ASSERT(0);
      }
    }
#endif
  }
}

void gather_mesh_indices(std::vector<uint32> &indices, const aiMesh *aimesh)
{
  for (uint32 i = 0; i < aimesh->mNumFaces; i++)
  {
    aiFace face = aimesh->mFaces[i];
    if (face.mNumIndices == 3)
    {
      for (GLuint j = 0; j < 3; j++)
      {
        indices.push_back(face.mIndices[j]);
      }
    }
  }
  if (indices.size() < 3)
  {
    set_message("Error: No triangles in mesh:", aimesh->mName.data);
    ASSERT(0); // will fail to upload
  }
}

void gather_meshes(const aiScene *scene, Imported_Scene_Data *dst)
{
  for (uint32 i = 0; i < scene->mNumMeshes; ++i)
  {
    const aiMesh *aimesh = scene->mMeshes[i];
    assert_valid_aimesh(aimesh, scene);
    dst->meshes.emplace_back(Mesh_Descriptor{});
    Mesh_Descriptor &d = dst->meshes.back();

    d.name = aimesh->mName.data;
    const int32 num_vertices = aimesh->mNumVertices;
    copy_mesh_data(d.mesh_data.positions, aimesh->mVertices, num_vertices);
    copy_mesh_data(d.mesh_data.normals, aimesh->mNormals, num_vertices);
    copy_mesh_data(d.mesh_data.tangents, aimesh->mTangents, num_vertices);
    copy_mesh_data(d.mesh_data.bitangents, aimesh->mBitangents, num_vertices);
    copy_mesh_data(d.mesh_data.texture_coordinates, aimesh->mTextureCoords[0], num_vertices);
    gather_mesh_indices(d.mesh_data.indices, aimesh);
    gather_mesh_bones(aimesh, &d);

    // for (uint32 i = 0; i < aimesh->mNumBones; ++i)
    //{
    //  const aiBone* aibone = aimesh->mBones[i];
    //  Bone bone;
    //  bone.name = aibone->mName.data;
    //  bone.offset = copy(aibone->mOffsetMatrix);

    //  bool found = false;
    //  for (auto& exists : dst->all_imported_bones)
    //  {
    //    Bone& existing_bone = exists.second;
    //    if (existing_bone.name == bone.name)
    //    {
    //      found = true;
    //      mat4& existing_matrix = existing_bone.offset;
    //      mat4& mesh_bone_matrix = bone.offset;
    //      ASSERT(existing_matrix == mesh_bone_matrix);
    //      break;
    //      //if we find matching names with mismatching offsets something is broken...
    //    }
    //  }
    //  if(!found)
    //  dst->all_imported_bones[bone.name] = bone;
    //}

#if 0

    //this wasnt right because the import was giving us every bone for every mesh
    //if we exclude the ones with 0 affected vertices, we dont gather the bone name
    //into the mapping


    // unfortunately this is a copy, there is no easy way to
    // retrieve the bone offset data from within the meshes
    // from the scene graph - the meshes are in an entirely different
    // tree branch even...
    for (size_t i = 0; i < d.mesh_specific_bones.size(); ++i)
    {
      Bone& mesh_bone = d.mesh_specific_bones[i];
      for (auto& exists : dst->all_imported_bones)
      {
        Bone& existing_bone = exists.second;
        if (existing_bone.name == mesh_bone.name)
        {
          mat4& existing_matrix = existing_bone.offset;
          mat4& mesh_bone_matrix = mesh_bone.offset;
          ASSERT(existing_matrix == mesh_bone_matrix);
          //if we find matching names with mismatching offsets something is broken...
        }
      }
      dst->all_imported_bones[d.mesh_specific_bones[i].name] = d.mesh_specific_bones[i];
    }

#endif
  }
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

  // pbr
  string BASE_COLOR = name_from_ai_type(mat, aiTextureType_BASE_COLOR);
  string NORMAL_CAMERA = name_from_ai_type(mat, aiTextureType_NORMAL_CAMERA);
  string EMISSION_COLOR = name_from_ai_type(mat, aiTextureType_EMISSION_COLOR);
  string METALNESS = name_from_ai_type(mat, aiTextureType_METALNESS);
  string DIFFUSE_ROUGHNESS = name_from_ai_type(mat, aiTextureType_DIFFUSE_ROUGHNESS);
  string AMBIENT_OCCLUSION = name_from_ai_type(mat, aiTextureType_AMBIENT_OCCLUSION);

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

Material_Descriptor build_material_descriptor(const aiScene *scene, uint32 i, const std::string_view path)
{
  Material_Descriptor d;
  // find the file extension
  size_t slice = path.find_last_of("/\\");
  string_view dir = path.substr(0, slice);
  slice = path.find_last_of(".");
  string_view extension = path.substr(slice, path.size() - slice);

  aiMaterial *mat = scene->mMaterials[i];
  gather_all_assimp_materials(mat);

  if (!set_texture_from_ai_type(&d.albedo, dir, mat, aiTextureType_BASE_COLOR))
  {
    set_texture_from_ai_type(&d.albedo, dir, mat, aiTextureType_DIFFUSE);
  }
  if (!set_texture_from_ai_type(&d.emissive, dir, mat, aiTextureType_EMISSION_COLOR))
  {
    set_texture_from_ai_type(&d.emissive, dir, mat, aiTextureType_EMISSIVE);
  }
  if (!set_texture_from_ai_type(&d.normal, dir, mat, aiTextureType_NORMAL_CAMERA))
  {
    set_texture_from_ai_type(&d.normal, dir, mat, aiTextureType_NORMALS);
  }
  set_texture_from_ai_type(&d.roughness, dir, mat, aiTextureType_DIFFUSE_ROUGHNESS);
  set_texture_from_ai_type(&d.metalness, dir, mat, aiTextureType_METALNESS);
  set_texture_from_ai_type(&d.ambient_occlusion, dir, mat, aiTextureType_AMBIENT_OCCLUSION);

  if (scene->mNumAnimations != 0)
  {
    d.vertex_shader = "skeletal_animation.vert";
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

/*
how to import anything from blender to warg with pbr materials :

1 : open blender
2 : import your models
3 : set your textures to the following mappings :

warg->          - blender settings

albedo map      ->diffuse   - color
emissive map    ->diffuse   - intensity(new: emission)
normal map      ->geometry  - normal
roughness map   ->specular  - hardness
metalness map   ->specular  - intensity
ambient_occ map ->shading   - mirror

note : if a map is missing, make one
or , leave it blankand it will have a generic default

note : these will look completely wrong for blender - but we dont care about
blender we're just hijacking the slots for our own purpose

4 : save a blender file so you can reload from here
5 : file -> export ->.fbx
6 : enable "Scale all..." to the right of the Scale slider(lighter color)
7 : change Up : to: Z up
8 : change Forward : to: Y forward
9 : geometry tab->check Tangent space
10 : export
*/

void gather_materials(const aiScene *scene, Imported_Scene_Data *dst)
{
  for (size_t i = 0; i < scene->mNumMaterials; ++i)
  {
    dst->materials.emplace_back(build_material_descriptor(scene, i, dst->assimp_filename));
  }
}

void gather_animation_subfolder(Imported_Scene_Data *dst)
{
  ASSERT(dst);
  using namespace std::filesystem;
  path filename = dst->assimp_filename;
  path model_path = filename.remove_filename();
  path animation_dir = model_path / "Animations";

  vector<string> animation_filenames;

  try
  {
    for (const directory_entry &dir : directory_iterator(animation_dir))
    {
      if (dir.is_regular_file())
      {
        path extension = dir.path().extension();
        path dirpath = dir.path();
        path dirextension = dirpath.extension();
        if (extension == ".FBX" || extension == ".fbx")
        {
          // path has dir/dir2/dir3\\filename.fbx

          path test = dir.path();
          test.make_preferred();
          string path_str = test.string();

          animation_filenames.push_back(path_str);
        }
      }
    }
  }
  catch (exception &e)
  {
  }

  for (size_t i = 0; i < animation_filenames.size(); ++i)
  {
    const aiScene *aiscene =
        IMPORTER.ReadFile(animation_filenames[i].c_str(), dst->import_flags & ~aiProcess_SplitByBoneCount);

    // AI_SCENE_FLAGS_INCOMPLETE is set if there are no meshes or no materials
    // the fbx animation only files will always trigger it regardless
    string err = IMPORTER.GetErrorString();
    if (!aiscene)
    {
      set_message("ERROR::ASSIMP::", err);
      continue;
    }

    gather_animations(aiscene, dst);
  }
}

void gather_unprocessed_mesh_only_bones(Imported_Scene_Data *dst)
{

  uint32 flags = dst->import_flags;
  // disable splitting because assimp appears to be culling bones that dont affect
  // any vertices
  flags = flags & ~aiProcess_SplitByBoneCount;
  flags = flags & ~aiProcess_CalcTangentSpace; // we dont need tangent space vectors for just bones here
  const aiScene *aiscene1 =
      IMPORTER.ReadFile(dst->assimp_filename.c_str(), dst->import_flags & ~aiProcess_SplitByBoneCount);
  for (uint32 i = 0; i < aiscene1->mNumMeshes; ++i)
  {

    aiMesh *aimesh = aiscene1->mMeshes[i];
    for (uint32 i = 0; i < aimesh->mNumBones; ++i)
    {
      const aiBone *aibone = aimesh->mBones[i];
      Bone bone;
      bone.name = aibone->mName.data;
      bone.offset = copy(aibone->mOffsetMatrix);

      bool found = false;
      for (auto &exists : dst->all_imported_bones)
      {
        Bone &existing_bone = exists.second;
        if (existing_bone.name == bone.name)
        {
          found = true;
          mat4 &existing_matrix = existing_bone.offset;
          mat4 &mesh_bone_matrix = bone.offset;
          ASSERT(existing_matrix == mesh_bone_matrix);
          break;
          // if we find matching names with mismatching offsets something is broken...
        }
      }
      if (!found)
        dst->all_imported_bones[bone.name] = bone;
    }
  }
}

bool Resource_Manager::import_aiscene_new(Imported_Scene_Data *dst)
{
  ASSERT(dst);

  // i forget why using the split flag doesnt work
  // we cant gather all scene bones after doing it?
  //               aiProcess_SplitByBoneCount
  // is the assimp flag actually optimizing out bones that it feels are unused
  // because there is no animation for them defined in this specific fbx file?
  // if it does thats wrong because we use separate fbx files per animation
  // that expect them to exist

  // gather all the unprocessed bones of the entire model first
  // currently just reads the aiscene file without the processing step
  // can it be sped up by applying the processing afterwards?

  // if there are bones that dont directly affect any vertices in the primary fbx
  // then those bones will only exist in the rig graph

  gather_unprocessed_mesh_only_bones(dst);

  // default here is 60 but we can do more
  AI_SBBC_DEFAULT_MAX_BONES;
  // IMPORTER.SetPropertyInteger(AI_CONFIG_PP_SBBC_MAX_BONES, 216);
  const aiScene *aiscene = IMPORTER.ReadFile(dst->assimp_filename.c_str(), dst->import_flags);

  if (!aiscene || aiscene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !aiscene->mRootNode)
  {
    set_message("ERROR::ASSIMP::", IMPORTER.GetErrorString());
    return false;
  }
  ASSERT(aiscene->mRootNode);
  ASSERT(aiscene->mRootNode->mNumMeshes == 0);

  // is being applied to the root node's scale vector
  aiscene->mMetaData->Get("UnitScaleFactor", dst->scale_factor);

  // the flat mesh array
  gather_meshes(aiscene, dst);

  // if we can pack all the anims into the same file this alone will work - would be easier
  gather_animations(aiscene, dst);

  // the flat material array
  gather_materials(aiscene, dst);

  // just gathering names/transforms/meshindices/materialindices/children
  dst->root_node = _import_aiscene_node(dst, aiscene, aiscene->mRootNode);

  // all animations seem to be importing correctly
  // they all have 247 names, all sequential
  gather_animation_subfolder(dst);

  // assign the materials for this import to use skeletal animation vertex shader here
  if (dst->animations.size())
  {
    for (size_t i = 0; i < dst->materials.size(); ++i)
    {
      dst->materials[i].vertex_shader = "skeletal_animation.vert";
    }
  }

  dst->valid = true;
  return true;
}

Material_Index Resource_Manager::push_material(Material_Descriptor *d)
{
  ASSERT(d);
  Material_Index result;
  result = current_material_pool_size;
  current_material_pool_size += 1;
  ASSERT(result < MAX_POOL_SIZE);
  material_pool[result].descriptor = *d;
  material_pool[result].reload_from_descriptor = true;
  return result;
}

Mesh_Index Resource_Manager::push_mesh(Mesh_Descriptor *d)
{
  Mesh_Index result;
  result = current_mesh_pool_size;
  current_mesh_pool_size += 1;
  ASSERT(result < MAX_POOL_SIZE);
  mesh_pool[result] = Mesh(*d);
  return result;
}

void Resource_Manager::clear()
{
  for (auto &e : mesh_pool)
  {
    e = {};
  }
  for (auto &e : material_pool)
  {
    e = {};
  }
  for (auto &e : animation_set_pool)
  {
    e = {};
  }
  for (auto &e : model_bone_set_pool)
  {
    e = {};
  }
  for (auto &e : animation_state_pool)
  {
    e = {};
  }
  current_mesh_pool_size = 0;
  current_material_pool_size = 0;
  current_animation_set_pool_size = 0;
  current_model_bone_set_pool_size = 0;
  current_animation_state_pool_size = 0;
  // import_thread.join();
  // thread_active = false;
}

// todo: not the best, refactor these so they construct in place

uint32 Resource_Manager::push_bone_set(std::unordered_map<std::string, Bone> *bones)
{
  ASSERT(current_model_bone_set_pool_size < MAX_POOL_SIZE);
  model_bone_set_pool[current_model_bone_set_pool_size].bones = *bones;
  current_model_bone_set_pool_size += 1;
  return current_model_bone_set_pool_size - 1;
}

uint32 Resource_Manager::push_animation_set(std::vector<Skeletal_Animation> *animation_set)
{
  ASSERT(current_animation_set_pool_size < MAX_POOL_SIZE);
  animation_set_pool[current_animation_set_pool_size].animation_set = *animation_set;
  current_animation_set_pool_size += 1;
  return current_animation_set_pool_size - 1;
}

uint32 Resource_Manager::push_animation_state()
{
  ASSERT(current_animation_state_pool_size < MAX_POOL_SIZE);
  current_animation_state_pool_size += 1;
  return current_animation_state_pool_size - 1;
}

Imported_Scene_Data *Resource_Manager::request_valid_resource(std::string path, bool wait_for_valid)
{
  if (import_data.contains(path))
  {
    Imported_Scene_Data *data_import = &import_data[path];
    ASSERT(data_import->assimp_filename == path);
    ASSERT(data_import->valid == true);
    return data_import;
  }
  Imported_Scene_Data *data_import = &import_data[path];
  data_import->import_flags = default_assimp_flags;
  data_import->assimp_filename = path;
  if (!wait_for_valid)
  {
    bool finished = import_aiscene_async(data_import);
    if (finished)
    {
      ASSERT(data_import->valid == true);
      ASSERT(data_import->assimp_filename == path);
      return data_import;
    }
    else
    {
      return nullptr;
    }
  }
  if (wait_for_valid)
  {
    bool success = import_aiscene_new(data_import);
    ASSERT(data_import->valid == true);
    ASSERT(data_import->assimp_filename == path);
    return data_import;
  }
}

Scene_Graph_Node::Scene_Graph_Node()
{
  for (uint32 i = 0; i < children.size(); ++i)
  {
    children[i] = NODE_NULL;
  }
  for (uint32 i = 0; i < model.size(); ++i)
  {
    model[i] = {{NODE_NULL}, {NODE_NULL}};
  }
}

Material_Descriptor *Scene_Graph::get_modifiable_material_pointer_for(Node_Index node, Model_Index model)
{
  Material_Index mi = nodes[node].model[model].second;

  return resource_manager->material_pool[mi].get_modifiable_descriptor();
}

// assuming world basis for now

Octree::Octree()
{
  nodes.resize(10000000);
  root = &nodes[0];
  root->size = 50;
  // root->halfsize = 0.5f * 50;
  root->minimum = -vec3(0.5f * root->size);
  root->center = root->minimum + vec3(0.5f * root->size);
  free_node = 1;
  root->mydepth = 0;
}

void Octree::push(Mesh_Descriptor *mesh, mat4 *transform, vec3 *velocity)
{
  update_render_entities = true;
  ASSERT(root == &nodes[0]);
  Mesh_Data *data = &mesh->mesh_data;
  std::vector<Triangle_Normal> colliders;

  bool all_worked = true;
  uint32 degenerate_count = 0;
  for (size_t i = 0; i < data->indices.size(); i += 3)
  {
    uint32 a, b, c;
    a = data->indices[i];
    b = data->indices[i + 1];
    c = data->indices[i + 2];
    Triangle_Normal t;
    if (velocity)
      t.v = *velocity;

    if (transform)
    {
      t.a = (*transform) * vec4(data->positions[a], 1);
      t.b = (*transform) * vec4(data->positions[b], 1);
      t.c = (*transform) * vec4(data->positions[c], 1);
    }
    else
    {
      t.a = data->positions[a];
      t.b = data->positions[b];
      t.c = data->positions[c];
    }
    vec3 atob = t.b - t.a;
    vec3 atoc = t.c - t.a;
    t.n = normalize(cross(atob, atoc));

    if (isnan(t.n.x) || isnan(t.n.y) || isnan(t.n.z))
    {
      degenerate_count += 1;
      continue;
    }
    all_worked = all_worked && root->push(t, 0, this);
    pushed_triangle_count++;
    // return;//sponge
  }
  if (degenerate_count)
    set_message(s("Warning: Degenerate triangles in Octree::push() for ", mesh->name, " Count:").c_str(),
        s(degenerate_count), 15.f);

  if (!all_worked)
  {
    set_message("OCTREE: not all triangles inserted", "", 20.f);
    // ASSERT(0);
  }
}

const Triangle_Normal *Octree::test(const AABB &probe, uint32 *counter) const
{
  return root->test(probe, 0, counter);
}

inline Octree_Node *Octree::new_node(vec3 p, float32 size, uint8 depth) noexcept
{
  if (!(free_node < nodes.size()))
  {
    return nullptr;
  }
  Octree_Node *ptr = &nodes[free_node];
  free_node += 1;
  ptr->minimum = p;
  ptr->size = size;
  // ptr->halfsize = 0.5f * size;
  ptr->center = p + vec3(0.5f * ptr->size);
  ptr->mydepth = depth + 1;
#ifdef OCTREE_VECTOR_STYLE
  ptr->occupying_triangles.reserve(16);
#endif
  return ptr;
}

void Octree::clear()
{
  root->clear();
  root = &nodes[0];
  free_node = 1;
  root->mydepth = 0;
  update_render_entities = true;
}

std::vector<Render_Entity> Octree::get_render_entities(Scene_Graph *scene)
{
  if (!update_render_entities)
  {
    pack_chosen_entities();
    return chosen_render_entities;
  }
  Mesh_Descriptor md1, md2, md3, mtriangles, mnormals, mvelocities;
  md1.mesh_data.reserve(10000);
  md2.mesh_data.reserve(10000);
  md3.mesh_data.reserve(10000);
  mtriangles.mesh_data.reserve(10000);
  mnormals.mesh_data.reserve(10000);
  mvelocities.mesh_data.reserve(10000);
  update_render_entities = false;

  bool init_resources = mat1 == NODE_NULL;
  if (init_resources)
  {

    Mesh_Descriptor md;
    md.mesh_data = load_mesh_plane();
    md.name = "octree_mesh_depth_1";
    mesh_depth_1 = scene->resource_manager->push_mesh(&md);
    md.name = "octree_mesh_depth_2";
    mesh_depth_2 = scene->resource_manager->push_mesh(&md);
    md.name = "octree_mesh_depth_3";
    mesh_depth_3 = scene->resource_manager->push_mesh(&md);
    md.name = "octree_mesh_triangles";
    mesh_triangles = scene->resource_manager->push_mesh(&md);
    md.name = "octree_mesh_normals";
    mesh_normals = scene->resource_manager->push_mesh(&md);
    md.name = "octree_mesh_velocities";
    mesh_velocities = scene->resource_manager->push_mesh(&md);

    Material_Descriptor material;
    material.frag_shader = "emission.frag";
    material.emissive.source = "white";
    material.wireframe = true;
    material.backface_culling = true;
    material.translucent_pass = false;

    material.emissive.mod = vec4(.10f, 0.0f, 0.0f, .10f);
    mat1 = scene->resource_manager->push_material(&material);
    material.emissive.mod = vec4(0.0f, .10f, 0.0f, .10f);
    mat2 = scene->resource_manager->push_material(&material);
    material.emissive.mod = vec4(0.0f, 0.0f, .10f, .10f);
    mat3 = scene->resource_manager->push_material(&material);

    material.wireframe = true;
    // material.translucent_pass = false;
    // material.fixed_function_blending = false;
    // material.frag_shader = "emission.frag";
    material.emissive.mod = vec4(2.0f, 0.0f, 0.0f, 1.0f);
    mat_triangles = scene->resource_manager->push_material(&material);

    material.emissive.mod = vec4(0.0f, 2.0f, 2.0f, 1.0f);
    mat_normals = scene->resource_manager->push_material(&material);

    material.emissive.mod = vec4(2.0f, 0.0f, 2.0f, 1.0f);
    mat_velocities = scene->resource_manager->push_material(&material);
  }

  for (uint32 i = 0; i < free_node; ++i)
  {
    Octree_Node *node = &nodes[i];
    if (depth_to_render != -1)
    {
      if (node->mydepth != depth_to_render)
        continue;
    }
    Mesh_Descriptor *dst = nullptr;
    if ((node->mydepth % 3) == 0)
      dst = &md1;
    if ((node->mydepth % 3) == 1)
      dst = &md2;
    if ((node->mydepth % 3) == 2)
      dst = &md3;
    ASSERT(dst);
    add_aabb(node->minimum, node->minimum + node->size, dst->mesh_data);
    bool limit_reached = false;
    if (draw_triangles || draw_normals || draw_velocity)
    {
#ifdef OCTREE_VECTOR_STYLE
      for (uint32 j = 0; j < node->occupying_triangles.size(); ++j)
#else
      for (uint32 j = 0; j < node->free_triangle_index; ++j)
#endif
      {
        const Triangle_Normal &tri = node->occupying_triangles[j];
        // if (draw_triangles)
        {
          add_triangle(tri.a, tri.b, tri.c, mtriangles.mesh_data);
        }

        if (mtriangles.mesh_data.positions.size() > 1000000)
        {
          set_message("octree triangle draw limit reached", "", 20.f);
          limit_reached = true;
          break;
        }
        // if (draw_normals)

        vec3 center = vec3(0.333f) * (tri.a + tri.b + tri.c);
        vec3 tip = center + tri.n;
        float32 base_width = 0.1f;
        vec3 offsettoa = base_width * (tri.a - center);
        vec3 offsettob = base_width * (tri.b - center);
        vec3 offsettoc = base_width * (tri.c - center);

        add_triangle(center, tip, center + offsettoa, mnormals.mesh_data);
        add_triangle(center, tip, center + offsettob, mnormals.mesh_data);
        add_triangle(center, tip, center + offsettoc, mnormals.mesh_data);

        // if (draw_velocity)

        center = vec3(0.33333f) * (tri.a + tri.b + tri.c);
        tip = center + 10.f * tri.v;
        base_width = 0.1f;
        offsettoa = base_width * (tri.a - center);
        offsettob = base_width * (tri.b - center);
        offsettoc = base_width * (tri.c - center);

        add_triangle(center, tip, center + offsettoa, mvelocities.mesh_data);
        add_triangle(center, tip, center + offsettob, mvelocities.mesh_data);
        add_triangle(center, tip, center + offsettoc, mvelocities.mesh_data);
      }
    }
    if (limit_reached)
      break;
  }
  scene->resource_manager->mesh_pool[mesh_depth_1] = md1;
  scene->resource_manager->mesh_pool[mesh_depth_2] = md2;
  scene->resource_manager->mesh_pool[mesh_depth_3] = md3;
  Mesh *m1 = &scene->resource_manager->mesh_pool[mesh_depth_1];
  Mesh *m2 = &scene->resource_manager->mesh_pool[mesh_depth_2];
  Mesh *m3 = &scene->resource_manager->mesh_pool[mesh_depth_3];
  Render_Entity e1("OctreeDepth1", m1, &scene->resource_manager->material_pool[mat1], mat4(1), Node_Index(NODE_NULL));
  Render_Entity e2("OctreeDepth2", m2, &scene->resource_manager->material_pool[mat2], mat4(1), Node_Index(NODE_NULL));
  Render_Entity e3("OctreeDepth3", m3, &scene->resource_manager->material_pool[mat3], mat4(1), Node_Index(NODE_NULL));
  set_message("octree vertex count:", s(mtriangles.mesh_data.positions.size()), 30.f);
  set_message("octree triangle count:", s(mtriangles.mesh_data.indices.size() / 3), 30.f);
  if (m1->mesh->descriptor.mesh_data.indices.size() > 0)
    cubes.push_back(e1);
  if (m2->mesh->descriptor.mesh_data.indices.size() > 0)
    cubes.push_back(e2);
  if (m3->mesh->descriptor.mesh_data.indices.size() > 0)
    cubes.push_back(e3);

  scene->resource_manager->mesh_pool[mesh_triangles] = mtriangles;
  Mesh *ptr = &scene->resource_manager->mesh_pool[mesh_triangles];
  triangles = Render_Entity(Array_String("OctreeTriangles"), ptr,
      &scene->resource_manager->material_pool[mat_triangles], mat4(1), Node_Index(NODE_NULL));

  scene->resource_manager->mesh_pool[mesh_normals] = mnormals;
  ptr = &scene->resource_manager->mesh_pool[mesh_normals];
  normals = Render_Entity(Array_String("OctreeNormals"), ptr, &scene->resource_manager->material_pool[mat_normals],
      mat4(1), Node_Index(NODE_NULL));

  scene->resource_manager->mesh_pool[mesh_velocities] = mvelocities;
  ptr = &scene->resource_manager->mesh_pool[mesh_velocities];
  velocities = Render_Entity(Array_String("OctreeVelocities"), ptr,
      &scene->resource_manager->material_pool[mat_velocities], mat4(1), Node_Index(NODE_NULL));

  pack_chosen_entities();
  return chosen_render_entities;
}

inline bool Octree_Node::insert_triangle(const Triangle_Normal &tri) noexcept
{

#ifdef OCTREE_VECTOR_STYLE
  occupying_triangles.push_back(tri);
#else
  if (free_triangle_index == TRIANGLES_PER_NODE)
  {
    return false;
  }
  occupying_triangles[free_triangle_index] = tri;
  free_triangle_index += 1;
#endif
  return true;
}

inline bool Octree_Node::push(const Triangle_Normal &triangle, uint8 depth, Octree *owner) noexcept
{
#ifdef OCTREE_SPLIT_STYLE
  if (depth == MAX_OCTREE_DEPTH)
  {
    owner->stored_triangle_count = owner->stored_triangle_count + 1;
    return insert_triangle(triangle);
  }

  bool requires_self = false;
  for (uint32 i = 0; i < 8; ++i)
  {

    AABB box = aabb_from_octree_child_index(i, minimum, 0.5f * size, size);
    bool intersects = aabb_triangle_intersection(box, triangle);
    if (intersects)
    {
      Octree_Node *child = children[i];

      if (!child)
      {
        child = children[i] = owner->new_node(box.min, 0.5f * size, depth);
        ASSERT(child);
      }

      bool retest = aabb_triangle_intersection(box, triangle);

      bool success = child->push(triangle, depth + 1, owner);
      if (!success)
      {
        requires_self = true;
      }
    }
  }
  if (requires_self)
  {
    owner->stored_triangle_count = owner->stored_triangle_count + 1;
    return insert_triangle(triangle);
  }
  return true;

#endif

#ifndef OCTREE_SPLIT_STYLE
  if (depth == MAX_OCTREE_DEPTH)
    return insert_triangle(triangle);
  const bool left = triangle.a.x < center.x;
  const bool back = triangle.a.y < center.y;
  const bool down = triangle.a.z < center.z;
  vec3 child_minimum = minimum + halfsize * vec3(float32(!left), float32(!back), float32(!down));
  if (left)
  {
    if (back)
    {
      if (down)
      { // left back down
        if (!(triangle.b.x < center.x))
          return insert_triangle(triangle);
        if (!(triangle.b.y < center.y))
          return insert_triangle(triangle);
        if (!(triangle.b.z < center.z))
          return insert_triangle(triangle);
        if (!(triangle.c.x < center.x))
          return insert_triangle(triangle);
        if (!(triangle.c.y < center.y))
          return insert_triangle(triangle);
        if (!(triangle.c.z < center.z))
          return insert_triangle(triangle);
        Octree_Node *node = children[backdownleft];
        if (!node)
          node = children[backdownleft] = owner->new_node(child_minimum, halfsize, depth);
        depth += 1;
        if (!node)
          return false;
        return node->push(triangle, depth, owner);
      }
      else
      { // left back up
        if (!(triangle.b.x < center.x))
          return insert_triangle(triangle);
        if (!(triangle.b.y < center.y))
          return insert_triangle(triangle);
        if (!(triangle.b.z > center.z))
          return insert_triangle(triangle);
        if (!(triangle.c.x < center.x))
          return insert_triangle(triangle);
        if (!(triangle.c.y < center.y))
          return insert_triangle(triangle);
        if (!(triangle.c.z > center.z))
          return insert_triangle(triangle);
        Octree_Node *node = children[backupleft];
        if (!node)
          node = children[backupleft] = owner->new_node(child_minimum, halfsize, depth);
        depth += 1;
        if (!node)
          return false;
        return node->push(triangle, depth, owner);
      }
    }
    else // forward
    {
      if (down)
      { // left forward down
        if (!(triangle.b.x < center.x))
          return insert_triangle(triangle);
        if (!(triangle.b.y > center.y))
          return insert_triangle(triangle);
        if (!(triangle.b.z < center.z))
          return insert_triangle(triangle);
        if (!(triangle.c.x < center.x))
          return insert_triangle(triangle);
        if (!(triangle.c.y > center.y))
          return insert_triangle(triangle);
        if (!(triangle.c.z < center.z))
          return insert_triangle(triangle);
        Octree_Node *node = children[forwarddownleft];
        if (!node)
          node = children[forwarddownleft] = owner->new_node(child_minimum, halfsize, depth);
        depth += 1;
        if (!node)
          return false;
        return node->push(triangle, depth, owner);
      }
      else
      { // left forward up
        if (!(triangle.b.x < center.x))
          return insert_triangle(triangle);
        if (!(triangle.b.y > center.y))
          return insert_triangle(triangle);
        if (!(triangle.b.z > center.z))
          return insert_triangle(triangle);
        if (!(triangle.c.x < center.x))
          return insert_triangle(triangle);
        if (!(triangle.c.y > center.y))
          return insert_triangle(triangle);
        if (!(triangle.c.z > center.z))
          return insert_triangle(triangle);
        Octree_Node *node = children[forwardupleft];
        if (!node)
          node = children[forwardupleft] = owner->new_node(child_minimum, halfsize, depth);
        depth += 1;
        if (!node)
          return false;
        return node->push(triangle, depth, owner);
      }
    }
  }
  else // right
  {
    if (back)
    {
      if (down) // right back down
      {
        if (!(triangle.b.x > center.x))
          return insert_triangle(triangle);
        if (!(triangle.b.y < center.y))
          return insert_triangle(triangle);
        if (!(triangle.b.z < center.z))
          return insert_triangle(triangle);
        if (!(triangle.c.x > center.x))
          return insert_triangle(triangle);
        if (!(triangle.c.y < center.y))
          return insert_triangle(triangle);
        if (!(triangle.c.z < center.z))
          return insert_triangle(triangle);
        Octree_Node *node = children[backdownright];
        if (!node)
          node = children[backdownright] = owner->new_node(child_minimum, halfsize, depth);
        depth += 1;
        if (!node)
          return false;
        return node->push(triangle, depth, owner);
      }
      else // right back up
      {
        if (!(triangle.b.x > center.x))
          return insert_triangle(triangle);
        if (!(triangle.b.y < center.y))
          return insert_triangle(triangle);
        if (!(triangle.b.z > center.z))
          return insert_triangle(triangle);
        if (!(triangle.c.x > center.x))
          return insert_triangle(triangle);
        if (!(triangle.c.y < center.y))
          return insert_triangle(triangle);
        if (!(triangle.c.z > center.z))
          return insert_triangle(triangle);
        Octree_Node *node = children[backupright];
        if (!node)
          node = children[backupright] = owner->new_node(child_minimum, halfsize, depth);
        depth += 1;
        if (!node)
          return false;
        return node->push(triangle, depth, owner);
      }
    }
    else // forward
    {
      if (down) // right forward down
      {
        if (!(triangle.b.x > center.x))
          return insert_triangle(triangle);
        if (!(triangle.b.y > center.y))
          return insert_triangle(triangle);
        if (!(triangle.b.z < center.z))
          return insert_triangle(triangle);
        if (!(triangle.c.x > center.x))
          return insert_triangle(triangle);
        if (!(triangle.c.y > center.y))
          return insert_triangle(triangle);
        if (!(triangle.c.z < center.z))
          return insert_triangle(triangle);
        Octree_Node *node = children[forwarddownright];
        if (!node)
          node = children[forwarddownright] = owner->new_node(child_minimum, halfsize, depth);
        depth += 1;
        if (!node)
          return false;
        return node->push(triangle, depth, owner);
      }
      else // right forward up
      {
        if (!(triangle.b.x > center.x))
          return insert_triangle(triangle);
        if (!(triangle.b.y > center.y))
          return insert_triangle(triangle);
        if (!(triangle.b.z > center.z))
          return insert_triangle(triangle);
        if (!(triangle.c.x > center.x))
          return insert_triangle(triangle);
        if (!(triangle.c.y > center.y))
          return insert_triangle(triangle);
        if (!(triangle.c.z > center.z))
          return insert_triangle(triangle);
        Octree_Node *node = children[forwardupright];
        if (!node)
          node = children[forwardupright] = owner->new_node(child_minimum, halfsize, depth);
        depth += 1;
        if (!node)
          return false;
        return node->push(triangle, depth, owner);
      }
    }
  }
  return false;
#endif
}

inline const Triangle_Normal *Octree_Node::test_this(
    const AABB &probe, uint32 *test_count, std::vector<Triangle_Normal> *accumulator) const
{
#ifdef OCTREE_VECTOR_STYLE
  for (uint32 i = 0; i < occupying_triangles.size(); ++i)
#else
  for (uint32 i = 0; i < free_triangle_index; ++i)
#endif
  {
    const Triangle_Normal *triangle = &occupying_triangles[i];

    bool need_add = true;
    if (accumulator)
    {
      for (uint32 j = 0; j < accumulator->size(); ++j)
      {
        if ((*accumulator)[j].a == triangle->a && (*accumulator)[j].b == triangle->b &&
            (*accumulator)[j].c == triangle->c)
        {
          need_add = false;
          break;
        }
      }
    }
    if (!need_add)
    {
      continue;
    }
    *test_count += 1;
    if (aabb_triangle_intersection(probe, *triangle))
    {
      if (!accumulator)
      {
        return triangle;
      }
      accumulator->push_back(*triangle);
    }
  }
  return nullptr;
}

inline const Triangle_Normal *Octree_Node::test(const AABB &probe, uint8 depth, uint32 *counter) const
{
#ifdef OCTREE_SPLIT_STYLE

  if (depth == MAX_OCTREE_DEPTH)
  {
    return test_this(probe, counter, nullptr);
  }
  bool requires_self = false;
  for (uint32 i = 0; i < 8; ++i)
  {
    Octree_Node *child = children[i];
    if (!child)
    {
      continue;
    }
    AABB box(child->minimum);
    box.max = box.min + vec3(child->size);
    if (aabb_intersection(box, probe))
    {

      const Triangle_Normal *tri = child->test(probe, depth + 1, counter);
      if (tri)
        return tri;
#ifndef OCTREE_VECTOR_STYLE
      if (!(child->free_triangle_index < child->occupying_triangles.size()))
      {
        requires_self = true;
      }
#endif
    }
  }

#ifndef OCTREE_VECTOR_STYLE
  if (requires_self)
  {
    return test_this(probe, counter, nullptr);
  }
#endif

#endif

#ifndef OCTREE_SPLIT_STYLE
  const Triangle_Normal *r = test_this(probe, counter, nullptr);
  if (r)
  {
    return r;
  }

  for (uint32 i = 0; i < 8; ++i)
  {
    Octree_Node *child = children[i];
    if (!child)
      continue;

    bool intersects_this_child =
        aabb_intersection(probe.min, probe.max, child->minimum, child->minimum + vec3(child->size));
    if (intersects_this_child)
    {
      const Triangle_Normal *r = child->test(probe, depth + 1, counter);
      if (r)
        return r;
    }
  }
  return nullptr;

#endif
}

void Octree_Node::testall(
    const AABB &probe, uint8 depth, uint32 *counter, std::vector<Triangle_Normal> *accumulator) const
{
#ifdef OCTREE_SPLIT_STYLE
  if (depth == MAX_OCTREE_DEPTH)
  {
    test_this(probe, counter, accumulator);
    return;
  }

  bool requires_self = false;
  for (uint32 i = 0; i < 8; ++i)
  {
    Octree_Node *child = children[i];
    if (!child)
    {
      continue;
    }

    if (aabb_intersection(probe.min, probe.max, child->minimum, child->minimum + vec3(child->size)))
    {
      child->testall(probe, depth + 1, counter, accumulator);

#ifndef OCTREE_VECTOR_STYLE
      if (!(child->free_triangle_index < child->occupying_triangles.size()))
      {
        requires_self = true;
      }
#endif
    }
  }
#ifndef OCTREE_VECTOR_STYLE
  if (requires_self)
  {
    test_this(probe, counter, accumulator);
    return;
  }
#endif

#endif

#ifndef OCTREE_SPLIT_STYLE
  test_this(probe, counter, accumulator);
  for (uint32 i = 0; i < 8; ++i)
  {
    Octree_Node *child = children[i];
    if (!child)
      continue;

    bool intersects_this_child =
        aabb_intersection(probe.min, probe.max, child->minimum, child->minimum + vec3(child->size));
    if (intersects_this_child)
    {
      child->testall(probe, depth + 1, counter, accumulator);
    }
  }
#endif
}

inline void Octree_Node::clear()
{
  for (uint32 i = 0; i < 8; ++i)
  {
    Octree_Node *node = children[i];
    if (node)
    {
      node->clear();
    }
#ifdef OCTREE_VECTOR_STYLE
    occupying_triangles.clear();
#else
    free_triangle_index = 0;
#endif
    children[i] = nullptr;
  }
}

// ccw
Plane_nd compute_plane(vec3 a, vec3 b, vec3 c)
{
  Plane_nd p;
  p.n = normalize(cross(b - a, c - a));
  p.d = dot(p.n, a);
  return p;
}

float32 TriArea2D(float32 x1, float32 y1, float32 x2, float32 y2, float32 x3, float32 y3)
{
  return (x1 - x2) * (y2 - y3) - (x2 - x3) * (y1 - y2);
}

// compute barycentric coordinates uvw for
// point p in triangle abc
vec3 barycentric(vec3 a, vec3 b, vec3 c, vec3 p)
{
  // Unnormalized triangle normal
  vec3 m = cross(b - a, c - a);
  // Nominators and one-over-denominator for u and v ratios
  float32 nu, nv, ood;
  // Absolute components for determining projection plane
  float32 x = abs(m.x), y = abs(m.y), z = abs(m.z);
  // Compute areas in plane of largest projection
  if (x >= y && x >= z)
  {
    // x is largest, project to the yz plane
    nu = TriArea2D(p.y, p.z, b.y, b.z, c.y, c.z); // Area of PBC in yz plane
    nv = TriArea2D(p.y, p.z, c.y, c.z, a.y, a.z); // Area of PCA in yz plane
    ood = 1.0f / m.x;                             // 1/(2*area of ABC in yz plane)
  }
  else if (y >= x && y >= z)
  {
    // y is largest, project to the xz plane
    nu = TriArea2D(p.x, p.z, b.x, b.z, c.x, c.z);
    nv = TriArea2D(p.x, p.z, c.x, c.z, a.x, a.z);
    ood = 1.0f / -m.y;
  }
  else
  {
    // z is largest, project to the xy plane
    nu = TriArea2D(p.x, p.y, b.x, b.y, c.x, c.y);
    nv = TriArea2D(p.x, p.y, c.x, c.y, a.x, a.y);
    ood = 1.0f / m.z;
  }
  vec3 result;
  result.x = nu * ood;
  result.y = nv * ood;
  result.z = 1.0f - result.x - result.y;
  return result;
}

bool point_within_square(vec3 p, vec3 ps, float32 size)
{
  return (p.x > ps.x) && (p.x < ps.x + size) && (p.y > ps.y) && (p.y < ps.y + size) && (p.z > ps.z) &&
         (p.z < ps.z + size);
}

bool aabb_intersection(const vec3 &mina, const vec3 &maxa, const vec3 &minb, const vec3 &maxb)
{
  if (maxa.x < minb.x)
    return false;
  if (mina.x > maxb.x)
    return false;

  if (maxa.y < minb.y)
    return false;
  if (mina.y > maxb.y)
    return false;

  if (maxa.z < minb.z)
    return false;
  if (mina.z > maxb.z)
    return false;

  return true;
}

bool aabb_intersection(const AABB &a, const AABB &b)
{
  return aabb_intersection(a.min, a.max, b.min, b.max);
}

bool aabb_plane_intersection(const AABB &b, const vec3 &n, float32 d)
{
  vec3 c = (b.max + b.min) * 0.5f;
  vec3 e = b.max - c;
  float32 r = e[0] * abs(n[0]) + e[1] * abs(n[1]) + e[2] * abs(n[2]);
  float32 s = dot(n, c) - d;
  return abs(s) <= r;
}

void push_aabb(AABB &aabb, const vec3 &p)
{
  if (p.x < aabb.min.x)
  {
    aabb.min.x = p.x;
  }
  if (p.x > aabb.max.x)
  {
    aabb.max.x = p.x;
  }

  if (p.y < aabb.min.y)
  {
    aabb.min.y = p.y;
  }
  if (p.y > aabb.max.y)
  {
    aabb.max.y = p.y;
  }

  if (p.z < aabb.min.z)
  {
    aabb.min.z = p.z;
  }
  if (p.z > aabb.max.z)
  {
    aabb.max.z = p.z;
  }
}

float32 max(float32 a, float32 b, float32 c)
{
  return glm::max(glm::max(a, b), c);
}

float32 min(float32 a, float32 b, float32 c)
{
  return glm::min(glm::min(a, b), c);
}

bool aabb_triangle_intersection(const AABB &b, const Triangle_Normal &triangle)
{
  float p0, p1, p2, r;
  vec3 c = (b.min + b.max) * 0.5f;
  float e0 = (b.max.x - b.min.x) * 0.5f;
  float e1 = (b.max.y - b.min.y) * 0.5f;
  float e2 = (b.max.z - b.min.z) * 0.5f;
  vec3 v0 = triangle.a - c;
  vec3 v1 = triangle.b - c;
  vec3 v2 = triangle.c - c;
  vec3 f0 = v1 - v0, f1 = v2 - v1, f2 = v0 - v2;

  p0 = v0.z * f0.y - v0.y * f0.z;
  p2 = v2.z * f0.y - v2.y * f0.z;
  r = e1 * abs(f0.z) + e2 * abs(f0.y);
  if (glm::max(-glm::max(p0, p2), glm::min(p0, p2)) > r)
    return 0;

  p0 = v0.z * f1.y - v0.y * f1.z;
  p2 = v2.z * f1.y - v2.y * f1.z;
  r = e1 * abs(f1.z) + e2 * abs(f1.y);
  if (glm::max(-glm::max(p0, p2), glm::min(p0, p2)) > r)
    return 0;

  p0 = v0.z * f2.y - v0.y * f2.z;
  p1 = v1.z * f2.y - v1.y * f2.z;
  r = e1 * abs(f2.z) + e2 * abs(f2.y);
  if (glm::max(-glm::max(p0, p1), glm::min(p0, p1)) > r)
    return 0;

  p0 = v0.x * f0.z - v0.z * f0.x;
  p2 = v2.x * f0.z - v2.z * f0.x;
  r = e0 * abs(f0.z) + e2 * abs(f0.x);
  if (glm::max(-glm::max(p0, p2), glm::min(p0, p2)) > r)
    return 0;

  p0 = v0.x * f1.z - v0.z * f1.x;
  p2 = v2.x * f1.z - v2.z * f1.x;
  r = e0 * abs(f1.z) + e2 * abs(f1.x);
  if (glm::max(-glm::max(p0, p2), glm::min(p0, p2)) > r)
    return 0;

  p0 = v0.x * f2.z - v0.z * f2.x;
  p1 = v1.x * f2.z - v1.z * f2.x;
  r = e0 * abs(f2.z) + e2 * abs(f2.x);
  if (glm::max(-glm::max(p0, p1), glm::min(p0, p1)) > r)
    return 0;

  p0 = v0.y * f0.x - v0.x * f0.y;
  p2 = v2.y * f0.x - v2.x * f0.y;
  r = e0 * abs(f0.y) + e1 * abs(f0.x);
  if (glm::max(-glm::max(p0, p2), glm::min(p0, p2)) > r)
    return 0;

  p0 = v0.y * f1.x - v0.x * f1.y;
  p2 = v2.y * f1.x - v2.x * f1.y;
  r = e0 * abs(f1.y) + e1 * abs(f1.x);
  if (glm::max(-glm::max(p0, p2), glm::min(p0, p2)) > r)
    return 0;

  p0 = v0.y * f2.x - v0.x * f2.y;
  p1 = v1.y * f2.x - v1.x * f2.y;
  r = e0 * abs(f2.y) + e1 * abs(f2.x);
  if (glm::max(-glm::max(p0, p1), glm::min(p0, p1)) > r)
    return 0;

  if (max(v0.x, v1.x, v2.x) < -e0 || min(v0.x, v1.x, v2.x) > e0)
    return 0;
  if (max(v0.y, v1.y, v2.y) < -e1 || min(v0.y, v1.y, v2.y) > e1)
    return 0;
  if (max(v0.z, v1.z, v2.z) < -e2 || min(v0.z, v1.z, v2.z) > e2)
    return 0;
  vec3 n = cross(f0, f1);
  float32 d = dot(n, v0 + c);
  return aabb_plane_intersection(b, n, d);
}

AABB aabb_from_octree_child_index(uint8 i, vec3 minimum, float32 halfsize, float32 size)
{
  //+y forward, +z up

  vec3 offset;

  // front up left
  if (i == 0)
  {
    offset = vec3(0, halfsize, halfsize);
  }
  // front up right
  else if (i == 1)
  {
    offset = vec3(halfsize, halfsize, halfsize);
  }
  // front down right
  else if (i == 2)
  {
    offset = vec3(halfsize, halfsize, 0);
  }
  // front down left
  else if (i == 3)
  {
    offset = vec3(0, halfsize, 0);
  }
  // back up left
  else if (i == 4)
  {
    offset = vec3(0, 0, halfsize);
  }
  // back up right
  else if (i == 5)
  {
    offset = vec3(halfsize, 0, halfsize);
  }
  // back down right
  else if (i == 6)
  {
    offset = vec3(halfsize, 0, 0);
  }
  // back down left
  else if (i == 7)
  {
    offset = vec3(0, 0, 0);
  }

  AABB box(minimum + offset);
  box.max = box.min + vec3(halfsize);
  return box;
}
