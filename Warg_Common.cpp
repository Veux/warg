#include "stdafx.h"
#include "Warg_Common.h"

vec3 collide_char_with_world(Collision_Packet &colpkt, int &collision_recursion_depth, const vec3 &pos, const vec3 &vel,
    const std::vector<Triangle> &colliders);

Blades_Edge::Blades_Edge(Flat_Scene_Graph &scene)
{
  spawn_pos[0] = {0, 0, 15};
  spawn_pos[1] = {45, 45, 5};
  spawn_orientation[0] = angleAxis(0.f, vec3(0, 1, 0));
  spawn_orientation[1] = angleAxis(0.f, vec3(0, -1, 0));

  material.albedo.wrap_s = GL_TEXTURE_WRAP_S;
  material.albedo.wrap_t = GL_TEXTURE_WRAP_T;
  material.metalness.mod = vec4(0);
  if (CONFIG.render_simple)
    material.albedo.mod = vec4(0.4f);

  node = scene.add_aiscene("Blades_Edge/bea2.fbx", "Blades Edge");
}

UID add_dummy(Game_State &game_state, Map &map, vec3 position)
{
  UID id = uid();

  Character_Stats stats;
  stats.global_cooldown = 1.5;
  stats.speed = 0.0;
  stats.cast_speed = 1;
  stats.hp_regen = 3;
  stats.mana_regen = 1000;
  stats.damage_mod = 1;
  stats.atk_dmg = 0;
  stats.atk_dmg = 1;

  Character dummy;
  dummy.id = id;
  dummy.base_stats = stats;
  dummy.team = 2;
  dummy.name = "Combat Dummy";
  dummy.physics.position = position;
  dummy.physics.orientation = map.spawn_orientation[0];
  game_state.characters.push_back(dummy);

  Living_Character lc;
  lc.id = id;
  lc.effective_stats = stats;
  lc.hp_max = 100;
  lc.hp = lc.hp_max;
  lc.mana_max = 10000;
  lc.mana = lc.mana_max;
  game_state.living_characters.push_back(lc);

  return id;
}

UID add_char(Game_State &gs, Map &map, int team, std::string_view name)
{
  UID id = uid();

  Character_Stats stats;
  stats.global_cooldown = 1.5;
  stats.speed = MOVE_SPEED;
  stats.cast_speed = 1;
  stats.hp_regen = 2;
  stats.mana_regen = 10;
  stats.damage_mod = 1;
  stats.atk_dmg = 5;
  stats.atk_speed = 2;

  Character c;
  c.id = id;
  c.base_stats = stats;
  c.team = team;
  c.name = name;
  c.physics.position = map.spawn_pos[team];
  c.physics.orientation = map.spawn_orientation[team];
  gs.characters.push_back(c);

  Living_Character lc;
  lc.id = id;
  lc.effective_stats = stats;
  lc.hp_max = 100;
  lc.hp = lc.hp_max;
  lc.mana_max = 500;
  lc.mana = lc.mana_max;
  gs.living_characters.push_back(lc);

  auto add_spell = [&](std::string_view name) {
    gs.character_spells.push_back({id, SPELL_DB.by_name[name]});
  };

  add_spell("Frostbolt");
  add_spell("Blink");
  add_spell("Seed of Corruption");
  add_spell("Icy Veins");
  add_spell("Sprint");
  add_spell("Demonic Circle: Summon");
  add_spell("Demonic Circle: Teleport");
  add_spell("Shadow Word: Pain");

  return id;
}

void move_char(Game_State &gs, Character &character, Input command, Flat_Scene_Graph &scene)
{
  if (command.m & Move_Status::Forwards)
  {
    character.physics.position.y += 10 *dt;
    character.physics.velocity = vec3(10000);//ebinj
  }
  else
  {
    character.physics.velocity = vec3(0);//ebinj
  }
  if (command.m & Move_Status::Backwards)
    character.physics.position.y -= 10 *dt;
  return;

  vec3 &pos = character.physics.position;
  character.physics.orientation = command.orientation;
  vec3 dir = command.orientation * vec3(0, 1, 0);
  vec3 &vel = character.physics.velocity;
  bool &grounded = character.physics.grounded;
  int move_status = command.m;

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
  if (move_status & ~Move_Status::Jumping)
  {
    auto lc = find_if(gs.living_characters, [&](auto &lc) { return lc.id == character.id; });
    if (lc != gs.living_characters.end())
      v = normalize(v) * lc->effective_stats.speed;
  }
  if (move_status & Move_Status::Jumping && grounded)
  {
    vel.z += 4;
    grounded = false;
  }

  vel.z = grounded ? 0 : max(vel.z - 9.81f * dt, -53.f);

  collide_and_slide_char(character.physics, character.radius, v * dt, vec3(0, 0, vel.z) * dt, scene);
}

void check_collision(Collision_Packet &colpkt, Flat_Scene_Graph &scene)
{

  AABB box(colpkt.pos_r3);
  push_aabb(box, colpkt.pos_r3 - (1.51f * colpkt.e_radius) - (1.f * colpkt.vel_r3));
  push_aabb(box, colpkt.pos_r3 + (1.51f * colpkt.e_radius) + (1.f * colpkt.vel_r3));
  uint32 counter = 0;
  std::vector<Triangle_Normal> colliders = scene.collision_octree.test_all(box, &counter);

  // set_message("trianglecounter:", s(counter), 1.0f);

  for (auto &surface : colliders)
  {
    Triangle t;
    t.a = surface.a / colpkt.e_radius;
    t.b = surface.b / colpkt.e_radius; // intentionally reversed, winding order mismatch somewhere, not sure
    t.c = surface.c / colpkt.e_radius;
    check_triangle(&colpkt, t);
  }
}

vec3 collide_char_with_world(Collision_Packet &colpkt, vec3 start_pos, vec3 start_vel, Flat_Scene_Graph &scene)
{
  float epsilon = 0.005f;

  vec3 pos = start_pos;
  vec3 vel = start_vel;

  for (int i = 0; i < 6; i++)
  {
    colpkt.vel = vel;
    colpkt.vel_normalized = normalize(colpkt.vel);
    colpkt.base_point = pos;
    colpkt.found_collision = false;

    check_collision(colpkt, scene);

    if (!colpkt.found_collision)
      return pos + vel;

    vec3 destination_point = pos + vel;
    vec3 new_base_point = pos;

    if (colpkt.nearest_distance >= epsilon)
    {
      vec3 v = vel;
      v = normalize(v) * (colpkt.nearest_distance - epsilon);
      new_base_point = colpkt.base_point + v;

      v = normalize(v);
      colpkt.intersection_point -= epsilon * v;
    }

    vec3 slide_plane_origin = colpkt.intersection_point;
    vec3 slide_plane_normal = new_base_point - colpkt.intersection_point;
    slide_plane_normal = normalize(slide_plane_normal);
    Plane sliding_plane = Plane(slide_plane_origin, slide_plane_normal);

    vec3 new_destination_point =
        destination_point - sliding_plane.signed_distance_to(destination_point) * slide_plane_normal;

    vec3 new_vel_vec = new_destination_point - colpkt.intersection_point;

    if (length(new_vel_vec) < epsilon)
      return new_base_point;

    pos = new_base_point;
    vel = new_vel_vec;
  }

  return pos;
}

void collide_and_slide_char(
    Character_Physics &physics, vec3 &radius, const vec3 &vel, const vec3 &gravity, Flat_Scene_Graph &scene)
{
  Collision_Packet colpkt;

  colpkt.e_radius = radius;
  colpkt.pos_r3 = physics.position;
  colpkt.vel_r3 = vel;

  vec3 e_space_pos = colpkt.pos_r3 / colpkt.e_radius;
  vec3 e_space_vel = colpkt.vel_r3 / colpkt.e_radius;

  vec3 final_pos = collide_char_with_world(colpkt, e_space_pos, e_space_vel, scene);

  if (physics.grounded)
  {
    colpkt.pos_r3 = final_pos * colpkt.e_radius;
    colpkt.vel_r3 = vec3(0, 0, -0.5);
    colpkt.vel = vec3(0, 0, -0.5) / colpkt.e_radius;
    colpkt.vel_normalized = normalize(colpkt.vel);
    colpkt.base_point = final_pos;
    colpkt.found_collision = false;

    check_collision(colpkt, scene);
    if (colpkt.found_collision && colpkt.nearest_distance > 0.05f)
      final_pos.z -= colpkt.nearest_distance - 0.005f;
  }

  colpkt.pos_r3 = final_pos * colpkt.e_radius;
  colpkt.vel_r3 = gravity;

  e_space_vel = gravity / colpkt.e_radius;

  final_pos = collide_char_with_world(colpkt, final_pos, e_space_vel, scene);

  colpkt.pos_r3 = final_pos * colpkt.e_radius;
  colpkt.vel_r3 = vec3(0, 0, -0.05);
  colpkt.vel = vec3(0, 0, -0.05) / colpkt.e_radius;
  colpkt.vel_normalized = normalize(colpkt.vel);
  colpkt.base_point = final_pos;
  colpkt.found_collision = false;

  check_collision(colpkt, scene);

  physics.grounded = colpkt.found_collision && gravity.z <= 0;

  final_pos *= colpkt.e_radius;
  physics.position = final_pos;
}

bool Character_Physics::operator==(const Character_Physics &b) const
{
  auto &a = *this;

  bool pos_match = a.position == b.position;
  bool vel_match = a.velocity == b.velocity;
  bool ori_match = a.orientation == b.orientation;
  bool grn_match = a.grounded == b.grounded;

  std::cout << s(
    "\n",

    pos_match,
    "\n",

    "position: ",
    "\ta: ", a.position,
    "\tb: ", b.position,
    "\n"
  );


  return pos_match && vel_match && ori_match && grn_match;
}

bool Character_Physics::operator!=(const Character_Physics &b) const
{
  return !(*this == b);
}

bool Input::operator==(const Input &b) const
{
  auto &a = *this;
  return a.orientation == b.orientation && a.m == b.m;
}

bool Input::operator!=(const Input &b) const
{
  return !(*this == b);
}

//size_t Input_Buffer::size()
//{
//  if (start <= end)
//    return end - start;
//  else
//    return (capacity - start) + end;
//}
//
//Input &Input_Buffer::operator[](size_t i)
//{
//  ASSERT(i < size());
//
//  if (start + i < capacity)
//    return buffer[start + i];
//  else
//    return buffer[i - (capacity - start)];
//}
//
//Input Input_Buffer::back()
//{
//  ASSERT(size());
//  return buffer[end];
//}
//
//void Input_Buffer::push(Input &input)
//{
//  size_t size_ = size();
//
//  if (size_ < capacity)
//  {
//    if (end < capacity)
//      buffer[end++] = input;
//    else
//    {
//      buffer[0] = input;
//      end = 1;
//    }
//    return;
//  }
//
//  buffer[start] = input;
//  end = start + 1;
//  if (start < capacity - 1)
//    start++;
//  else
//    start = 0;
//}
//
//void Input_Buffer::pop_older_than(uint32 input_number)
//{
//  uint32 delta = input_number - buffer[start].number + 1;
//  size_t size_ = size();
//
//  if (delta > size_)
//    start = end = 0;
//  else if (start + delta < capacity)
//    start += delta;
//  else
//    start = delta - (capacity - start);
//}

void damage_character(Game_State &gs, UID subject_id, UID object_id, float32 damage)
{
  auto tlc = find_by(gs.living_characters, &Living_Character::id, object_id);

  tlc->hp -= damage;

  std::vector<Character_Buff> cbs;
  for (auto &cb : gs.character_buffs)
    if (cb.character == object_id && subject_id)
      cbs.push_back(cb);
  for (auto &cb : cbs)
  {
    if (SPELL_DB.buff_damage_seed_of_corruption.contains(cb.buff_id))
    {
      auto b_id = SPELL_DB.buff_by_name["Seed of Corruption"];
      auto soc = find_if(
          gs.seeds_of_corruption, [&](auto &soc) { return soc.character == object_id && soc.caster == subject_id; });
      if (soc == gs.seeds_of_corruption.end())
        return;

      soc->damage_taken += damage;
      if (soc->damage_taken < 20)
        return;

      erase_if(gs.character_buffs,
          [&](auto &cd) { return cd.character == object_id && cd.buff_id == b_id && cd.caster == soc->caster; });
      gs.seeds_of_corruption.erase(soc);
      end_buff(gs, cb);
    }
  }
}

void remove_dead_characters(Game_State &gs)
{
  auto first_dead =
      std::partition(gs.living_characters.begin(), gs.living_characters.end(), [](auto &lc) { return lc.hp > 0; });
  std::for_each(first_dead, gs.living_characters.end(), [&](auto &lc) {
    erase_if(gs.character_targets, [&](auto &ct) { return ct.character == lc.id || ct.target == lc.id; });
    erase_if(gs.character_casts, [&](auto &cc) { return cc.caster == lc.id; });
    erase_if(gs.spell_cooldowns, [&](auto &sc) { return sc.character == lc.id; });
    erase_if(gs.character_buffs, [&](auto &cb) { return cb.character == lc.id; });
    erase_if(gs.character_silencings, [&](auto &cs) { return cs.character == lc.id; });
    erase_if(gs.character_gcds, [&](auto &cg) { return cg.character == lc.id; });
    erase_if(gs.demonic_circles, [&](auto &dc) { return dc.owner == lc.id; });
    erase_if(gs.seeds_of_corruption, [&](auto &soc) { return soc.character == lc.id; });
  });
  gs.living_characters.erase(first_dead, gs.living_characters.end());
}

void update_spell_objects(Game_State &gs, Flat_Scene_Graph &scene)
{
  for (auto &so : gs.spell_objects)
  {
    auto t = find_by(gs.characters, &Character::id, so.target);
    auto speed = SPELL_DB.sobj_speed[so.spell_id];
    so.pos += normalize(t->physics.position - so.pos) * (speed *dt);
  }

  auto first_hit = std::partition(gs.spell_objects.begin(), gs.spell_objects.end(), [&](auto &so) {
    auto t = find_by(gs.characters, &Character::id, so.target);
    auto speed = SPELL_DB.sobj_speed[so.spell_id];
    float d = length(t->physics.position - so.pos);
    float epsilon = speed * dt / 2.f * 1.05f;
    return d > epsilon;
  });
  auto hits = std::vector(first_hit, gs.spell_objects.end());
  gs.spell_objects.erase(first_hit, gs.spell_objects.end());

  for (auto &so : hits)
  {
    if (none_of(gs.living_characters, [&](auto &lc) { return lc.id == so.target; }))
      continue;

    auto hit_damage = SPELL_DB.sobj_hit_damage.find(so.spell_id);
    if (hit_damage != SPELL_DB.sobj_hit_damage.end())
      damage_character(gs, so.caster, so.target, hit_damage->second);

    auto buff_application = SPELL_DB.sobj_hit_buff_application.find(so.spell_id);
    if (buff_application != SPELL_DB.sobj_hit_buff_application.end())
    {
      erase_if(gs.character_buffs,
        [&](auto &cd) { return cd.character == so.target && cd.buff_id == buff_application->second; });
      gs.character_buffs.push_back({
        .character = so.target,
        .buff_id = buff_application->second,
        .caster = so.caster,
        .duration = SPELL_DB.buff_duration[buff_application->second],
        .time_since_last_tick = 0
      });
    }

    if (SPELL_DB.sobj_hit_seed_of_corruption.contains(so.spell_id))
    {
      auto b_id = SPELL_DB.buff_by_name["Seed of Corruption"];
      auto existing =
          find_if(gs.character_buffs, [&](auto &cd) { return cd.character == so.target && cd.buff_id == b_id; });

      if (existing != gs.character_buffs.end())
      {
        erase_if(gs.character_buffs, [&](auto &cd) { return cd.character == so.target && cd.buff_id == b_id; });
        break;
      }

      gs.character_buffs.push_back({
        .character = so.target,
        .buff_id = b_id,
        .caster = so.caster,
        .duration = SPELL_DB.buff_duration[b_id],
        .time_since_last_tick = 0
      });

      gs.seeds_of_corruption.push_back({so.target, so.caster, 0.0});
    }
  }
}

void end_buff(Game_State &gs, Character_Buff &b) 
{
  if (SPELL_DB.buff_end_demonic_circle_destroy.contains(b.buff_id))
    erase_if(gs.demonic_circles, [&](auto &ds) { return ds.owner == b.character; });

  if (SPELL_DB.buff_end_seed_of_corruption.contains(b.buff_id))
  {
    std::queue<Character_Buff> seeds_to_detonate;
    seeds_to_detonate.push(b);
    while (!seeds_to_detonate.empty())
    {
      auto soc = seeds_to_detonate.front();
      
      auto c = find_by(gs.characters, &Character::id, soc.character);
      auto pos = c->physics.position;

      for (auto &c1 : gs.characters)
      {
        if (c1.team != c->team || length(c1.physics.position - pos) > 10.f)
          continue;
        if (none_of(gs.living_characters, [&](auto &lc) { return lc.id == c1.id; }))
          continue;
        UID soc_id = SPELL_DB.buff_by_name["Seed of Corruption"];
        auto first_soc = std::partition(gs.character_buffs.begin(), gs.character_buffs.end(),
            [&](auto &cd) { return cd.character != c1.id || cd.buff_id != soc_id; });
        std::for_each(first_soc, gs.character_buffs.end(), [&](auto &soc1) { seeds_to_detonate.push(soc1); });
        gs.character_buffs.erase(first_soc, gs.character_buffs.end());
        erase_if(gs.seeds_of_corruption, [&](auto &soc1) { return soc1.character == c1.id; });
        damage_character(gs, c->id, c1.id, 10);
        UID corruption_id = SPELL_DB.buff_by_name["Corruption"];
        float32 corruption_duration = SPELL_DB.buff_duration[corruption_id];
        gs.character_buffs.push_back({
          .character = c1.id,
          .buff_id = corruption_id,
          .caster = c->id,
          .duration = corruption_duration,
          .time_since_last_tick = 0
        });
      }

      seeds_to_detonate.pop();
    }
  }
}

void update_buffs(std::vector<Character_Buff> &bs, Game_State &gs)
{
  std::vector<Character_Buff> buffs_to_tick;
  for (auto &b : bs)
  {
    b.duration -= dt;
    b.time_since_last_tick += dt;
    auto lc = find_by(gs.living_characters, &Living_Character::id, b.character);

    auto stats_modifiers = SPELL_DB.buff_stats_modifiers.find(b.buff_id);
    if (stats_modifiers != SPELL_DB.buff_stats_modifiers.end())
    {
      lc->effective_stats *= stats_modifiers->second;
    }

    auto tick_freq = SPELL_DB.buff_tick_freq.find(b.buff_id);
    if (tick_freq != SPELL_DB.buff_tick_freq.end() && b.time_since_last_tick > 1.f / tick_freq->second)
    {
      buffs_to_tick.push_back(b);
      b.time_since_last_tick = 0;
    }
  }
  for (auto &b : buffs_to_tick)
  {
    UID b_id = b.buff_id;
    auto tick_damage = SPELL_DB.buff_tick_damage.find(b_id);
    if (tick_damage != SPELL_DB.buff_tick_damage.end())
      damage_character(gs, b.caster, b.character, tick_damage->second);
  }
  auto first_ended = std::partition(bs.begin(), bs.end(), [&](auto &b) { return b.duration > 0; });
  auto ended_buffs = std::vector(first_ended, bs.end());
  bs.erase(first_ended, bs.end());
  for (auto &b : ended_buffs)
    end_buff(gs, b);
}

void cast_spell(Game_State &gs, Flat_Scene_Graph &scene, UID caster_id, UID target_id, UID spell_id)
{
  if (none_of(gs.character_spells, [&](auto &cs) { return cs.character == caster_id && cs.spell == spell_id; }))
    return;

  auto caster_lc = find_by(gs.living_characters, &Living_Character::id, caster_id);

  if (SPELL_DB.targets[spell_id] == Spell_Targets::Self)
    target_id = caster_id;

  bool target_dead = none_of(gs.living_characters, [&](auto &lc) { return lc.id == target_id; });
  if (caster_lc == gs.living_characters.end() || target_dead)
    return;

  if (any_of(gs.character_silencings, [caster_id](auto &cs) { return cs.character == caster_id; }))
    return;

  if (SPELL_DB.on_gcd.contains(spell_id) &&
      any_of(gs.character_gcds, [caster_id](auto &cg) { return cg.character == caster_id; }))
    return;

  if (any_of(gs.spell_cooldowns, [&](auto &c) { return c.character == caster_id && c.spell == spell_id; }))
    return;

  auto mana_cost = SPELL_DB.mana_cost.find(spell_id);
  if (mana_cost != SPELL_DB.mana_cost.end() && mana_cost->second > caster_lc->mana)
    return;

  if (caster_id != target_id)
  {
    auto caster = find_if(gs.characters, [&](auto &c) { return c.id == caster_id; });
    auto target = find_if(gs.characters, [&](auto &c) { return c.id == target_id; });
    auto range = SPELL_DB.range.find(spell_id);
    if (range == SPELL_DB.range.end() || length(caster->physics.position - target->physics.position) > range->second)
      return;
  }

  if (any_of(gs.character_casts, [&](auto &c) { return c.caster == caster_id; }))
    return;

  if (SPELL_DB.cast_time.contains(spell_id))
  {
    gs.character_casts.push_back({caster_id, target_id, spell_id, 0});
  }
  else
  {
    auto object_creation = SPELL_DB.spell_release_object_creation.find(spell_id);
    if (object_creation != SPELL_DB.spell_release_object_creation.end())
    {
      auto c = find_if(gs.characters, [&](auto &c) { return c.id == caster_id; });
      Spell_Object object;
      object.spell_id = object_creation->second;
      object.caster = caster_id;
      object.target = target_id;
      object.pos = c->physics.position;
      object.id = uid();
      gs.spell_objects.push_back(object);
    }

    const auto &buff_applications = SPELL_DB.spell_release_buff_application.find(spell_id);
    if (buff_applications != SPELL_DB.spell_release_buff_application.end())
    {
      for (auto b_id : buff_applications->second)
      {
        bool is_debuff = SPELL_DB.buff_is_debuff.contains(b_id);
        erase_if(gs.character_buffs, [&](auto &cb) { return cb.character == target_id && cb.buff_id == b_id; });
        gs.character_buffs.push_back({.character = target_id,
            .buff_id = b_id,
            .caster = caster_id,
            .duration = SPELL_DB.buff_duration[b_id],
            .time_since_last_tick = 0});
      }
    }

    if (SPELL_DB.spell_release_blink.contains(spell_id))
    {
      auto caster = find_if(gs.characters, [&](auto &c) { return c.id == caster_id; });
      vec3 dir = caster->physics.orientation * vec3(0, 1, 0);
      vec3 delta = normalize(dir) * vec3(15.f);
      collide_and_slide_char(caster->physics, caster->radius, delta, vec3(0, 0, -100), scene);
    }

    if (SPELL_DB.spell_release_demonic_circle_summon.contains(spell_id))
    {
      auto b_id = SPELL_DB.buff_by_name["Demonic Circle"];
      erase_if(gs.character_buffs, [&](auto &cb) { return cb.character == caster_id && cb.buff_id == b_id; });
      erase_if(gs.demonic_circles, [&](auto &ds) { return ds.owner == caster_id; });
      gs.character_buffs.push_back({.character = caster_id,
          .buff_id = b_id,
          .caster = caster_id,
          .duration = SPELL_DB.buff_duration[b_id]});

      auto c = find_if(gs.characters, [&](auto &c) { return c.id == caster_id; });
      gs.demonic_circles.push_back({caster_id, c->physics.position});
    }

    if (SPELL_DB.spell_release_demonic_circle_teleport.contains(spell_id))
    {
      auto ds = find_if(gs.demonic_circles, [&](auto &ds) { return ds.owner == caster_id; });
      if (ds == gs.demonic_circles.end())
        return;
      auto c = find_if(gs.characters, [&](auto &c) { return c.id == caster_id; });
      c->physics.position = ds->position;
      c->physics.grounded = false;
    }

    auto cd = SPELL_DB.cooldown.find(spell_id);
    if (cd != SPELL_DB.cooldown.end())
      gs.spell_cooldowns.push_back({caster_id, spell_id, cd->second});

    auto mana_cost = SPELL_DB.mana_cost.find(spell_id);
    if (mana_cost != SPELL_DB.mana_cost.end())
      caster_lc->mana = max(0.f, caster_lc->mana - mana_cost->second);
  }
  if (SPELL_DB.on_gcd.contains(spell_id))
    gs.character_gcds.push_back({caster_id, caster_lc->effective_stats.global_cooldown});
}

// clang-format off

int cmp(Character &c, Living_Character &lc) { return (lc.id < c.id) - (c.id < lc.id); }
int cmp(Living_Character &lc, Character &c) { return -cmp(c, lc); }
int cmp(Living_Character &lc, Character_GCD &cg) { return (cg.character < lc.id) - (lc.id < cg.character); }
int cmp(Character_GCD &cg, Living_Character &lc) { return -cmp(lc, cg); }
int cmp(Living_Character &lc, Character_Cast &cc) { return (cc.caster < lc.id) - (lc.id < cc.caster); }
int cmp(Character_Cast &cc, Living_Character &lc) { return -cmp(lc, cc); }

// clang-format on

void update_casts(Game_State &gs, Flat_Scene_Graph &scene)
{
  join_for(gs.character_casts, gs.living_characters,
      [](auto &cc, auto &lc) { cc.progress += lc.effective_stats.cast_speed * dt; });

  auto first_completed = std::partition(gs.character_casts.begin(), gs.character_casts.end(), [&](auto &cc) {
    return cc.progress < SPELL_DB.cast_time[cc.spell];
  });
  auto completed = std::vector(first_completed, gs.character_casts.end());
  gs.character_casts.erase(first_completed, gs.character_casts.end());

  for (auto &cc : completed)
  {
    auto caster_lc = find_if(gs.living_characters, [&](auto &lc) { return lc.id == cc.caster; });

    if (none_of(gs.living_characters, [&](auto &lc) { return lc.id == cc.target; }))
      continue;

    auto mana_cost = SPELL_DB.mana_cost.find(cc.spell);
    if (mana_cost != SPELL_DB.mana_cost.end() && mana_cost->second > caster_lc->mana)
      continue;

    if (cc.caster != cc.target)
    {
      auto caster = find_if(gs.characters, [&](auto &c) { return c.id == cc.caster; });
      auto target = find_if(gs.characters, [&](auto &c) { return c.id == cc.target; });
      auto range = SPELL_DB.range.find(cc.spell);
      if (range == SPELL_DB.range.end() ||
          length(caster->physics.position - target->physics.position) > range->second)
        continue;
    }

    auto object_creation = SPELL_DB.spell_release_object_creation.find(cc.spell);
    if (object_creation != SPELL_DB.spell_release_object_creation.end())
    {
      auto c = find_if(gs.characters, [&](auto &c) { return c.id == cc.caster; });
      Spell_Object object;
      object.spell_id = object_creation->second;
      object.caster = cc.caster;
      object.target = cc.target;
      object.pos = c->physics.position;
      object.id = uid();
      gs.spell_objects.push_back(object);
    }

    auto cd = SPELL_DB.cooldown.find(cc.spell);
    if (cd != SPELL_DB.cooldown.end())
      gs.spell_cooldowns.push_back({cc.caster, cc.spell, cd->second});

    caster_lc->mana = std::max(0.f, caster_lc->mana - mana_cost->second);
  }
}

void update_hp(std::vector<Living_Character> &lcs)
{
  for (auto &lc : lcs)
    lc.hp = min(lc.hp + lc.effective_stats.hp_regen * dt, lc.hp_max);
}

void update_mana(std::vector<Living_Character> &lcs)
{
  for (auto &lc : lcs)
    lc.mana = min(lc.hp + lc.effective_stats.mana_regen * dt, lc.mana_max);
}

void update_gcds(std::vector<Living_Character> &lcs, std::vector<Character_GCD> &cgs)
{
  join_for(cgs, lcs, [](auto &cg, auto &lc) { cg.remaining -= dt * lc.effective_stats.cast_speed; });
  erase_if(cgs, [](auto &cg) { return cg.remaining <= 0.f; });
}

void update_spell_cooldowns(std::vector<Spell_Cooldown> &scs)
{
  for (auto &scd : scs)
    scd.cooldown_remaining -= dt;
  erase_if(scs, [](auto &scd) { return scd.cooldown_remaining <= 0.0; });
}

void reset_stats(std::vector<Character> &cs, std::vector<Living_Character> &lcs)
{
  join_for(cs, lcs, [](auto &c, auto &lc) { lc.effective_stats = c.base_stats; });
}

void respawn_dummy(std::vector<Character> &cs, std::vector<Living_Character> &lcs, Game_State &gs, Map &map)
{
  if (join_none_of(cs, lcs, [](auto &c, auto &lc) { return c.name == "Combat Dummy"; }))
    add_dummy(gs, map, {1, 1, 5});
}

void move_character(Game_State &gs, Map &map, Flat_Scene_Graph &scene, std::map<UID, Input> &inputs, Character &c)
{
  Input last_input;
  if (any_of(gs.living_characters, [&](auto &lc) { return lc.id == c.id; }))
  {
    auto last_input_it = inputs.find(c.id);
    if (last_input_it != inputs.end())
      last_input = last_input_it->second;
  }

  vec3 old_pos = c.physics.position;
  move_char(gs, c, last_input, scene);
  if (_isnan(c.physics.position.x) || _isnan(c.physics.position.y) || _isnan(c.physics.position.z))
    c.physics.position = map.spawn_pos[c.team];
  if (c.physics.position.z < -50)
    c.physics.position = map.spawn_pos[c.team];
  if (any_of(gs.living_characters, [&](auto &lc) { return lc.id == c.id; }))
  {
    if (c.physics.position != old_pos)
    {
      erase_if(gs.character_casts, [&](auto &cc) { return cc.caster == c.id; });
      erase_if(gs.character_gcds, [&](auto &cg) { return cg.character == c.id; });
    }
  }
  else
  {
    c.physics.orientation = angleAxis(-half_pi<float32>(), vec3(1.f, 0.f, 0.f));
  }
}

void move_characters(Game_State &gs, Map &map, Flat_Scene_Graph &scene, std::map<UID, Input> &inputs)
{
  for (auto &c : gs.characters)
    move_character(gs, map, scene, inputs, c);
}

void update_game(Game_State &gs, Map &map, Flat_Scene_Graph &scene, std::map<UID, Input> &inputs)
{
  reset_stats(gs.characters, gs.living_characters);
  update_buffs(gs.character_buffs, gs);
  move_characters(gs, map, scene, inputs);
  update_hp(gs.living_characters);
  update_mana(gs.living_characters);
  update_gcds(gs.living_characters, gs.character_gcds);
  update_spell_objects(gs, scene);
  update_spell_cooldowns(gs.spell_cooldowns);
  update_casts(gs, scene);
  remove_dead_characters(gs);
  respawn_dummy(gs.characters, gs.living_characters, gs, map);
  gs.tick++;
}

Game_State predict(Game_State gs, Map &map, Flat_Scene_Graph &scene, UID character_id, Input input)
{
  std::map<UID, Input> inputs;
  inputs[character_id] = input;

  auto c_it = find_by(gs.characters, &Character::id, character_id);
  if (c_it != gs.characters.end())
    move_character(gs, map, scene, inputs, *c_it);

  gs.tick++;

  return gs;
}

bool verify_prediction(Game_State &a, Game_State &b, UID character_id)
{
  ASSERT(a.tick == b.tick);

  std::cout << s("verifying prediction for tick=", a.tick, "\n");

  auto ac = find_if(a.characters, [&](auto &c) { return c.id == character_id; });
  auto bc = find_if(b.characters, [&](auto &c) { return c.id == character_id; });

  if (ac == a.characters.end()) return bc == b.characters.end();
  if (bc == b.characters.end()) return ac == a.characters.end();

  return ac->physics == bc->physics;
}

void merge_prediction(Game_State &dst, const Game_State &pred, UID character_id)
{
  auto dc = find_if(dst.characters, [&](auto &c) { return c.id == character_id; });
  auto pc = find_if(pred.characters, [&](auto &c) { return c.id == character_id; });

  if (dc != dst.characters.end() && pc != pred.characters.end())
    dc->physics = pc->physics;
}