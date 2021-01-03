#include "stdafx.h"
#include "Mesh_Loader.h"
#include "Globals.h"
#include "Render.h"

Mesh_Data generate_grid(ivec2 vertex_count)
{
  //assuming that tile_count meant vertex count for heightmaps, we need 1 fewer tile
  //than we want vertices, in order for them to match properly
  ivec2 tile_count = vertex_count -ivec2(1);
  Mesh_Data data;

  for (uint32 y = 0; y < (uint32)tile_count.y + 1; ++y)
  {

    for (uint32 x = 0; x < (uint32)tile_count.x + 1; ++x)
    {
      data.positions.push_back(vec3(vec2(x, y), 0));
      data.normals.push_back(vec3(0, 0, 1));
      data.tangents.push_back(vec3(1, 0, 0));
      data.bitangents.push_back(vec3(0, 1, 0));
      data.texture_coordinates.push_back(vec2(x, y) / vec2(tile_count));
    }
  }

  for (uint32 y = 0; y < (uint32)tile_count.y; ++y)
  {

    for (uint32 x = 0; x < (uint32)tile_count.x; ++x)
    {
      uint32 lower_left = y * ((uint32)tile_count.x+1) + x;
      uint32 lower_right = lower_left + 1;
      uint32 upper_left = lower_left + ((uint32)tile_count.x+1);
      uint32 upper_right = upper_left + 1;

      data.indices.push_back(upper_left);
      data.indices.push_back(lower_left);
      data.indices.push_back(lower_right);

      data.indices.push_back(upper_left);
      data.indices.push_back(lower_right);
      data.indices.push_back(upper_right);
    }
  }

  vec2 tile_size = vec2(1.f) / vec2(vertex_count);
  vec2 offset = vec2(-0.5f);
  for (uint32 i = 0; i < data.positions.size(); ++i)
  {
    data.positions[i] = vec3(tile_size, 1) * data.positions[i];
    data.positions[i] = data.positions[i] + vec3(offset, 0);
  }
  return data;
}
void add_triangle(vec3 a, vec3 b, vec3 c, Mesh_Data &mesh)
{
  std::vector<vec3> pos = {a, b, c};
  vec3 atob = b - a;
  vec3 atoc = c - a;
  vec3 normal = cross(atob, atoc);
  std::vector<vec2> uvs = {{0, 0}, {1, 1}, {0, 1}};
  vec2 atob_uv = vec2(1, 1);
  vec2 atoc_uv = vec2(0, 1);
  // float32 t = 1.0f / (atob_uv.x * atoc_uv.y - atoc_uv.x - atob_uv.y);
  // vec3 tangent = {t * (atoc_uv.y * atob.x - atob_uv.y * atoc.x), t * (atoc_uv.y * atob.y - atob_uv.y * atoc.y),
  //    t * (atoc_uv.y * atob.z - atob_uv.y * atoc.z)};

  float32 t = 1.0f / (atob_uv.x * atoc_uv.y - atob_uv.y * atoc_uv.x);
  glm::vec3 tangent = t * (atob * atoc_uv.y - atoc * atob_uv.y);
  glm::vec3 bitangent = t * (atoc * atob_uv.x - atob * atoc_uv.x);

  tangent = normalize(tangent);
  // vec3 bitangent = {t * (-atoc_uv.x * atob.x + atob_uv.x * atoc.x), t * (-atoc_uv.x * atob.y + atob_uv.x * atoc.y),
  //    t * (-atoc_uv.x * atob.z + atob_uv.x * atoc.z)};
  bitangent = normalize(bitangent);
  std::vector<vec3> tan = {tangent, tangent, tangent};
  std::vector<vec3> bitan = {bitangent, bitangent, bitangent};
  int32 end = (int32)mesh.positions.size();
  std::vector<int32> ind = {end + 0, end + 1, end + 2};
  mesh.tangents.insert(mesh.tangents.end(), tan.begin(), tan.end());
  mesh.bitangents.insert(mesh.bitangents.end(), bitan.begin(), bitan.end());
  mesh.positions.insert(mesh.positions.end(), pos.begin(), pos.end());
  for (uint32 i = 0; i < 3; ++i)
  {
    mesh.normals.push_back(normal);
  }
  mesh.indices.insert(mesh.indices.end(), ind.begin(), ind.end());
  mesh.texture_coordinates.insert(mesh.texture_coordinates.end(), uvs.begin(), uvs.end());
}

void add_quad(vec3 a, vec3 b, vec3 c, vec3 d, Mesh_Data &mesh)
{
  std::vector<vec3> pos = {a, b, c, a, c, d};
  vec3 atob = b - a;
  vec3 atoc = c - a;
  vec3 normal = cross(atob, atoc);
  std::vector<vec2> uvs = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
  vec2 atob_uv = vec2(1, 0);
  vec2 atoc_uv = vec2(1, 1);

  float32 t = 1.0f / (atob_uv.x * atoc_uv.y - atob_uv.y * atoc_uv.x);
  glm::vec3 tangent = t * (atob * atoc_uv.y - atoc * atob_uv.y);
  glm::vec3 bitangent = t * (atoc * atob_uv.x - atob * atoc_uv.x);

  // float32 t = 1.0f / (atob_uv.x * atoc_uv.y - atoc_uv.x - atob_uv.y);
  // vec3 tangent = {t * (atoc_uv.y * atob.x - atob_uv.y * atoc.x), t * (atoc_uv.y * atob.y - atob_uv.y * atoc.y),
  //    t * (atoc_uv.y * atob.z - atob_uv.y * atoc.z)};
  tangent = normalize(tangent);
  // vec3 bitangent = {t * (-atoc_uv.x * atob.x + atob_uv.x * atoc.x), t * (-atoc_uv.x * atob.y + atob_uv.x * atoc.y),
  //    t * (-atoc_uv.x * atob.z + atob_uv.x * atoc.z)};
  bitangent = normalize(bitangent);
  std::vector<vec3> tan = {tangent, tangent, tangent, tangent, tangent, tangent};
  std::vector<vec3> bitan = {
      bitangent,
      bitangent,
      bitangent,
      bitangent,
      bitangent,
      bitangent,
  };
  int32 end = (int32)mesh.positions.size();
  std::vector<int32> ind = {end + 0, end + 1, end + 2, end + 3, end + 4, end + 5};
  mesh.tangents.insert(mesh.tangents.end(), tan.begin(), tan.end());
  mesh.bitangents.insert(mesh.bitangents.end(), bitan.begin(), bitan.end());
  mesh.positions.insert(mesh.positions.end(), pos.begin(), pos.end());
  for (uint32 i = 0; i < 6; ++i)
  {
    mesh.normals.push_back(normal);
  }
  mesh.indices.insert(mesh.indices.end(), ind.begin(), ind.end());
  mesh.texture_coordinates.insert(mesh.texture_coordinates.end(), uvs.begin(), uvs.end());
}

Mesh_Data load_mesh_cube()
{
  Mesh_Data cube;
  vec3 a, b, c, d;

  // top
  a = {-0.5, -0.5, 0.5};
  b = {0.5, -0.5, 0.5};
  c = {0.5, 0.5, 0.5};
  d = {-0.5, 0.5, 0.5};
  add_quad(a, b, c, d, cube);

  // bottom
  a = {-0.5, -0.5, -0.5};
  b = {-0.5, 0.5, -0.5};
  c = {0.5, 0.5, -0.5};
  d = {0.5, -0.5, -0.5};
  add_quad(a, b, c, d, cube);

  // left
  a = {-0.5, 0.5, -0.5};
  b = {-0.5, -0.5, -0.5};
  c = {-0.5, -0.5, 0.5};
  d = {-0.5, 0.5, 0.5};
  add_quad(a, b, c, d, cube);

  // right
  a = {0.5, -0.5, -0.5};
  b = {0.5, 0.5, -0.5};
  c = {0.5, 0.5, 0.5};
  d = {0.5, -0.5, 0.5};
  add_quad(a, b, c, d, cube);

  // front
  a = {-0.5, -0.5, -0.5};
  b = {0.5, -0.5, -0.5};
  c = {0.5, -0.5, 0.5};
  d = {-0.5, -0.5, 0.5};
  add_quad(a, b, c, d, cube);

  // back
  a = {0.5, 0.5, -0.5};
  b = {-0.5, 0.5, -0.5};
  c = {-0.5, 0.5, 0.5};
  d = {0.5, 0.5, 0.5};
  add_quad(a, b, c, d, cube);

  return cube;
}

void add_aabb(vec3 min, vec3 max, Mesh_Data &mesh)
{

  vec3 a, b, c, d;

  // top
  a = {min.x, min.y, max.z};
  b = {min.x, max.y, max.z};
  c = {max.x, max.y, max.z};
  d = {max.x, min.y, max.z};
  add_quad(a, b, c, d, mesh);

  // bottom
  a = {min.x, min.y, min.z};
  b = {max.x, min.y, min.z};
  c = {max.x, max.y, min.z};
  d = {min.x, max.y, min.z};
  add_quad(a, b, c, d, mesh);

  // left
  a = {min.x, max.y, min.z};
  b = {min.x, max.y, max.z};
  c = {min.x, min.y, max.z};
  d = {min.x, min.y, min.z};
  add_quad(a, b, c, d, mesh);

  // right
  a = {max.x, min.y, min.z};
  b = {max.x, min.y, max.z};
  c = {max.x, max.y, max.z};
  d = {max.x, max.y, min.z};
  add_quad(a, b, c, d, mesh);

  // front
  a = {min.x, min.y, min.z};
  b = {min.x, min.y, max.z};
  c = {max.x, min.y, max.z};
  d = {max.x, min.y, min.z};
  add_quad(a, b, c, d, mesh);

  // back
  a = {max.x, max.y, min.z};
  b = {max.x, max.y, max.z};
  c = {min.x, max.y, max.z};
  d = {min.x, max.y, min.z};
  add_quad(a, b, c, d, mesh);
}

Mesh_Data load_mesh_plane()
{
  Mesh_Data mesh;
  mesh.positions = {{-0.5, -0.5, 0}, {0.5, -0.5, 0}, {0.5, 0.5, 0}, {-0.5, -0.5, 0}, {0.5, 0.5, 0}, {-0.5, 0.5, 0}};
  mesh.texture_coordinates = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
  mesh.indices = {0, 1, 2, 3, 4, 5};
  mesh.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  mesh.tangents = {{1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}};
  mesh.bitangents = {
      {0, 1, 0},
      {0, 1, 0},
      {0, 1, 0},
      {0, 1, 0},
      {0, 1, 0},
      {0, 1, 0},
  };
  return mesh;
}

Mesh_Data load_mesh(Mesh_Primitive p)
{
  if (p == cube)
    return load_mesh_cube();
  else if (p == plane)
    return load_mesh_plane();

  ASSERT(0);
  return Mesh_Data();
}

std::string to_string(Mesh_Primitive p)
{
  if (p == null)
  {
    return "null";
  }
  else if (p == plane)
  {
    return "generated plane";
  }
  else if (p == cube)
  {
    return "generated cube";
  }
  else
  {
    throw;
  }
  return "";
}

Mesh_Primitive s_to_primitive(std::string p)
{
  if (p == "null")
  {
    return null;
  }
  if (p == "generated plane")
  {
    return plane;
  }
  else if (p == "generated cube")
  {
    return cube;
  }
  else
  {
    throw;
  }
  return Mesh_Primitive();
}

void copy_mesh_data(std::vector<vec3> &dst, aiVector3D *src, uint32 length)
{
  ASSERT(dst.size() == 0);
  dst.reserve(length);
  for (uint32 i = 0; i < length; ++i)
    dst.emplace_back(src[i].x, src[i].y, src[i].z);
}
void copy_mesh_data(std::vector<vec2> &dst, aiVector3D *src, uint32 length)
{
  ASSERT(dst.size() == 0);
  dst.reserve(length);
  for (uint32 i = 0; i < length; ++i)
    dst.emplace_back(src[i].x, src[i].y);
}


