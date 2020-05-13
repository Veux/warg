#pragma once
#include "Globals.h"
#include <glm/glm.hpp>
#include <vector>
using namespace glm;
struct Flat_Scene_Graph;

struct Triangle
{
  vec3 a, b, c;
};

struct Triangle_Normal
{
  vec3 a, b, c;
  vec3 n;
  vec3 v;
};

std::vector<Triangle_Normal> collect_colliders_with_normal(Flat_Scene_Graph &scene);
//typedef std::vector<Triangle> Colliders;

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

bool ray_intersects_trianglet(vec3 origin, vec3 dir, const vec3 &v0, const vec3 &v1, const vec3 &v2, float *tptr);
bool ray_intersects_triangle(vec3 origin, vec3 dir, Triangle tri, vec3 *intersection_point);
bool ray_intersects_triangle(
    vec3 origin, vec3 dir, const vec3 &v0, const vec3 &v1, const vec3 &v2, vec3 *intersection_point);

bool vec3_has_nan(vec3 v);
