#include "Warg_Common.h"

std::vector<Triangle> collect_colliders(const Scene_Graph &scene);
void check_collision(
    Collision_Packet &colpkt, const std::vector<Triangle> &colliders);
vec3 collide_char_with_world(Character &character, const vec3 &pos,
    const vec3 &vel, const std::vector<Triangle> &colliders);
void collide_and_slide_char(Character &character, const vec3 &vel,
    const vec3 &gravity, const std::vector<Triangle> &colliders);

Map make_blades_edge()
{
  Map blades_edge;

  // spawns
  blades_edge.spawn_pos[0] = {5, 5, 5};
  blades_edge.spawn_pos[1] = {45, 45, 5};
  blades_edge.spawn_dir[0] = {0, 1, 0};
  blades_edge.spawn_dir[1] = {0, -1, 0};

  blades_edge.mesh.unique_identifier = "blades_edge_map";
  blades_edge.material.backface_culling = false;
  blades_edge.material.albedo = "crate_diffuse.png";
  blades_edge.material.emissive = "";
  blades_edge.material.normal = "crate_normal.png";
  blades_edge.material.roughness = "crate_roughness.png";
  blades_edge.material.vertex_shader = "vertex_shader.vert";
  blades_edge.material.frag_shader = "fragment_shader.frag";
  blades_edge.material.casts_shadows = true;
  blades_edge.material.uv_scale = vec2(16);

  return blades_edge;
}

void Character::update_hp(float32 dt)
{
  hp += e_stats.hp_regen * dt;
  if (hp > hp_max)
    hp = hp_max;
}

void Character::update_mana(float32 dt)
{
  mana += e_stats.mana_regen * dt;

  if (mana > mana_max)
    mana = mana_max;
}

void Character::update_spell_cooldowns(float32 dt)
{
  for (auto &spell_ : spellbook)
  {
    auto &spell = spell_.second;
    if (spell.cd_remaining > 0)
      spell.cd_remaining -= dt;
    if (spell.cd_remaining < 0)
      spell.cd_remaining = 0;
  }
}

void Character::update_global_cooldown(float32 dt)
{
  gcd -= dt;
  if (gcd < 0)
    gcd = 0;
}

void Character::apply_modifier(CharMod &modifier)
{
  switch (modifier.type)
  {
    case CharModType::DamageTaken:
      e_stats.damage_mod *= modifier.damage_taken.n;
      break;
    case CharModType::Speed:
      e_stats.speed *= modifier.speed.m;
      break;
    case CharModType::CastSpeed:
      e_stats.cast_speed *= modifier.cast_speed.m;
    case CharModType::Silence:
      silenced = true;
    default:
      break;
  }
}

void Character::apply_modifiers()
{
  e_stats = b_stats;
  silenced = false;

  for (auto &buff : buffs)
    for (auto &modifier : buff.def.char_mods)
      apply_modifier(*modifier);
  for (auto &debuff : debuffs)
    for (auto &modifier : debuff.def.char_mods)
      apply_modifier(*modifier);
}

void Character::move(float32 dt, const std::vector<Triangle> &colliders)
{
  vec3 v = vec3(0);
  if (move_status & Move_Status::Forwards)
    v += vec3(dir.x, dir.y, 0);
  else if (move_status & Move_Status::Backwards)
    v += -vec3(dir.x, dir.y, 0);
  if (move_status & Move_Status::Left)
  {
    mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
    vec4 v_ = vec4(dir.x, dir.y, 0, 0);
    v += vec3(r * v_);
  }
  else if (move_status & Move_Status::Right)
  {
    mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
    vec4 v_ = vec4(dir.x, dir.y, 0, 0);
    v += vec3(r * v_);
  }
  if (move_status)
  {
    v = normalize(v);
    v *= e_stats.speed;
  }

  vel.z -= 9.81 * dt;
  if (vel.z < -53)
    vel.z = -53;
  if (grounded)
    vel.z = 0;

  collide_and_slide_char(*this, v * dt, vec3(0, 0, vel.z) * dt, colliders);
}

std::vector<Triangle> collect_colliders(Scene_Graph &scene)
{
  std::vector<Triangle> collider_cache;

  auto entities = scene.visit_nodes_st_start();
  for (auto &entity : entities)
  {
    auto transform = [&](vec3 p) {
      vec4 q = vec4(p.x, p.y, p.z, 1.0) * entity.transformation;
      return vec3(q.x, q.y, q.z);
    };
    auto &mesh_data = entity.mesh->mesh->data;
    for (int i = 0; i < mesh_data.indices.size(); i += 3)
    {
      uint32 a, b, c;
      a = mesh_data.indices[i];
      b = mesh_data.indices[i + 1];
      c = mesh_data.indices[i + 2];
      Triangle t;
      t.a = mesh_data.positions[a];
      t.c = mesh_data.positions[b];
      t.b = mesh_data.positions[c];
      collider_cache.push_back(t);
    }
  }

  return collider_cache;
}

void check_collision(
    Collision_Packet &colpkt, const std::vector<Triangle> &colliders)
{
  for (auto &surface : colliders)
  {
    Triangle t;
    t.a = surface.a / colpkt.e_radius;
    t.b = surface.b / colpkt.e_radius;
    t.c = surface.c / colpkt.e_radius;
    check_triangle(&colpkt, t);
  }
}

vec3 collide_char_with_world(Character &character, const vec3 &pos,
    const vec3 &vel, const std::vector<Triangle> &colliders)
{
  float epsilon = 0.005f;

  if (character.collision_recursion_depth > 5)
    return pos;

  character.colpkt.vel = vel;
  character.colpkt.vel_normalized = normalize(character.colpkt.vel);
  character.colpkt.base_point = pos;
  character.colpkt.found_collision = false;

  check_collision(character.colpkt, colliders);

  if (!character.colpkt.found_collision)
  {
    return pos + vel;
  }

  vec3 destination_point = pos + vel;
  vec3 new_base_point = pos;

  if (character.colpkt.nearest_distance >= epsilon)
  {
    vec3 v = vel;
    v = normalize(v) * (character.colpkt.nearest_distance - epsilon);
    new_base_point = character.colpkt.base_point + v;

    v = normalize(v);
    character.colpkt.intersection_point -= epsilon * v;
  }

  vec3 slide_plane_origin = character.colpkt.intersection_point;
  vec3 slide_plane_normal =
      new_base_point - character.colpkt.intersection_point;
  slide_plane_normal = normalize(slide_plane_normal);
  Plane sliding_plane = Plane(slide_plane_origin, slide_plane_normal);

  vec3 new_destination_point =
      destination_point -
      sliding_plane.signed_distance_to(destination_point) * slide_plane_normal;

  vec3 new_vel_vec =
      new_destination_point - character.colpkt.intersection_point;

  if (length(new_vel_vec) < epsilon)
  {
    return new_base_point;
  }

  character.collision_recursion_depth++;
  return collide_char_with_world(character, new_base_point, new_vel_vec, colliders);
}

void collide_and_slide_char(Character &character, const vec3 &vel,
    const vec3 &gravity, const std::vector<Triangle> &colliders)
{
  character.colpkt.e_radius = character.radius;
  character.colpkt.pos_r3 = character.pos;
  character.colpkt.vel_r3 = vel;

  vec3 e_space_pos = character.colpkt.pos_r3 / character.colpkt.e_radius;
  vec3 e_space_vel = character.colpkt.vel_r3 / character.colpkt.e_radius;

  character.collision_recursion_depth = 0;

  vec3 final_pos = collide_char_with_world(character, e_space_pos, e_space_vel, colliders);

  if (character.grounded)
  {
    character.colpkt.pos_r3 = final_pos * character.colpkt.e_radius;
    character.colpkt.vel_r3 = vec3(0, 0, -0.5);
    character.colpkt.vel = vec3(0, 0, -0.5) / character.colpkt.e_radius;
    character.colpkt.vel_normalized = normalize(character.colpkt.vel);
    character.colpkt.base_point = final_pos;
    character.colpkt.found_collision = false;

    check_collision(character.colpkt, colliders);
    if (character.colpkt.found_collision &&
        character.colpkt.nearest_distance > 0.05)
      final_pos.z -= character.colpkt.nearest_distance - 0.005;
  }

  character.colpkt.pos_r3 = final_pos * character.colpkt.e_radius;
  character.colpkt.vel_r3 = gravity;

  e_space_vel = gravity / character.colpkt.e_radius;
  character.collision_recursion_depth = 0;

  final_pos = collide_char_with_world(character, final_pos, e_space_vel, colliders);

  character.colpkt.pos_r3 = final_pos * character.colpkt.e_radius;
  character.colpkt.vel_r3 = vec3(0, 0, -0.05);
  character.colpkt.vel = vec3(0, 0, -0.05) / character.colpkt.e_radius;
  character.colpkt.vel_normalized = normalize(character.colpkt.vel);
  character.colpkt.base_point = final_pos;
  character.colpkt.found_collision = false;

  check_collision(character.colpkt, colliders);

  character.grounded = character.colpkt.found_collision && gravity.z <= 0;

  final_pos *= character.colpkt.e_radius;
  character.pos = final_pos;
}
