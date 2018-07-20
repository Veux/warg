#include "stdafx.h"
#include "Json.h"
#include "Render.h"
#include "Scene_Graph.h"

extern std::unordered_map<std::string, std::weak_ptr<Mesh_Handle>> MESH_CACHE;

void _pretty_json(std::string &result, const std::string &input, std::string::size_type &pos, size_t le, size_t indent)
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

void to_json(json &result, const Flat_Scene_Graph &p)
{
  json j;
  j["Light_Array"] = p.lights;
  j["Node Serialization"] = p.serialize();
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
  j["Shadow Map Resolution"] = p.shadow_map_resolution;
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
  p.shadow_map_resolution = j.at("Shadow Map Resolution");
  p.shadow_blur_iterations = j.at("Shadow Blur Iterations");
  p.shadow_blur_radius = j.at("Shadow Blur Radius");
  p.shadow_near_plane = j.at("Shadow Near Plane");
  p.shadow_far_plane = j.at("Shadow Far Plane");
  p.max_variance = j.at("Max Variance");
  p.shadow_fov = j.at("Shadow FoV");
}

void to_json(json &result, const Material &p) { result = p.descriptor; }

void from_json(const json &j, Material &p)
{
  Material_Descriptor m = j;
  p = Material(m);
}

void to_json(json &result, const Environment_Map &p) { result = p.m; }

void from_json(const json &j, Environment_Map &p)
{
  Environment_Map_Descriptor m = j;
  p = Environment_Map(m);
}

void to_json(json &result, const Environment_Map_Descriptor &p)
{
  json j;
  j["Radiance"] = p.radiance;
  j["Irradiance"] = p.irradiance;
  j["Environment Faces"] = p.environment_faces;
  j["Irradiance Faces"] = p.irradiance_faces;
  j["Equirectangular"] = p.source_is_equirectangular;
  result = j;
}

void from_json(const json &j, Environment_Map_Descriptor &p)
{
  Environment_Map_Descriptor result;
  result.radiance = j.at("Radiance").get<std::string>();
  result.irradiance = j.at("Irradiance").get<std::string>();
  result.environment_faces = j.at("Environment Faces");
  result.irradiance_faces = j.at("Irradiance Faces");
  result.source_is_equirectangular = j.at("Equirectangular");
  p = result;
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
  result.metalness = j.at("Metalness");
  result.tangent = j.at("Tangent");
  result.ambient_occlusion = j.at("Ambient Occlusion");
  result.emissive = j.at("Emissive");
  result.vertex_shader = j.at("Vertex Shader").get<std::string>();
  result.frag_shader = j.at("Fragment Shader").get<std::string>();
  result.uv_scale = j.at("UV Scale");
  result.albedo_alpha_override = j.at("Albedo Alpha Override");
  result.backface_culling = j.at("Backface Culling");
  result.uses_transparency = j.at("Uses Transparency");
  result.casts_shadows = j.at("Casts Shadows");
  p = result;
}

void to_json(json &result, const std::vector<vec3> &p)
{
  json j;
  for (auto &v : p)
  {
    j.push_back(v);
  }
  result = j;
}
void from_json(const json &j, std::vector<vec3> &p)
{
  std::vector<vec3> v;
  for (json i : j)
  {
    v.push_back(i);
  }
  p = v;
}

void to_json(json &result, const std::vector<vec2> &p)
{
  json j;
  for (auto &v : p)
  {
    j.push_back(v);
  }
  result = j;
}
void from_json(const json &j, std::vector<vec2> &p)
{
  std::vector<vec2> v;
  for (json i : j)
  {
    v.push_back(i);
  }
  p = v;
}
void to_json(json &result, const Mesh_Data &p)
{
  json j;
  j["Name"] = p.name;
  j["Positions"] = p.positions;
  j["Normals"] = p.normals;
  j["Texture_Coordinates"] = p.texture_coordinates;
  j["Tangents"] = p.tangents;
  j["Bitangents"] = p.bitangents;
  j["Indices"] = p.indices;
  result = j;
}
void from_json(const json &j, Mesh_Data &p)
{
  Mesh_Data result;
  result.name = j.at("Name").get<std::string>();
  result.positions = j.at("Positions").get<std::vector<vec3>>();
  result.normals = j.at("Normals").get<std::vector<vec3>>();
  result.texture_coordinates = j.at("Texture_Coordinates").get<std::vector<vec2>>();
  result.tangents = j.at("Tangents").get<std::vector<vec3>>();
  result.bitangents = j.at("Bitangents").get<std::vector<vec3>>();
  result.indices = j.at("Indices").get<std::vector<uint32>>();
  p = result;
}
void to_json(json &result, const Mesh_Descriptor &p)
{
  json j;
  j["Name"] = p.name;
  j["Assimp_Filename"] = p.assimp_filename;
  j["Assimp_Index"] = p.assimp_index;
  j["Primitive"] = p.primitive;
  j["Mesh_Data"] = p.mesh_data;
  result = j;
}

void from_json(const json &j, Mesh_Descriptor &p)
{
  Mesh_Descriptor result;
  std::string name = j.at("Name");
  result.name = name;
  result.assimp_filename = j.at("Assimp_Filename").get<std::string>();
  result.assimp_index = j["Assimp_Index"].get<uint32>();
  result.primitive = j.at("Primitive");
  result.mesh_data = j.at("Mesh_Data");
  p = result;
}

void to_json(json &result, const Light_Array &p)
{
  json j;
  j["Lights"] = p.lights;
  j["Light Count"] = p.light_count;
  j["Environment"] = p.environment;
  result = j;
}

void from_json(const json &j, Light_Array &p)
{
  Light_Array l;
  l.lights = j.at("Lights");
  l.light_count = j.at("Light Count");
  l.environment = j.at("Environment");
  p = l;
}
