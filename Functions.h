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

struct Triangle
{
  vec3 a, b, c;
};

struct Segment
{
  vec3 p, q;
};

struct Sphere
{
  vec3 c;
  float32 r;
};

struct Barbell
{
	Segment pq;
	float32 r;
};

struct Map
{
  std::vector<Triangle> surfaces;
  vec3 ground_pos;
  vec3 ground_dir;
  vec3 ground_dim;
  vec3 spawn_pos[2];
  vec3 spawn_dir[2];
};

bool intersects(Segment pq, Triangle abc, vec3 &uvw, float32 &t);
bool intersects(Sphere s, Triangle abc, vec3 &p);
bool intersects(Barbell b, Triangle abc);
vec3 closest_point(vec3 p, Triangle abc);
