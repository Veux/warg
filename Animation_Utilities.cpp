#pragma once
#include "Animation_Utilities.h"

template <typename V, typename T> V lerp(V &v1, V &v2, T &t)
{
  return v1 + t * (v2 - v1);
}
float lerp(float v1, float v2, float t)
{
  return v1 + t * (v2 - v1);
}

// struct Bezier_Curve
//{
//  Bezier_Curve() {}
//  Bezier_Curve(std::vector<glm::vec4> pts) : points(pts) {}
//  glm::vec4 lerp(float t)
//  {
//    if (remainder.size() == 0)
//    {
//      remainder = points;
//    }
//    if (remainder.size() == 1)
//    {
//      glm::vec4 p = remainder[0];
//      remainder.clear();
//      return p;
//    }
//
//    for (uint32 i = 0; i + 1 < remainder.size(); ++i)
//    {
//      glm::vec4 p = glm::mix(remainder[i], remainder[i + 1], t);
//      remainder[i] = p;
//    }
//    remainder.pop_back();
//
//    return lerp(t);
//  }
//  std::vector<glm::vec4> points;
//
// private:
//  std::vector<glm::vec4> remainder;
//};

void fire_emitter(Renderer *renderer, Flat_Scene_Graph *scene, Particle_Emitter *pe, Light *l, vec3 pos, vec2 size)
{
  Particle_Emitter_Descriptor *ped = &pe->descriptor;
  const float32 time = get_real_time();
  static bool first = true;
  static Mesh_Index mesh_index;
  static Material_Index material_index;

  if (first)
  {
    Material_Descriptor material;
    material.vertex_shader = "instance.vert";
    material.frag_shader = "emission.frag";
    material.emissive = "color(1,1,1,1)";
    material.emissive.mod = vec4(15.f, 3.3f, .7f, 1.f);
    Node_Index particle_node = scene->add_mesh(cube, "fire particle", &material);
    mesh_index = scene->nodes[particle_node].model[0].first;
    material_index = scene->nodes[particle_node].model[0].second;
    scene->nodes[particle_node].visible = false;
    first = false;
  }
  l->color = vec3(1.0f, .1f, .1f);
  l->position.z = 1.0f;
  l->ambient = 0.002f;
  l->attenuation.z = 0.06f;

  const float32 area = size.x * size.y;
  pe->mesh_index = mesh_index;
  pe->material_index = material_index;
  pe->descriptor.position = pos;
  ped->emission_descriptor.initial_position_variance = vec3(size, .1f);
  ped->emission_descriptor.initial_scale = vec3(0.025f);
  ped->emission_descriptor.initial_scale_variance = vec3(0.01f);
  ped->emission_descriptor.initial_velocity = vec3(0, 0, 5);
  ped->emission_descriptor.particles_per_second = area * 5500.f;
  ped->emission_descriptor.time_to_live = .25f;
  ped->emission_descriptor.time_to_live_variance = 1.05f;
  ped->physics_descriptor.type = wind;

  ped->emission_descriptor.initial_velocity_variance =
      vec3(abs(3.5f * sin(2 * time)), abs(3.5f * cos(1.5 * time)), abs(1.f + sin(time) * 2.f));
  ped->physics_descriptor.intensity = random_between(11.f, 35.f);
  static vec3 dir;
  if (fract(sin(time)) > .5)
    dir = vec3(.75, .75, .25) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

  ped->physics_descriptor.direction = dir;
  l->brightness = area * random_between(13.f, 16.f);
  l->position = pos;
  pe->update(renderer->projection, renderer->camera, dt);
}