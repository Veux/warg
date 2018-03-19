#include "Globals.h"
#include <vector>
#include <glm/glm.hpp>
#include "Mesh_Loader.h"
#include "Render.h"
#include <assimp/types.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
 

void add_triangle(vec3 a, vec3 b, vec3 c, Mesh_Data& mesh)
{
  std::vector<vec3> pos =
  {
    a, b, c
  };
  vec3 atob = b - a;
  vec3 atoc = c - a;
  vec3 normal = cross(atoc, atob);
  std::vector<vec2> uvs =
  {
    { 0,0 },{ 0,1 },{ 1,1 }
  };
  vec2 atob_uv = vec2(0, 1) - vec2(0, 0);
  vec2 atoc_uv = vec2(1, 1) - vec2(0, 0);
  float32 t = 1.0f / (atob_uv.x * atoc_uv.y - atoc_uv.x - atob_uv.y);
  vec3 tangent =
  {
    t*(atoc_uv.y * atob.x - atob_uv.y * atoc.x),
    t*(atoc_uv.y * atob.y - atob_uv.y * atoc.y),
    t*(atoc_uv.y * atob.z - atob_uv.y * atoc.z)
  };
  tangent = normalize(tangent);
  vec3 bitangent =
  {
    t*(-atoc_uv.x * atob.x + atob_uv.x * atoc.x),
    t*(-atoc_uv.x * atob.y + atob_uv.x * atoc.y),
    t*(-atoc_uv.x * atob.z + atob_uv.x * atoc.z)
  };
  bitangent = normalize(bitangent);
  std::vector<vec3> tan =
  {
    tangent,tangent,tangent
  };
  std::vector<vec3> bitan =
  {
    bitangent,bitangent,bitangent
  };
  int32 base = mesh.positions.size();
  std::vector<int32> ind =
  {
    base + 0, base + 1, base + 2
  };
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

void add_quad(vec3 a, vec3 b, vec3 c, vec3 d,
	Mesh_Data& mesh)
{
	std::vector<vec3> pos =
	{
		a, b, c,
		a, c, d
	};
	vec3 atob = b - a;
	vec3 atoc = c - a;
	vec3 normal = cross(atoc, atob);
	std::vector<vec2> uvs =
	{
		{0,0}, {0,1}, {1,1},
		{0,0}, {1,1}, {1,0}
	};
  vec2 atob_uv = vec2(0, 1) - vec2(0, 0);
  vec2 atoc_uv = vec2(1, 1) - vec2(0, 0);
  float32 t = 1.0f / (atob_uv.x * atoc_uv.y - atoc_uv.x - atob_uv.y);
  vec3 tangent =
  {
    t*(atoc_uv.y * atob.x - atob_uv.y * atoc.x),
    t*(atoc_uv.y * atob.y - atob_uv.y * atoc.y),
    t*(atoc_uv.y * atob.z - atob_uv.y * atoc.z)
  };
  tangent = normalize(tangent);
  vec3 bitangent =
  {
    t*(-atoc_uv.x * atob.x + atob_uv.x * atoc.x),
    t*(-atoc_uv.x * atob.y + atob_uv.x * atoc.y),
    t*(-atoc_uv.x * atob.z + atob_uv.x * atoc.z)
  };
  bitangent = normalize(bitangent);
	std::vector<vec3> tan =
	{
    tangent,tangent,tangent,
    tangent,tangent,tangent
	};
	std::vector<vec3> bitan =
	{
    bitangent,bitangent,bitangent,
    bitangent,bitangent,bitangent,
	};
	int32 base = mesh.positions.size();
	std::vector<int32> ind =
	{
		base + 0, base + 1, base + 2,
		base + 3, base + 4, base + 5
	};
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
  cube.name = "cube";
  cube.unique_identifier = to_string(Mesh_Primitive::cube);
	vec3 a, b, c, d;

	//top
	a = { -0.5, -0.5, 0.5 };
	b = { -0.5, 0.5, 0.5 };
	c = { 0.5, 0.5, 0.5 };
	d = { 0.5, -0.5, 0.5 };
	add_quad(a, b, c, d, cube);

	//bottom
	a = { -0.5, -0.5, -0.5 };
	b = { 0.5, -0.5, -0.5 };
	c = { 0.5, 0.5, -0.5 };
	d = { -0.5, 0.5, -0.5 };
	add_quad(a, b, c, d, cube);

	//left
	a = { -0.5, 0.5, -0.5 };
	b = { -0.5, 0.5, 0.5 };
	c = { -0.5, -0.5, 0.5 };
	d = { -0.5, -0.5, -0.5 };
	add_quad(a, b, c, d, cube);

	//right
	a = { 0.5, -0.5, -0.5 };
	b = { 0.5, -0.5, 0.5 };
	c = { 0.5, 0.5, 0.5 };
	d = { 0.5, 0.5, -0.5 };
	add_quad(a, b, c, d, cube);

	//front
	a = { -0.5, -0.5, -0.5 };
	b = { -0.5, -0.5, 0.5 };
	c = { 0.5, -0.5, 0.5 };
	d = { 0.5, -0.5, -0.5 };
	add_quad(a, b, c, d, cube);

	//back
	a = { 0.5, 0.5, -0.5 };
	b = { 0.5, 0.5, 0.5 };
	c = { -0.5, 0.5, 0.5 };
	d = { -0.5, 0.5,-0.5 };
	add_quad(a, b, c, d, cube);

	return cube;
}

Mesh_Data load_mesh_plane()
{
  set_message("building plane mesh_data");
	Mesh_Data mesh;
  mesh.name = "plane";
  mesh.unique_identifier = to_string(Mesh_Primitive::plane);
  mesh.positions =
	{
		{-0.5,-0.5,0}, {-0.5,0.5,0}, {0.5,0.5,0},
		{-0.5,-0.5,0}, {0.5,0.5,0}, {0.5,-0.5,0}
	};
  mesh.texture_coordinates =
	{
		{0,0}, {0,1}, {1,1},
    {0,0}, {1,1}, {1,0}
	};
  mesh.indices =
	{
		0,1,2,
    3,4,5
	};
  mesh.normals =
	{
		{0,0,1},{0,0,1},{0,0,1},
		{0,0,1},{0,0,1},{0,0,1}
	};
  mesh.tangents =
	{
		{ 1,0,0 },{ 1,0,0 },{ 1,0,0 },
		{ 1,0,0 },{ 1,0,0 },{ 1,0,0 }
	};
  mesh.bitangents =
	{
		{ 0,1,0 },{ 0,1,0 },{ 0,1,0 },
		{ 0,1,0 },{ 0,1,0 },{ 0,1,0 },
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

void copy_mesh_data(std::vector<vec3>& dst, aiVector3D* src, uint32 length)
{
  ASSERT(dst.size() == 0);
  dst.reserve(length);
  for (uint32 i = 0; i < length; ++i)
	  dst.push_back(vec3(src[i].x,src[i].y,src[i].z));
}
void copy_mesh_data(std::vector<vec2>& dst, aiVector3D* src, uint32 length)
{
  ASSERT(dst.size() == 0);
  dst.reserve(length);
  for (uint32 i = 0; i < length; ++i)
	  dst.push_back(vec2(src[i].x, src[i].y));
}

Mesh_Data load_mesh(const aiMesh* aimesh, std::string unique_identifier)
{
  ASSERT(aimesh);
  ASSERT(aimesh->HasNormals());
  ASSERT(aimesh->HasPositions());
  ASSERT(aimesh->GetNumUVChannels() == 1);
  ASSERT(aimesh->HasTextureCoords(0));
  ASSERT(aimesh->HasTangentsAndBitangents());
  ASSERT(aimesh->HasVertexColors(0) == false); //TODO: add support for this
  ASSERT(aimesh->mNumUVComponents[0] == 2); //TODO: add support for UVW cubemaps
  Mesh_Data data;
  data.unique_identifier = unique_identifier;

  for (uint32 i = 0; i < aimesh->mName.length; ++i)
    data.name.push_back(aimesh->mName.data[i]);

  const int32 num_vertices = aimesh->mNumVertices;
  copy_mesh_data(data.positions, aimesh->mVertices, num_vertices);
  copy_mesh_data(data.normals, aimesh->mNormals, num_vertices);
  copy_mesh_data(data.tangents, aimesh->mTangents, num_vertices);
  copy_mesh_data(data.bitangents, aimesh->mBitangents, num_vertices);

  const uint32 uv_channel = 0;//only one uv channel supported
  copy_mesh_data(data.texture_coordinates, aimesh->mTextureCoords[uv_channel], num_vertices);

  for (uint32 i = 0; i < aimesh->mNumFaces; i++)
  {
    aiFace face = aimesh->mFaces[i];
    if (face.mNumIndices == 3)
    {
      for (GLuint j = 0; j < 3; j++)
      {
        data.indices.push_back(face.mIndices[j]);
      }
    }
    else
    {
      set_message("non-triangle warning", "", 5);
    }
  }
  return data;
}
