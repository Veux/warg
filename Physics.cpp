#include "Physics.h"

Plane::Plane(const vec3 &origin, const vec3 &normal)
{
  this->normal = normal;
  this->origin = origin;
  equation[0] = normal.x;
  equation[1] = normal.y;
  equation[2] = normal.z;
  equation[3] =
      -(normal.x * origin.x + normal.y * origin.y + normal.z * origin.z);
}

Plane::Plane(const Triangle &t)
{
  normal = cross(t.b - t.a, t.c - t.a);
  normal = normalize(normal);
  origin = t.a;

  equation[0] = normal.x;
  equation[1] = normal.y;
  equation[2] = normal.z;
  equation[3] =
      -(normal.x * origin.x + normal.y * origin.y + normal.z * origin.z);
}

bool Plane::is_facing(const vec3 &direction) const
{
  return dot(normal, direction) <= 0;
}

float Plane::signed_distance_to(const vec3 &p) const
{
  return dot(p, normal) + equation[3];
}

float squared_length(vec3 v)
{
  float l = length(v);
  return l * l;
}

float squared_length(float f)
{
  float l = length(f);
  return l * l;
}

bool check_point_in_triangle(vec3 &p, Triangle &t)
{
  vec3 e10 = t.b - t.a;
  vec3 e20 = t.c - t.a;

  float a = dot(e10, e10);
  float b = dot(e10, e20);
  float c = dot(e20, e20);
  float ac_bb = a * c - b * b;
  vec3 vp = vec3(p.x - t.a.x, p.y - t.a.y, p.z - t.a.z);

  float d = dot(vp, e10);
  float e = dot(vp, e20);
  float x = d * c - e * b;
  float y = e * a - d * b;
  float z = x + y - ac_bb;

  return (((uint32 &)z & ~((uint32 &)x | (uint32 &)y)) & 0x80000000);
}

bool get_lowest_root(float a, float b, float c, float max_r, float *root)
{
  float determinant = b * b - 4.0f * a * c;

  if (determinant < 0.0f)
    return false;

  float sqrt_d = sqrt(determinant);
  float r1 = (-b - sqrt_d) / (2 * a);
  float r2 = (-b + sqrt_d) / (2 * a);

  if (r1 > r2)
  {
    float temp = r2;
    r2 = r1;
    r1 = temp;
  }

  if (r1 > 0 && r1 < max_r)
  {
    *root = r1;
    return true;
  }

  if (r2 > 0 && r2 < max_r)
  {
    *root = r2;
    return true;
  }

  return false;
}

bool check_triangle(Collision_Packet *colpkt, Triangle &tri)
{
  Plane triangle_plane = Plane(tri);

  if (!triangle_plane.is_facing(colpkt->vel_normalized))
    return false;

  float t0, t1;
  bool embedded_in_plane = false;

  float signed_dist_to_triangle_plane =
      triangle_plane.signed_distance_to(colpkt->base_point);

  float normal_dot_vel = dot(triangle_plane.normal, colpkt->vel);

  if (normal_dot_vel == 0.0f)
  {
    if (fabs(signed_dist_to_triangle_plane) >= 1.0f)
    {
      return false;
    }
    else
    {
      embedded_in_plane = true;
      t0 = 0.0;
      t1 = 1.0;
    }
  }
  else
  {
    t0 = (-1.0 - signed_dist_to_triangle_plane) / normal_dot_vel;
    t1 = (1.0 - signed_dist_to_triangle_plane) / normal_dot_vel;

    if (t0 > t1)
    {
      float temp = t1;
      t1 = t0;
      t0 = temp;
    }

    if (t0 > 1.0f || t1 < 0.0f)
    {
      return false;
    }

    if (t0 < 0.0)
      t0 = 0.0;
    if (t1 < 0.0)
      t1 = 0.0;
    if (t0 > 1.0)
      t0 = 1.0;
    if (t1 > 1.0)
      t1 = 1.0;
  }

  vec3 collision_point;
  bool found_collision = false;
  float t = 1.0;

  if (!embedded_in_plane)
  {
    vec3 plane_intersection_point =
        (colpkt->base_point - triangle_plane.normal) + (colpkt->vel * t0);

    if (check_point_in_triangle(plane_intersection_point, tri))
    {
      found_collision = true;
      t = t0;
      collision_point = plane_intersection_point;
    }
  }

  if (!found_collision)
  {
    vec3 vel = colpkt->vel;
    vec3 base = colpkt->base_point;
    float vel_squared_length = squared_length(vel);
    float a, b, c;
    float new_t;

    a = vel_squared_length;

    b = 2.0 * dot(vel, base - tri.a);
    c = squared_length(tri.a - base) - 1.0;
    if (get_lowest_root(a, b, c, t, &new_t))
    {
      t = new_t;
      found_collision = true;
      collision_point = tri.a;
    }

    b = 2.0 * dot(vel, base - tri.b);
    c = squared_length(tri.b - base) - 1.0;
    if (get_lowest_root(a, b, c, t, &new_t))
    {
      t = new_t;
      found_collision = true;
      collision_point = tri.b;
    }

    b = 2.0 * dot(vel, base - tri.c);
    c = squared_length(tri.c - base) - 1.0;
    if (get_lowest_root(a, b, c, t, &new_t))
    {
      t = new_t;
      found_collision = true;
      collision_point = tri.c;
    }

    vec3 edge = tri.b - tri.a;
    vec3 base_to_vertex = tri.a - base;
    float edge_squared_length = squared_length(edge);
    float edge_dot_vel = dot(edge, vel);
    float edge_dot_base_to_vertex = dot(edge, base_to_vertex);

    a = edge_squared_length * -vel_squared_length + edge_dot_vel * edge_dot_vel;
    b = edge_squared_length * 2.0 * dot(vel, base_to_vertex) -
        2.0 * edge_dot_vel * edge_dot_base_to_vertex;
    c = edge_squared_length * (1 - squared_length(base_to_vertex)) +
        edge_dot_base_to_vertex * edge_dot_base_to_vertex;

    if (get_lowest_root(a, b, c, t, &new_t))
    {
      float f = (edge_dot_vel * new_t - edge_dot_base_to_vertex) /
                edge_squared_length;
      if (f >= 0.0 && f <= 1.0)
      {
        t = new_t;
        found_collision = true;
        collision_point = tri.a + f * edge;
      }
    }

    edge = tri.c - tri.b;
    base_to_vertex = tri.b - base;
    edge_squared_length = squared_length(edge);
    edge_dot_vel = dot(edge, vel);
    edge_dot_base_to_vertex = dot(edge, base_to_vertex);

    a = edge_squared_length * -vel_squared_length + edge_dot_vel * edge_dot_vel;
    b = edge_squared_length * 2.0 * dot(vel, base_to_vertex) -
        2.0 * edge_dot_vel * edge_dot_base_to_vertex;
    c = edge_squared_length * (1 - squared_length(base_to_vertex)) +
        edge_dot_base_to_vertex * edge_dot_base_to_vertex;

    if (get_lowest_root(a, b, c, t, &new_t))
    {
      float f = (edge_dot_vel * new_t - edge_dot_base_to_vertex) /
                edge_squared_length;
      if (f >= 0.0f && f <= 1.0f)
      {
        t = new_t;
        found_collision = true;
        collision_point = tri.b + f * edge;
      }
    }

    edge = tri.a - tri.c;
    base_to_vertex = tri.c - base;
    edge_squared_length = squared_length(edge);
    edge_dot_vel = dot(edge, vel);
    edge_dot_base_to_vertex = dot(edge, base_to_vertex);

    a = edge_squared_length * -vel_squared_length + edge_dot_vel * edge_dot_vel;
    b = edge_squared_length * 2.0 * dot(vel, base_to_vertex) -
        2.0 * edge_dot_vel * edge_dot_base_to_vertex;
    c = edge_squared_length * (1 - squared_length(base_to_vertex)) +
        edge_dot_base_to_vertex * edge_dot_base_to_vertex;

    if (get_lowest_root(a, b, c, t, &new_t))
    {
      float f = (edge_dot_vel * new_t - edge_dot_base_to_vertex) /
                edge_squared_length;
      if (f >= 0.0 && f <= 1.0)
      {
        t = new_t;
        found_collision = true;
        collision_point = tri.c + f * edge;
      }
    }
  }

  if (found_collision == true)
  {
    float dist_to_collision = t * length(colpkt->vel);

    if (!colpkt->found_collision ||
        dist_to_collision < colpkt->nearest_distance)
    {
      colpkt->nearest_distance = dist_to_collision;
      colpkt->intersection_point = collision_point;
      colpkt->found_collision = true;

	  return true;
    }
  }

  return false;
}
