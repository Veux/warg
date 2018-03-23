#include "Json.h"

extern std::unordered_map<std::string, std::weak_ptr<Mesh_Handle>> MESH_CACHE;

json jsonify(Scene_Graph &scene) { return scene; }

void dejsonificate(Scene_Graph *scene, json j) { *scene = j; }

void _pretty_json(std::string &result, const std::string &input,
    std::string::size_type &pos, size_t le, size_t indent)
{
  bool in_string = false;
  for (; pos < input.length(); ++pos)
  {
    char curr_char = input[pos];
    if ((curr_char == ' ' || curr_char == '\t') && !in_string)
    {
      continue;
    }
    else if ((curr_char == '\r' || curr_char == '\n') && !in_string)
    {
      continue;
    }
    else if ((curr_char == '{' || curr_char == '[') && !in_string)
    {
      result.append(1, curr_char);
      result.append(1, '\n');
      result.append((le + 1) * indent, ' ');
      _pretty_json(result, input, ++pos, le + 1, indent);
    }
    else if ((curr_char == '}' || curr_char == ']') && !in_string)
    {
      result.append(1, '\n');
      result.append((le - 1) * indent, ' ');
      result.append(1, curr_char);
      return;
    }
    else if (curr_char == ':' && !in_string)
    {
      result.append(1, curr_char);
      result.append(1, ' ');
    }
    else if (curr_char == ',' && !in_string)
    {
      result.append(1, curr_char);
      result.append(1, '\n');
      result.append(le * indent, ' ');
    }
    else if (curr_char == '"')
    {
      char last_char = (pos == 0) ? 0 : input[pos - 1];
      if (last_char != '\\')
      {
        result.append(1, curr_char);
        in_string = !in_string;
      }
    }
    else
    {
      result.append(1, curr_char);
    }
  }
}

std::string pretty_dump(const json &j)
{
  size_t pos = 0;
  std::string result;
  _pretty_json(result, j.dump(), pos);
  return result;
}

void to_json(json &result, const Light &p)
{
  json j;
  j["Position"] = p.position;
  j["Direction"] = p.direction;
  j["Brightness"] = p.brightness;
  j["Color"] = p.color;
  j["Attenuation"] = p.attenuation;
  j["Ambient"] = p.ambient;
  j["Cone Angle"] = p.cone_angle;
  j["Light Type"] = (uint32)p.type;
  j["Casts Shadows"] = p.casts_shadows;
  j["Shadow Blur Iterations"] = p.shadow_blur_iterations;
  j["Shadow Blur Radius"] = p.shadow_blur_radius;
  j["Shadow Near Plane"] = p.shadow_near_plane;
  j["Shadow Far Plane"] = p.shadow_far_plane;
  j["Max Variance"] = p.max_variance;
  j["Shadow FoV"] = p.shadow_fov;
  result = j;
}

void from_json(const json &j, Light &p)
{
  p.position = j.at("Position");
  p.direction = j.at("Direction");
  p.brightness = j.at("Brightness");
  p.color = j.at("Color");
  p.attenuation = j.at("Attenuation");
  p.ambient = j.at("Ambient");
  p.cone_angle = j.at("Cone Angle");
  p.type = (Light_Type)j.at("Light Type");
  p.casts_shadows = j.at("Casts Shadows");
  p.shadow_blur_iterations = j.at("Shadow Blur Iterations");
  p.shadow_blur_radius = j.at("Shadow Blur Radius");
  p.shadow_near_plane = j.at("Shadow Near Plane");
  p.shadow_far_plane = j.at("Shadow Far Plane");
  p.max_variance = j.at("Max Variance");
  p.shadow_fov = j.at("Shadow FoV");
}

void to_json(json &result, const Mesh &p)
{
  // the only thing this will let us restore is already-imported-for-this-run
  // Mesh_Handles  that havent gone out of scope yet, and Mesh_Primitives
  json j;
  j["Name"] = p.name;
  j["Unique Identifier"] = p.unique_identifier;
  result = j;

  ////below is just for a warning:

  std::shared_ptr<Mesh_Handle> cached_mesh =
      MESH_CACHE[p.unique_identifier].lock();

  if (!cached_mesh)
  {
    if (s_to_primitive(p.name) ==
        null) // j["Data"] = p.stored_copy_of_vertex_data;?
      set_message(s("Mesh to_json warning: Attempting to save a mesh that was "
                    "created with custom Mesh_Data (unsupported). Name:",
                      p.name, " Unique Identifier:", p.unique_identifier),
          "", 5.0f);
  }
}

void from_json(const json &j, Mesh &p)
{
  std::shared_ptr<Mesh_Handle> cached_mesh;
  std::string name = j.at("Name");
  std::string id = j.at("Unique Identifier");
  cached_mesh = MESH_CACHE[id].lock();
  if (cached_mesh)
  {
    p.unique_identifier = id;
    p.mesh = cached_mesh;
    p.name = name;
    return;
  }
  else
  { // was not in the cache
    Mesh_Primitive primitive = s_to_primitive(name);
    if (primitive != null)
    {
      p = Mesh(primitive, name);
      return;
    }
    else
    { // was not a primitive, and was not found in the assimp MESH_CACHE

      // todo: Meshes made with custom Mesh_Data are unsupported - the mesh isnt
      // currently holding the mesh data,  and cannot be restored from json

      throw s(
          "Mesh from_json failed to find Unique ID in cache: Unique_ID:", id);
      // if this is an assimp import child, maybe the handle was let go just
      // before this function call?  maybe the parent assimp import was
      // unsuccessful, or the unique id is incorrect
    }
  }
}

void to_json(json &result, const Material &p) { result = p.m; }

void from_json(const json &j, Material &p)
{
  Material_Descriptor m = j;
  p = Material(m);
}

void to_json(json &result, const Texture_Descriptor &p)
{
  json j;
  j["Filename"] = p.name;
  j["Mod"] = p.mod;
  j["Size"] = p.size;
  j["Format"] = p.format;
  j["Minification Filter"] = p.minification_filter;
  j["Magnification Filter"] = p.magnification_filter;
  j["Wrap_S"] = p.wrap_s;
  j["Wrap_T"] = p.wrap_t;
  j["Premultiply"] = p.process_premultiply;
  result = j;
}

void from_json(const json &j, Texture_Descriptor &p)
{
  Texture_Descriptor result;
  std::string value = j.at("Filename");
  result.name = value;
  result.mod = j.at("Mod");
  result.size = j.at("Size");
  result.format = j.at("Format");
  result.minification_filter = j.at("Minification Filter");
  result.magnification_filter = j.at("Magnification Filter");
  result.wrap_s = j.at("Wrap_S");
  result.wrap_t = j.at("Wrap_T");
  result.process_premultiply = j.at("Premultiply");
  p = result;
}

void to_json(json &result, const Material_Descriptor &p)
{
  json j;
  j["Albedo"] = p.albedo;
  j["Normal"] = p.normal;
  j["Roughness"] = p.roughness;
  j["Specular"] = p.specular;
  j["Metalness"] = p.metalness;
  j["Tangent"] = p.tangent;
  j["Ambient Occlusion"] = p.ambient_occlusion;
  j["Emissive"] = p.emissive;
  j["Vertex Shader"] = p.vertex_shader;
  j["Fragment Shader"] = p.frag_shader;
  j["UV Scale"] = p.uv_scale;
  j["Albedo Alpha Override"] = p.albedo_alpha_override;
  j["Backface Culling"] = p.backface_culling;
  j["Uses Transparency"] = p.uses_transparency;
  j["Casts Shadows"] = p.casts_shadows;
  result = j;
}

void from_json(const json &j, Material_Descriptor &p)
{
  Material_Descriptor result;
  result.albedo = j.at("Albedo");
  result.normal = j.at("Normal");
  result.roughness = j.at("Roughness");
  result.specular = j.at("Specular");
  result.metalness = j.at("Metalness");
  result.tangent = j.at("Tangent");
  result.ambient_occlusion = j.at("Ambient Occlusion");
  result.emissive = j.at("Emissive");
  std::string vert = j.at("Vertex Shader");
  result.vertex_shader = vert;
  std::string frag = j.at("Fragment Shader");
  result.frag_shader = frag;
  result.uv_scale = j.at("UV Scale");
  result.albedo_alpha_override = j.at("Albedo Alpha Override");
  result.backface_culling = j.at("Backface Culling");
  result.uses_transparency = j.at("Uses Transparency");
  result.casts_shadows = j.at("Casts Shadows");
  p = result;
}

void to_json(json &result, const Light_Array &p)
{
  json j;
  j["Lights"] = p.lights;
  j["Light Count"] = p.light_count;
  j["Additional Ambient"] = p.additional_ambient;
  result = j;
}

void from_json(const json &j, Light_Array &p)
{
  Light_Array l;
  l.lights = j.at("Lights");
  l.light_count = j.at("Light Count");
  l.additional_ambient = j.at("Additional Ambient");
  p = l;
}

void to_json(json &result, const std::shared_ptr<Scene_Graph_Node> &node_ptr)
{
  const Scene_Graph_Node &node = *node_ptr.get();
  result = node;
}

void to_json(json &result, const Scene_Graph_Node &node)
{
  json j; 
    if (!node.include_in_save)
    {
      result = j;
      return;
    }

  j["Name"] = node.name;
  j["Position"] = node.position;
  j["Orientation"] = node.orientation;
  j["Scale"] = node.scale;
  j["Velocity"] = node.velocity;
  j["Visible"] = node.visible;
  j["Propagate Visibility"] = node.propagate_visibility;
  j["Import Basis"] = node.import_basis;

  j["Filename Of Import"] = node.filename_of_import;
  j["Is Root Of Import"] = node.is_root_of_import;

  json model_json;
  for (uint32 i = 0; i < node.model.size(); ++i)
  {
    const Mesh &mesh = node.model[i].first;
    const Material &material = node.model[i].second;

    json jpair;

    json jmesh = mesh;
    json jmaterial = material;

    std::string key = mesh.unique_identifier;

    jpair["Mesh"] = jmesh;
    jpair["Material"] = jmaterial;

    model_json[key] = jpair;
  }
  j["Model"] = model_json;

  j["Owned Children"] = {};
  for (uint32 i = 0; i < node.owned_children.size(); ++i)
  {
    auto ptr = node.owned_children[i];
    json child = ptr;
    j["Owned Children"].push_back(child);
  }

  // for (uint32 i = 0; i < node.unowned_children.size(); ++i)
  //{//should this even be here? can you think of a reason to actually save
  // these?
  //  auto ptr = node.unowned_children[i].lock();
  //  j["unowned_children"].push_back(ptr);
  //}
  result = j;
}

void to_json(json &result, const Scene_Graph &scene)
{
  json j;
  j["SCENE_GRAPH_ROOT"] = scene.root;
  j["Lights"] = scene.lights;
  result = j;
}

// only to be called on an already-imported asset node ptr, the json will only
// attempt to  restore the materials for the import
void set_import_materials(Node_Ptr ptr, json json_for_ptr)
{
  json jmodel = json_for_ptr.at("Model");
  for (auto &pair : ptr->model)
  {
    Mesh *mesh = &pair.first;
    Material *material = &pair.second;

    std::string *desired_key = &mesh->unique_identifier;

    json jpair = jmodel.at(*desired_key);
    json jmesh = jpair.at("Mesh");
    json jmaterial = jpair.at("Material");

    //*mesh = jmesh; shouldnt be needed
    *material = jmaterial;
  }

  // ptr->owned_children should not contain anything but the children created by
  // the import  but json could have some that are not
  json jchildren = json_for_ptr.at("Owned Children");

  const uint32 import_children_size = ptr->owned_children.size();
  const uint32 jchildren_size = jchildren.size();

  // we need the full json for the child that matches this already-constructed
  // child in order to recurse
  for (uint32 i = 0; i < import_children_size; ++i)
  {
    if (i >= jchildren_size)
    {
      // problem: if the user added something to this node's owned-children
      // before the final import child, the order will be ruined, and the
      // remaining nodes will never have their materials restored
      // unless you implement heuristics or save the vertex data raw
      break;
    }

    Node_Ptr child = ptr->owned_children[i];
    child->include_in_save = true;
    json jchild = jchildren[i];
    std::string name = jchild.at("Name");
    if (child->name == name)
    { // probably the thing we want - not guaranteed - if the index and name of
      // the object are the same, its going to get the overriden material
      set_import_materials(child, jchild);
    }
    else
    { // todo: better json support for materials: asset file has probably
      // changed could try some heuristics here to match it up, but for now just
      // abort
      continue;
    }
  }
  // todo: keep track of if the entire asset was imported with an actual
  // material-override pointer  and always respawn it with that override, when
  // reconstructing the node_ptr in build_node_graph_from_json
  // this ensures that even if theres an asset file modification
  // the override materials are still applied
}

Node_Ptr build_node_graph_from_json(const json &j, Scene_Graph &scene)
{
  Node_Ptr ptr;
  std::string name;
  try
  {
    std::string naem = j.at("Name");
    name = naem;
  }
  catch (std::exception &e)
  {
    set_message(s(
        "No \"Name\" found for json node:", j.dump(), "Exception:", e.what()));
    return nullptr;
  }

  if (name == "SCENE_GRAPH_ROOT")
  {
    ptr = std::make_shared<Scene_Graph_Node>();
    ptr->name = name;
  }

  const bool is_root_of_import = j.at("Is Root Of Import");
  const std::string filename_of_import = j.at("Filename Of Import");

  if (is_root_of_import)
  { // assimp import case - does entire import and restores its materials
    // (probably)
    ptr = scene.add_aiscene(filename_of_import);
    ptr->position = j.at("Position");
    ptr->name = name;
    ptr->orientation = j.at("Orientation");
    ptr->scale = j.at("Scale");
    ptr->velocity = j.at("Velocity");
    ptr->visible = j.at("Visible");
    ptr->propagate_visibility = j.at("Propagate Visibility");
    ptr->import_basis = j.at("Import Basis");

    ptr->filename_of_import = filename_of_import;
    ptr->is_root_of_import = true;
    ptr->include_in_save = true;

    set_import_materials(ptr, j);
  }

  if (filename_of_import == "")
  { // primitive case (or custom Mesh_Data (not implemented))

    json json_saved_model = j.at("Model");
    for (json &jpair : json_saved_model)
    {
      json jmesh = jpair.at("Mesh");
      json jmaterial = jpair.at("Material");

      Material_Descriptor material = jmaterial;

      std::string unique_id = jmesh.at("Unique Identifier");
      Mesh_Primitive primitive = null;
      try
      {
        primitive = s_to_primitive(unique_id);
      }
      catch (std::exception &e)
      {
        std::string name = jmesh.at("Name");
        set_message(s("JSON unable to restore mesh/material pair. Name:", name,
            " UID:", unique_id, "Exception:", e.what()));
      }

      if (primitive != null)
      {
        ASSERT(filename_of_import == "");
        ASSERT(!is_root_of_import);
        ASSERT(name != "SCENE_GRAPH_ROOT");

        ptr = scene.add_primitive_mesh(primitive, name, material, nullptr);
      }
    }
  }

  json owned_children = j.at("Owned Children");
  for (auto &owned_child : owned_children)
  {
    Node_Ptr child = build_node_graph_from_json(owned_child, scene);
    if (child)
    {
      child->include_in_save = true;
      ptr->owned_children.push_back(child);
    }
  }
  return ptr;
}

void from_json(const json &k, Scene_Graph &scene)
{
  // problem: very difficult to tell if a child node position(for example) was
  // changed by changing its position  in the scene graph, or by changing its
  // position in the asset import file itself, so, default to using the data
  // provided by the file for all imported scenes

  // possible solution: use filename last modified time  if file is newer than
  // the date saved in the json, file overrides, else json overrides

  try
  {
    json root = k.at("SCENE_GRAPH_ROOT");
    scene.root = build_node_graph_from_json(root, scene);
  }
  catch (std::exception &e)
  {
    std::string pretty = pretty_dump(k);
    set_message(s("Warning: JSON for Scene_Graph::root load failed.",
                    "Exception: ", e.what(), " JSON:\n"),
        pretty, 15.0f);
    scene = Scene_Graph();
  }
  try
  {
    scene.lights = k.at("Lights");
  }
  catch (std::exception &e)
  {
    std::string pretty = pretty_dump(k);
    set_message(s("Warning: JSON for Scene_Graph::lights load failed.",
                    "Exception: ", e.what(), " JSON:\n"),
        pretty, 15.0f);
  }
}
