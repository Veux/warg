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

UID add_dummy(Game_State &game_state, Map *map, vec3 position)
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
  strcpy(dummy.name, "Combat Dummy");
  dummy.physics.position = position;
  dummy.physics.orientation = map->spawn_orientation[0];
  game_state.characters.push_back(dummy);

  Living_Character lc;
  lc.id = id;
  lc.effective_stats = stats;
  lc.hp_max = 15;
  lc.hp = lc.hp_max;
  lc.mana_max = 10000;
  lc.mana = lc.mana_max;
  game_state.living_characters.push_back(lc);

  return id;
}

UID add_char(Game_State &game_state, Map *map, Spell_Database &spell_db, int team, const char *name)
{
  ASSERT(0 <= team && team < 2);
  ASSERT(name);

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

  Character character;
  character.id = id;
  character.base_stats = stats;
  character.team = team;
  strncpy(character.name, name, MAX_CHARACTER_NAME_LENGTH);
  character.physics.position = map->spawn_pos[team];
  character.physics.orientation = map->spawn_orientation[team];
  game_state.characters.push_back(character);

  Living_Character lc;
  lc.id = id;
  lc.effective_stats = stats;
  lc.hp_max = 100;
  lc.hp = lc.hp_max;
  lc.mana_max = 500;
  lc.mana = lc.mana_max;
  game_state.living_characters.push_back(lc);

  auto add_spell = [&](const char *name) {
    game_state.character_spells.push_back({id, spell_db.get_spell(name)->index});
  };

  add_spell("Frostbolt");
  add_spell("Blink");
  add_spell("Seed of Corruption");
  add_spell("Icy Veins");
  add_spell("Sprint");
  add_spell("Demonic Circle: Summon");
  add_spell("Demonic Circle: Teleport");

  return id;
}

void move_char(Game_State &gs, Character &character, Input command, Flat_Scene_Graph &scene)
{
  // return;
  vec3 &pos = character.physics.position;
  character.physics.orientation = command.orientation;
  vec3 dir = command.orientation * vec3(0, 1, 0);
  vec3 &vel = character.physics.velocity;
  bool &grounded = character.physics.grounded;
  Move_Status move_status = command.m;

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
    auto &lc = std::find_if(
        gs.living_characters.begin(), gs.living_characters.end(), [&](auto &lc) { return lc.id == character.id; });
    v = normalize(v) * lc->effective_stats.speed;
    // v *= character.effective_stats.speed;
  }
  if (move_status & Move_Status::Jumping && grounded)
  {
    vel.z += 4;
    grounded = false;
  }

  vel.z -= 9.81f * dt;
  if (vel.z < -53)
    vel.z = -53;
  if (grounded)
    vel.z = 0;

  // vel.z = 0;
  // pos.z = 0;

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

vec3 collide_char_with_world(
    Collision_Packet &colpkt, int &collision_recursion_depth, const vec3 &pos, const vec3 &vel, Flat_Scene_Graph &scene)
{
  float epsilon = 0.005f;

  if (collision_recursion_depth > 5)
    return pos;

  colpkt.vel = vel;
  colpkt.vel_normalized = normalize(colpkt.vel);
  colpkt.base_point = pos;
  colpkt.found_collision = false;

  check_collision(colpkt, scene);

  if (!colpkt.found_collision)
  {
    return pos + vel;
  }

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
  {
    return new_base_point;
  }

  collision_recursion_depth++;
  return collide_char_with_world(colpkt, collision_recursion_depth, new_base_point, new_vel_vec, scene);
}

void collide_and_slide_char(
    Character_Physics &physics, vec3 &radius, const vec3 &vel, const vec3 &gravity, Flat_Scene_Graph &scene)
{

  Collision_Packet colpkt;
  int collision_recursion_depth;

  colpkt.e_radius = radius;
  colpkt.pos_r3 = physics.position;
  colpkt.vel_r3 = vel;

  vec3 e_space_pos = colpkt.pos_r3 / colpkt.e_radius;
  vec3 e_space_vel = colpkt.vel_r3 / colpkt.e_radius;

  collision_recursion_depth = 0;

  vec3 final_pos = collide_char_with_world(colpkt, collision_recursion_depth, e_space_pos, e_space_vel, scene);

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
  collision_recursion_depth = 0;

  final_pos = collide_char_with_world(colpkt, collision_recursion_depth, final_pos, e_space_vel, scene);

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
  return a.position == b.position && a.orientation == b.orientation && a.velocity == b.velocity &&
         a.grounded == b.grounded;
}

bool Character_Physics::operator!=(const Character_Physics &b) const
{
  return !(*this == b);
}

bool Character_Physics::operator<(const Character_Physics &b) const
{
  return command_number < b.command_number;
}

bool Input::operator==(const Input &b) const
{
  auto &a = *this;
  return a.number == b.number && a.orientation == b.orientation && a.m == b.m;
}

bool Input::operator!=(const Input &b) const
{
  return !(*this == b);
}

size_t Input_Buffer::size()
{
  if (start <= end)
    return end - start;
  else
    return (capacity - start) + end;
}

Input &Input_Buffer::operator[](size_t i)
{
  ASSERT(i < size());

  if (start + i < capacity)
    return buffer[start + i];
  else
    return buffer[i - (capacity - start)];
}

Input Input_Buffer::back()
{
  ASSERT(size());
  return buffer[end];
}

void Input_Buffer::push(Input &input)
{
  size_t size_ = size();

  if (size_ < capacity)
  {
    if (end < capacity)
      buffer[end++] = input;
    else
    {
      buffer[0] = input;
      end = 1;
    }
    return;
  }

  buffer[start] = input;
  end = start + 1;
  if (start < capacity - 1)
    start++;
  else
    start = 0;
}

void Input_Buffer::pop_older_than(uint32 input_number)
{
  uint32 delta = input_number - buffer[start].number + 1;
  size_t size_ = size();

  if (delta > size_)
    start = end = 0;
  else if (start + delta < capacity)
    start += delta;
  else
    start = delta - (capacity - start);
}

Character *Game_State::get_character(UID id)
{
  auto character = std::find_if(characters.begin(), characters.end(), [id](auto &c) { return c.id == id; });
  return character == characters.end() ? nullptr : &*character;
}

bool damage_character(Game_State &gs, UID subject_id, UID object_id, float32 damage)
{
  auto tlc = std::find_if(
      gs.living_characters.begin(), gs.living_characters.end(), [&](auto &lc) { return lc.id == object_id; });

  tlc->hp -= damage;

  for (auto &cd : gs.character_debuffs)
    if (cd.character == object_id && subject_id)
      buff_on_damage_dispatch(cd.buff._id, subject_id, object_id, cd.buff, damage, gs);

  if (tlc->hp <= 0)
  {
    erase_if(gs.character_casts, [&](auto &cc) { return cc.caster == tlc->id; });
    erase_if(gs.spell_cooldowns, [&](auto &sc) { return sc.character == tlc->id; });
    erase_if(gs.character_buffs, [&](auto &cb) { return cb.character == tlc->id; });
    erase_if(gs.character_debuffs, [&](auto &cd) { return cd.character == tlc->id; });
    erase_if(gs.character_silencings, [&](auto &cs) { return cs.character == tlc->id; });
    erase_if(gs.character_gcds, [&](auto &cg) { return cg.character == tlc->id; });
    gs.living_characters.erase(tlc);
    return true;
  }
  return false;
}

void update_spell_objects(Game_State &gs, Spell_Database &sdb, Flat_Scene_Graph &scene)
{
  for (auto &so : gs.spell_objects)
  {
    Spell_Object_Formula *sof = sdb.get_spell_object(so.formula_index);
    auto t = std::find_if(gs.characters.begin(), gs.characters.end(), [&](auto &c) { return c.id == so.target; });
    vec3 a = normalize(t->physics.position - so.pos);
    a *= vec3(sof->speed * dt);
    so.pos += a;
  }
  
  auto &first_hit = std::partition(gs.spell_objects.begin(), gs.spell_objects.end(), [&](auto &so) {
    Spell_Object_Formula *sof = sdb.get_spell_object(so.formula_index);
    auto t = std::find_if(gs.characters.begin(), gs.characters.end(), [&](auto &c) { return c.id == so.target; });
    float d = length(t->physics.position - so.pos);
    float epsilon = sof->speed * dt / 2.f * 1.05f;
    return d > epsilon;
  });
  std::vector<Spell_Object> hits(first_hit, gs.spell_objects.end());
  gs.spell_objects.erase(first_hit, gs.spell_objects.end());

  for (auto &so : hits)
  {
    Spell_Object_Formula *sof = sdb.get_spell_object(so.formula_index);
    spell_object_on_hit_dispatch(sof->_id, so, gs, scene);
  }
}

void update_buffs(std::vector<Character_Buff> &bs, Game_State& gs, Spell_Database& sdb)
{
  for (auto &b : bs)
  {
    BuffDef *buff_formula = sdb.get_buff(b.buff.formula_index);
    b.buff.duration -= dt;
    b.buff.time_since_last_tick += dt;
    auto &lc = std::find_if(gs.living_characters.begin(), gs.living_characters.end(),
        [&](auto &lc) { return lc.id == b.character; });
    lc->effective_stats *= buff_formula->stats_modifiers;
    if (b.buff.time_since_last_tick > 1.f / buff_formula->tick_freq)
    {
      buff_on_tick_dispatch(buff_formula->_id, b.character, b.buff, gs);
      b.buff.time_since_last_tick = 0;
    }
  }
  auto first_ended = std::partition(bs.begin(), bs.end(), [&](auto &b) { return b.buff.duration > 0; });
  std::vector<Character_Buff> ended_buffs(first_ended, bs.end());
  bs.erase(first_ended, bs.end());
  for (auto &b : ended_buffs)
    buff_on_end_dispatch(b.buff._id, b.character, b.buff, gs);
}

Cast_Error cast_viable(Game_State &game_state, Spell_Database &spell_db, UID caster_id, UID target_id,
    Spell_Index spell_id, bool ignore_gcd, bool ignore_already_casting)
{
  Spell_Formula *spell_formula = spell_db.get_spell(spell_id);
  ASSERT(spell_formula);

  Character *caster = game_state.get_character(caster_id);
  ASSERT(caster);

  Character *target = game_state.get_character(target_id);
  if (spell_formula->targets == Spell_Targets::Self)
    target = caster;

  if (!target)
    return Cast_Error::Invalid_Target;
  auto &casterr = std::find_if(game_state.living_characters.begin(), game_state.living_characters.end(),
      [&](auto &lc) { return lc.id == caster_id; });
  bool target_alive = std::any_of(game_state.living_characters.begin(), game_state.living_characters.end(),
      [&](auto &lc) { return lc.id == target->id; });
  if (casterr == game_state.living_characters.end() || !target_alive)
    return Cast_Error::Invalid_Target;
  if (std::any_of(game_state.character_silencings.begin(), game_state.character_silencings.end(),
          [caster_id](auto &cs) { return cs.character == caster_id; }))
    return Cast_Error::Silenced;
  if (!ignore_gcd && spell_formula->on_global_cooldown &&
      std::any_of(game_state.character_gcds.begin(), game_state.character_gcds.end(),
          [caster_id](auto &cg) { return cg.character == caster_id; }))
    return Cast_Error::Global_Cooldown;
  if (std::any_of(game_state.spell_cooldowns.begin(), game_state.spell_cooldowns.end(),
          [&](auto &c) { return c.character == caster_id && c.spell == spell_id; }))
    return Cast_Error::Cooldown;
  if (spell_formula->mana_cost > casterr->mana)
    return Cast_Error::Insufficient_Mana;
  if (!target && spell_formula->targets != Spell_Targets::Terrain)
    return Cast_Error::Invalid_Target;
  if (target && length(caster->physics.position - target->physics.position) > spell_formula->range &&
      spell_formula->targets != Spell_Targets::Terrain && spell_formula->targets != Spell_Targets::Self)
    return Cast_Error::Out_of_Range;
  if (!ignore_already_casting && std::any_of(game_state.character_casts.begin(), game_state.character_casts.end(),
                                     [&](auto &c) { return c.caster == caster->id; }))
    return Cast_Error::Already_Casting;

  return Cast_Error::Success;
}

void release_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id,
    UID target_id, Spell_Index spell_id)
{
  Spell_Formula *spell_formula = spell_db.get_spell(spell_id);
  ASSERT(spell_formula);

  Cast_Error err;
  if (static_cast<int>(err = cast_viable(game_state, spell_db, caster_id, target_id, spell_id, true, true)))
    return;

  spell_on_release_dispatch(spell_formula->_id, caster_id, target_id, game_state, scene);

  auto &lc = std::find_if(game_state.living_characters.begin(), game_state.living_characters.end(),
      [&](auto &lc) { return lc.id == caster_id; });
  if (!spell_formula->cast_time && spell_formula->on_global_cooldown)
    game_state.character_gcds.push_back({caster_id, lc->effective_stats.global_cooldown});

  if (spell_formula->cooldown > 0.0)
    game_state.spell_cooldowns.push_back({caster_id, spell_id, spell_formula->cooldown});

  lc->mana = std::max(0.f, lc->mana - spell_formula->mana_cost);
}

void begin_cast(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id, UID target_id,
    Spell_Index spell_id)
{
  Spell_Formula *formula = spell_db.get_spell(spell_id);
  ASSERT(formula->cast_time > 0);

  Character_Cast cast;
  cast.caster = caster_id;
  cast.target = target_id;
  cast.spell = spell_id;
  cast.progress = 0;
  game_state.character_casts.push_back(cast);

  auto &lc = std::find_if(game_state.living_characters.begin(), game_state.living_characters.end(),
      [&](auto &lc) { return lc.id == caster_id; });
  if (formula->on_global_cooldown)
    game_state.character_gcds.push_back({caster_id, lc->effective_stats.global_cooldown});
}

void cast_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id, UID target_id,
    Spell_Index spell_id)
{
  Spell_Formula *formula = spell_db.get_spell(spell_id);
  if (formula->cast_time > 0)
    begin_cast(game_state, spell_db, scene, caster_id, target_id, spell_id);
  else
    release_spell(game_state, spell_db, scene, caster_id, target_id, spell_id);
}

void try_cast_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id,
    UID target_id, Spell_Index spell_id)
{
  if (std::none_of(game_state.character_spells.begin(), game_state.character_spells.end(),
          [&](auto &cs) { return cs.character == caster_id && cs.spell == spell_id; }))
    return;

  Spell_Formula *spell = spell_db.get_spell(spell_id);
  ASSERT(spell);

  if (spell->targets == Spell_Targets::Self)
    target_id = caster_id;

  if (cast_viable(game_state, spell_db, caster_id, target_id, spell_id, false, false) == Cast_Error::Success)
    cast_spell(game_state, spell_db, scene, caster_id, target_id, spell_id);
}

void update_casts(std::vector<Character_Cast> &ccs, std::vector<Living_Character> &lcs, Game_State &gs, Spell_Database &sdb,
  Flat_Scene_Graph &scene) {
  for (auto &cc : ccs)
  {
    auto &lc = std::find_if(lcs.begin(), lcs.end(), [&](auto &lc) { return lc.id == cc.caster; });
    cc.progress += lc->effective_stats.cast_speed * dt;
  }

  auto first_completed = std::partition(ccs.begin(), ccs.end(),
    [&](auto &cc) { return cc.progress < sdb.get_spell(cc.spell)->cast_time; });
  auto completed = std::vector(first_completed, ccs.end());
  ccs.erase(first_completed, ccs.end());

  for (auto &cc : completed)
    release_spell(gs, sdb, scene, cc.caster, cc.target, cc.spell);
}

void update_game(Game_State &gs, Map *map, Spell_Database &spell_db, Flat_Scene_Graph &scene)
{
  // if (!std::any_of(gs.living_characters.begin(), gs.living_characters.end(), [&](auto &lc) {
  //      return std::any_of(gs.characters.begin(), gs.characters.end(),
  //          [&](auto &c) { return c.id == lc.id && std::string(c.name) == "Combat Dummy"; });
  //    }))
  //  add_dummy(gs, map, {1, 1, 5});

  for (auto &lc : gs.living_characters)
  {
    auto &c = std::find_if(gs.characters.begin(), gs.characters.end(), [&](auto &c) { return c.id == lc.id; });
    lc.effective_stats = c->base_stats;
  }

  update_buffs(gs.character_buffs, gs, spell_db);
  update_buffs(gs.character_debuffs, gs, spell_db);

  for (auto &lc : gs.living_characters)
    lc.hp = min(lc.hp + lc.effective_stats.hp_regen * dt, lc.hp_max);

  for (auto &lc : gs.living_characters)
    lc.mana = min(lc.hp + lc.effective_stats.mana_regen * dt, lc.mana_max);

  for (auto &cg : gs.character_gcds)
    cg.remaining -= dt;
  erase_if(gs.character_gcds, [](auto &cg) { return cg.remaining <= 0.f; });

  update_spell_objects(gs, spell_db, scene);

  for (auto &scd : gs.spell_cooldowns)
    scd.cooldown_remaining -= dt;
  erase_if(gs.spell_cooldowns, [](auto &scd) { return scd.cooldown_remaining <= 0.0; });

  update_casts(gs.character_casts, gs.living_characters, gs, spell_db, scene);
}