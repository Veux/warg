#pragma once
#include <glm/glm.hpp>
#include <vector>
using namespace glm;

struct Cylinder
{
  float32 h, r;
};

struct Wall
{
  vec3 p1;
  vec2 p2;
  float32 h;
};

struct Map
{
  std::vector<Wall> walls;
  vec3 ground_pos;
  vec3 ground_dir;
  vec3 ground_dim;
  vec3 spawn_pos[2];
  vec3 spawn_dir[2];
};

bool intersects(vec3 pa, vec3 da, vec3 pb, vec3 db);
bool intersects(vec3 pa, Cylinder ca, vec3 pb, Cylinder cb);
bool intersects(vec3 pa, Cylinder ca, Wall w);
