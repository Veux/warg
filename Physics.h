#pragma once
#include "Globals.h"
#include "Scene_Graph.h"
#include <glm/glm.hpp>
#include <vector>
using namespace glm;

struct Triangle
{
  vec3 a, b, c;
};

struct Ellipsoid
{
  vec3 c, r;
};

struct Plane
{
  float equation[4];
  vec3 origin;
  vec3 normal;

  Plane() {}
  Plane(const vec3 &origin, const vec3 &normal);
  Plane(const Triangle &t);

  bool is_facing(const vec3 &direction) const;
  float signed_distance_to(const vec3 &p) const;
};

struct Map
{
  Node_Ptr node;
  Mesh_Data mesh;
  Material_Descriptor material;

  std::vector<Triangle> surfaces; // trash
  vec3 spawn_pos[2];
  vec3 spawn_dir[2];
};

struct Collision_Packet
{
  vec3 e_radius;

  vec3 vel_r3;
  vec3 pos_r3;

  vec3 vel;
  vec3 vel_normalized;
  vec3 base_point;

  bool found_collision;
  float nearest_distance;
  vec3 intersection_point;
};

bool check_triangle(Collision_Packet *colpkt, Triangle &tri);

bool ray_intersects_triangle(
    vec3 origin, vec3 dir, Triangle tri, vec3 *intersection_point);
