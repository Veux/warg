#include "stdafx.h"
#include "Mesh_Loader.h"
#include "Globals.h"
#include "Render.h"

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
    dst.push_back(vec3(src[i].x, src[i].y, src[i].z));
}
void copy_mesh_data(std::vector<vec2> &dst, aiVector3D *src, uint32 length)
{
  ASSERT(dst.size() == 0);
  dst.reserve(length);
  for (uint32 i = 0; i < length; ++i)
    dst.push_back(vec2(src[i].x, src[i].y));
}

Mesh_Descriptor build_mesh_descriptor(const aiScene *scene, uint32 i, std::string path)
{
  Mesh_Descriptor d;
  const aiMesh *aimesh = scene->mMeshes[i];
  ASSERT(aimesh);
  ASSERT(aimesh->HasNormals());
  ASSERT(aimesh->HasPositions());
  ASSERT(aimesh->GetNumUVChannels() == 1);
  ASSERT(aimesh->HasTextureCoords(0));
  if (!aimesh->HasTangentsAndBitangents())
  {
    const aiScene *s = aiApplyPostProcessing(scene, aiProcess_CalcTangentSpace);
    std::string err = aiGetErrorString();
    ASSERT(s == scene); // post process failed
  }
  ASSERT(aimesh->HasTangentsAndBitangents());
  ASSERT(aimesh->HasVertexColors(0) == false); // TODO: add support for vertex colors
  ASSERT(aimesh->mNumUVComponents[0] == 2);

  d.name = copy(&aimesh->mName);

  const int32 num_vertices = aimesh->mNumVertices;
  // const float32 cm_to_meters = 0.01;
  d.mesh_data.positions.reserve(num_vertices);
  for (uint32 i = 0; i < num_vertices; ++i)
  {
    d.mesh_data.positions.push_back(vec3(aimesh->mVertices[i].x, aimesh->mVertices[i].y, aimesh->mVertices[i].z));
    // d.mesh_data.positions.push_back(
    //   cm_to_meters * vec3(aimesh->mVertices[i].x, aimesh->mVertices[i].y, aimesh->mVertices[i].z));
  }

  copy_mesh_data(d.mesh_data.normals, aimesh->mNormals, num_vertices);
  copy_mesh_data(d.mesh_data.tangents, aimesh->mTangents, num_vertices);
  copy_mesh_data(d.mesh_data.bitangents, aimesh->mBitangents, num_vertices);

  const uint32 uv_channel = 0; // only one uv channel supported
  copy_mesh_data(d.mesh_data.texture_coordinates, aimesh->mTextureCoords[uv_channel], num_vertices);

  for (uint32 i = 0; i < aimesh->mNumFaces; i++)
  {
    aiFace face = aimesh->mFaces[i];
    if (face.mNumIndices == 3)
    {
      for (GLuint j = 0; j < 3; j++)
      {
        d.mesh_data.indices.push_back(face.mIndices[j]);
      }
    }
    else
    {
      // set_message("non-triangle error", "", 5);
    }
  }
  if (d.mesh_data.indices.size() < 3)
  {
    set_message("Error: No triangles in mesh:", d.name);
    ASSERT(0); // will fail to upload
  }
  return d;
}
