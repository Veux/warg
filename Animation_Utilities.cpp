#pragma once
#include "stdafx.h"
#include "Animation_Utilities.h"
#include "Warg_Common.h"

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

void fire_emitter(Renderer *renderer, Particle_Emitter *pe, Light *l, vec3 pos, vec2 size)
{
  Particle_Emitter_Descriptor *ped = &pe->descriptor;
  const float32 time = (float32)get_real_time();

  l->color = vec3(1.0f, .1f, .1f);
  l->position.z = 1.0f;
  l->ambient = 0.002f;
  l->attenuation.z = 0.06f;

  const float32 area = size.x * size.y;
  pe->descriptor.position = pos;
  ped->emission_descriptor.initial_position_variance = vec3(size, .1f);
  ped->emission_descriptor.initial_scale = vec3(0.035f);
  ped->emission_descriptor.initial_extra_scale_variance = vec3(0.021f);
  ped->emission_descriptor.initial_velocity = vec3(0, 0, 6);
  ped->emission_descriptor.particles_per_second = area * 3500.f;
  ped->emission_descriptor.minimum_time_to_live = .315f;
  ped->emission_descriptor.extra_time_to_live_variance = .3215f;
  ped->physics_descriptor.type = wind;

  ped->emission_descriptor.initial_velocity_variance =
      vec3(abs(2.15f * sin(2 * time)), abs(2.15f * cos(1.5 * time)), abs((1.3f + sin(time)) * 1.15f));

  ped->emission_descriptor.initial_velocity_variance = vec3(2, 2, 2);

  ped->physics_descriptor.wind_intensity = random_between(11.f, 25.f);
  static vec3 wind_dir;
  if (fract(sin(time)) > .5)
    wind_dir = vec3(.575, .575, .325) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

  ped->physics_descriptor.direction = wind_dir;
  l->brightness = area * random_between(13.f, 16.f);
  l->position = pos;
  pe->update(renderer->projection, renderer->camera, dt);
}

void fire_emitter2(Renderer *renderer, Flat_Scene_Graph *scene, Particle_Emitter *pe, Light *l, vec3 pos, vec2 size)
{
  Particle_Emitter_Descriptor *ped = &pe->descriptor;
  const float32 time = (float32)get_real_time();

  //l->color = vec3(1.0f, .1f, .1f);
  //l->position.z = 1.0f;
  //l->ambient = 0.002f;
  //l->attenuation.z = 0.06f;

  const float32 area = size.x * size.y;
  pe->descriptor.position = pos;
  //ped->emission_descriptor.initial_position_variance = vec3(0);
  // vec3(size, .1f);
  ped->emission_descriptor.initial_scale = vec3(.750612835f);
  //ped->emission_descriptor.initial_extra_scale_variance = vec3(.005121315151f);
  ped->emission_descriptor.initial_velocity = vec3(0, 0, 7);
  ped->emission_descriptor.particles_per_second = 125.f;
  ped->emission_descriptor.minimum_time_to_live = 35.315f;
  //ped->emission_descriptor.extra_time_to_live_variance = 03; // 0.053215f;
  ped->emission_descriptor.randomized_orientation_angle_variance = 0;
  ped->physics_descriptor.type = wind;

  // ped->emission_descriptor.initial_velocity_variance =
  //    vec3(abs(5.15f * sin(2 * time)), abs(5.15f * cos(1.5 * time)), abs((1.3f + sin(time)) * 5.15f));

  ped->emission_descriptor.initial_velocity_variance = vec3(1, 1, 00.1);

  ped->physics_descriptor.wind_intensity = random_between(21.f, 55.f);
  static vec3 wind_dir;
  if (fract(sin(time)) > .5)
    wind_dir = vec3(.575, .575, .325) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

  if (fract(sin(time)) > .5)
    wind_dir = vec3(.575, .575, 0) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

  
  //wind_dir = vec3(0);
  ped->physics_descriptor.direction = wind_dir;
  l->brightness = area * random_between(13.f, 16.f);
  l->position = pos;

  pe->descriptor.physics_descriptor.static_octree = &scene->collision_octree;

  pe->update(renderer->projection, renderer->camera, dt);

  Material_Descriptor *md = scene->resource_manager->material_pool[pe->material_index].get_modifiable_descriptor();
  md->uniform_set.vec3_uniforms["emitter_location"] = pos;
}