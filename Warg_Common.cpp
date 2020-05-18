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

void Character::update_hp(float32 dt)
{
  hp += (int)round(effective_stats.hp_regen * dt); // todo: fix dis shit
  if (hp > hp_max)
    hp = hp_max;
}

void Character::update_mana(float32 dt)
{
  mana += effective_stats.mana_regen * dt;

  if (mana > mana_max)
    mana = mana_max;
}

void update_spell_cooldowns(std::vector<Spell_Cooldown> &spell_cooldowns, float32 dt)
{
  for (auto &scd : spell_cooldowns)
    scd.cooldown_remaining -= dt;
  auto erase_it = std::remove_if(
      spell_cooldowns.begin(), spell_cooldowns.end(), [](auto &scd) { return scd.cooldown_remaining <= 0.0; });
  spell_cooldowns.erase(erase_it, spell_cooldowns.end());
}

void Character::update_global_cooldown(float32 dt)
{
  global_cooldown -= dt * effective_stats.cast_speed;
  if (global_cooldown < 0)
    global_cooldown = 0;
}

void Character::take_heal(float32 heal)
{
  if (!alive)
    return;

  hp += heal;

  if (hp > hp_max)
  {
    hp = hp_max;
  }
}

void Character::apply_buff(Buff *buff)
{
  if (buff_count >= MAX_BUFFS)
    return;

  buffs[buff_count++] = *buff;
}

void Character::apply_debuff(Buff *debuff)
{
  if (debuff_count >= MAX_DEBUFFS)
    return;

  debuffs[debuff_count++] = *debuff;
}

Buff *Character::find_buff(Spell_ID buff_id)
{
  Buff *buff = nullptr;
  for (size_t i = 0; i < buff_count; i++)
    if (buffs[i]._id == buff_id)
      buff = &buffs[i];

  return buff;
}

Buff *Character::find_debuff(Spell_ID debuff_id)
{
  Buff *debuff = nullptr;
  for (size_t i = 0; i < debuff_count; i++)
    if (debuffs[i]._id == debuff_id)
      debuff = &debuffs[i];

  return debuff;
}

void Character::remove_buff(Spell_ID buff_id)
{
  for (size_t i = 0; i < buff_count; i++)
  {
    Buff *buff = &buffs[i];
    if (buff->_id == buff_id)
    {
      *buff = buffs[buff_count - 1];
      buff_count--;
    }
  }
}

void Character::remove_debuff(Spell_ID debuff_id)
{
  for (size_t i = 0; i < debuff_count; i++)
  {
    Buff *debuff = &debuffs[i];
    if (debuff->_id == debuff_id)
    {
      *debuff = debuffs[debuff_count - 1];
      debuff_count--;
    }
  }
}

void move_char(Character &character, Input command, Flat_Scene_Graph *scene)
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
    v = normalize(v);
    v *= character.effective_stats.speed;
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

// std::vector<Triangle> collect_colliders(Flat_Scene_Graph &scene)
//{
//  std::vector<Triangle> collider_cache;
//
//  std::vector<Render_Entity> entities = scene.visit_nodes_start();
//  for (Render_Entity &entity : entities)
//  {
//    auto transform = [&](vec3 p) {
//      vec4 q = entity.transformation * vec4(p.x, p.y, p.z, 1.0);
//      return vec3(q.x, q.y, q.z);
//    };
//    Mesh_Data &mesh_data = entity.mesh->mesh->descriptor.mesh_data;
//
//    for (size_t i = 0; i < mesh_data.indices.size(); i += 3)
//    {
//      uint32 a, b, c;
//      a = mesh_data.indices[i];
//      b = mesh_data.indices[i + 1];
//      c = mesh_data.indices[i + 2];
//      Triangle t;
//      t.a = transform(mesh_data.positions[a]);
//      t.c = transform(mesh_data.positions[b]);
//      t.b = transform(mesh_data.positions[c]);
//      collider_cache.push_back(t);
//    }
//  }
//
//  return collider_cache;
//}

void check_collision(Collision_Packet &colpkt, Flat_Scene_Graph *scene)
{

  AABB box(colpkt.pos_r3);
  push_aabb(box, colpkt.pos_r3 - (1.51f * colpkt.e_radius) - (1.f * colpkt.vel_r3));
  push_aabb(box, colpkt.pos_r3 + (1.51f * colpkt.e_radius) + (1.f * colpkt.vel_r3));
  uint32 counter = 0;
  std::vector<Triangle_Normal> colliders = scene->collision_octree.test_all(box, &counter);

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
    Collision_Packet &colpkt, int &collision_recursion_depth, const vec3 &pos, const vec3 &vel, Flat_Scene_Graph *scene)
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
    Character_Physics &physics, vec3 &radius, const vec3 &vel, const vec3 &gravity, Flat_Scene_Graph *scene)
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

void Game_State::damage_character(Character *subject, Character *object, float32 damage)
{
  if (!object->alive)
    return;

  object->hp -= damage;

  if (object->hp <= 0)
  {
    object->hp = 0;
    object->alive = false;

    float32 new_height = object->radius.y;
    object->physics.grounded = false;
    object->physics.position.z -= object->radius.z - object->radius.y;

    return;
  }

  for (size_t i = 0; i < object->debuff_count; i++)
  {
    Buff *buff = &object->debuffs[i];
    BuffDef *buff_formula = SPELL_DB.get_buff(buff->formula_index);
    if (subject)
      buff_on_damage_dispatch(buff_formula, buff, this, subject, object, damage);
  }
}

bool update_spell_object(
    Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, Spell_Object &spell_object)
{
  Spell_Object_Formula *object_formula = spell_db.get_spell_object(spell_object.formula_index);

  ASSERT(spell_object.target);
  Character *target = game_state.get_character(spell_object.target);
  ASSERT(target);

  vec3 a = normalize(target->physics.position - spell_object.pos);
  a *= vec3(object_formula->speed * dt);
  spell_object.pos += a;

  float d = length(target->physics.position - spell_object.pos);
  float epsilon = object_formula->speed * dt / 2.f * 1.05f;
  if (d < epsilon)
  {
    spell_object_on_hit_dispatch(object_formula, &spell_object, &game_state, &scene);
    return true;
  }

  return false;
}

void update_spell_objects(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene)
{

  for (size_t i = 0; i < game_state.spell_object_count; i++)
  {
    Spell_Object &spell_object = game_state.spell_objects[i];
    if (update_spell_object(game_state, spell_db, scene, spell_object))
    {
      Spell_Object *last_object = &game_state.spell_objects[game_state.spell_object_count - 1];
      game_state.spell_objects[i] = *last_object;
      game_state.spell_object_count--;
    }
  }
}

void update_buffs(Game_State &game_state, Spell_Database &spell_db, UID character_id)
{
  ASSERT(character_id);
  Character *character = game_state.get_character(character_id);
  ASSERT(character);

  character->effective_stats = character->base_stats;
  character->silenced = false;

  auto update_buffs_ = [&](Buff *buffs, uint8 *buff_count) {
    for (size_t i = 0; i < *buff_count; i++)
    {
      Buff *buff = buffs + i;
      BuffDef *buff_formula = spell_db.get_buff(buff->formula_index);
      buff->duration -= dt;
      buff->time_since_last_tick += dt;
      character->effective_stats *= buff_formula->stats_modifiers;
      if (buff->time_since_last_tick > 1.f / buff_formula->tick_freq)
      {
        buff_on_tick_dispatch(buff_formula, buff, &game_state, character);
        buff->time_since_last_tick = 0;
      }
      if (buff->duration <= 0)
      {
        buff_on_end_dispatch(buff_formula, buff, &game_state, character);
        *buff = buffs[*buff_count - 1];
        (*buff_count)--;
      }
    }
  };

  update_buffs_(character->buffs, &character->buff_count);
  update_buffs_(character->debuffs, &character->debuff_count);
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
  if (!caster->alive || !target->alive)
    return Cast_Error::Invalid_Target;
  if (caster->silenced)
    return Cast_Error::Silenced;
  if (!ignore_gcd && caster->global_cooldown > 0 && spell_formula->on_global_cooldown)
    return Cast_Error::Global_Cooldown;
  if (std::any_of(game_state.spell_cooldowns.begin(), game_state.spell_cooldowns.end(),
          [&](auto &c) { return c.character == caster_id && c.spell == spell_id; }))
    return Cast_Error::Cooldown;
  if (spell_formula->mana_cost > caster->mana)
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

  Character *caster = game_state.get_character(caster_id);
  ASSERT(caster);

  Character *target = game_state.get_character(target_id);

  Cast_Error err;
  if (static_cast<int>(err = cast_viable(game_state, spell_db, caster_id, target_id, spell_id, true, true)))
    return;

  spell_on_release_dispatch(spell_formula, &game_state, caster, &scene);

  if (!spell_formula->cast_time && spell_formula->on_global_cooldown)
    caster->global_cooldown = caster->effective_stats.global_cooldown;

  if (spell_formula->cooldown > 0.0)
    game_state.spell_cooldowns.push_back({caster_id, spell_id, spell_formula->cooldown});
}

void begin_cast(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id, UID target_id,
    Spell_Index spell_id)
{
  Spell_Formula *formula = spell_db.get_spell(spell_id);
  ASSERT(formula->cast_time > 0);
  ASSERT(game_state.get_character(target_id));

  Character *caster = game_state.get_character(caster_id);
  ASSERT(caster);

  Character_Cast cast;
  cast.caster = caster_id;
  cast.target = target_id;
  cast.spell = spell_id;
  cast.progress = 0;
  game_state.character_casts.push_back(cast);

  if (formula->on_global_cooldown)
    caster->global_cooldown = caster->effective_stats.global_cooldown;
}

void cast_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id, UID target_id,
    Spell_Index spell_id)
{
  Character *caster = game_state.get_character(caster_id);

  Spell_Formula *formula = spell_db.get_spell(spell_id);
  if (formula->cast_time > 0)
    begin_cast(game_state, spell_db, scene, caster_id, target_id, spell_id);
  else
    release_spell(game_state, spell_db, scene, caster_id, target_id, spell_id);
}

// void try_cast_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, Character &caster,
//    UID target_id, Spell_Index spell_formula_index)
//{
//  Spell_Formula *spell_formula = spell_db.get_spell(spell_formula_index);
//
//  bool caster_has_spell = false;
//  Spell_Status *spell_status = nullptr;
//  for (size_t i = 0; i < caster.spell_set.spell_count; i++)
//    if (caster.spell_set.spell_statuses[i].formula_index == spell_formula_index)
//    {
//      caster_has_spell = true;
//      spell_status = &caster.spell_set.spell_statuses[i];
//    }
//  if (!caster_has_spell)
//    return;
//
//  Character *target = nullptr;
//  if (spell_formula->targets == Spell_Targets::Self)
//    target = &caster;
//  else if (spell_formula->targets == Spell_Targets::Terrain)
//    ASSERT(target_id == 0);
//  else if (target_id && game_state.get_character(target_id))
//    target = game_state.get_character(target_id);
//
//  Cast_Error err;
//  if (cast_viable(game_state, spell_db, scene, caster.id, target_id, spell_status, false) == Cast_Error::Success)
//    cast_spell(game_state, spell_db, scene, caster.id, target ? target->id : 0, spell_status);
//}

void try_cast_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id,
    UID target_id, Spell_Index spell_id)
{
  if (std::none_of(game_state.character_spells.begin(), game_state.character_spells.end(),
          [&](auto &cs) { return cs.character == caster_id && cs.spell == spell_id; }))
    return;

  Spell_Formula *spell = spell_db.get_spell(spell_id);
  ASSERT(spell);

  Character *caster = game_state.get_character(caster_id);
  ASSERT(caster);

  Character *target = game_state.get_character(target_id);
  if (spell->targets == Spell_Targets::Self)
    target = caster;

  if (cast_viable(game_state, spell_db, caster_id, target_id, spell_id, false, false) == Cast_Error::Success)
    cast_spell(game_state, spell_db, scene, caster->id, target ? target->id : 0, spell_id);
}

// void interrupt_cast(Game_State &game_state, Spell_Database &spell_db, UID character_id)
//{
//  ASSERT(character_id);
//  Character *character = game_state.get_character(character_id);
//  ASSERT(character);
//
//  if (!character->casting)
//    return;
//
//  ASSERT(character->casting_spell_status_index < character->spell_set.spell_count);
//  Spell_Status *casting_spell_status = &character->spell_set.spell_statuses[character->casting_spell_status_index];
//  Spell_Formula *casting_spell_formula = spell_db.get_spell(casting_spell_status->formula_index);
//
//  character->casting = false;
//  character->cast_progress = 0;
//  if (casting_spell_formula->on_global_cooldown)
//    character->global_cooldown = 0;
//}

// void update_cast(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id, float32
// dt)
//{
//  Character *caster = game_state.get_character(caster_id);
//  ASSERT(caster);
//
//  if (!caster->casting)
//    return;
//
//  ASSERT(caster->casting_spell_status_index < caster->spell_set.spell_count);
//  Spell_Status *casting_spell_status = &caster->spell_set.spell_statuses[caster->casting_spell_status_index];
//  ASSERT(casting_spell_status);
//
//  Spell_Formula *formula = spell_db.get_spell(casting_spell_status->formula_index);
//  caster->cast_progress += caster->effective_stats.cast_speed * dt;
//  if (caster->cast_progress >= formula->cast_time)
//  {
//    caster->cast_progress = 0;
//    caster->casting = false;
//    release_spell(game_state, spell_db, scene, caster_id, caster->cast_target, casting_spell_status);
//    caster->cast_target = 0;
//  }
//}

void update_casts(Game_State &game_state, std::vector<Character_Cast> &character_casts, Spell_Database &spell_db,
    Flat_Scene_Graph &scene, float32 dt)
{
  for (auto &cc : character_casts)
    cc.progress += dt;
  auto erase_it = std::remove_if(character_casts.begin(), character_casts.end(), [&](auto &cc) {
    if (cc.progress >= spell_db.get_spell(cc.spell)->cast_time)
    {
      release_spell(game_state, spell_db, scene, cc.caster, cc.target, cc.spell);
      return true;
    }
    return false;
  });
  character_casts.erase(erase_it, character_casts.end());
}

void update_targets(std::vector<Character> &characters, std::vector<Character_Target> &character_targets)
{
  auto erase_it = std::remove_if(character_targets.begin(), character_targets.end(), [&](auto &t) {
    return std::none_of(characters.begin(), characters.end(), [&](auto &c) { return c.id == t.t; });
  });
  character_targets.erase(erase_it, character_targets.end());
}

UID add_dummy(Game_State &game_state, Map *map, vec3 position)
{
  UID id = uid();

  Character dummy;
  dummy.id = id;
  dummy.team = 2;
  strcpy(dummy.name, "Combat Dummy");
  dummy.physics.position = position;
  dummy.physics.orientation = map->spawn_orientation[0];
  dummy.hp_max = 50;
  dummy.hp = dummy.hp_max;
  dummy.mana_max = 10000;
  dummy.mana = dummy.mana_max;

  Character_Stats stats;
  stats.global_cooldown = 1.5;
  stats.speed = 0.0;
  stats.cast_speed = 1;
  stats.hp_regen = 0;
  stats.mana_regen = 1000;
  stats.damage_mod = 1;
  stats.atk_dmg = 0;
  stats.atk_dmg = 1;

  dummy.base_stats = stats;
  dummy.effective_stats = stats;

  game_state.characters.push_back(dummy);

  return id;
}

UID add_char(Game_State &game_state, Map *map, Spell_Database &spell_db, int team, const char *name)
{
  ASSERT(0 <= team && team < 2);
  ASSERT(name);

  UID id = uid();

  Character character;
  character.id = id;
  character.team = team;
  strncpy(character.name, name, MAX_CHARACTER_NAME_LENGTH);
  character.physics.position = map->spawn_pos[team];
  character.physics.orientation = map->spawn_orientation[team];
  character.hp_max = 100;
  character.hp = character.hp_max;
  character.mana_max = 500;
  character.mana = character.mana_max;

  Character_Stats stats;
  stats.global_cooldown = 1.5;
  stats.speed = MOVE_SPEED;
  stats.cast_speed = 1;
  stats.hp_regen = 2;
  stats.mana_regen = 10;
  stats.damage_mod = 1;
  stats.atk_dmg = 5;
  stats.atk_speed = 2;

  character.base_stats = stats;
  character.effective_stats = stats;

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

  game_state.characters.push_back(character);

  return id;
}

void update_game(Game_State &game_state, Map *map, Spell_Database &spell_db, Flat_Scene_Graph &scene, float32 dt)
{
  bool found_living_dummy = false;
  for (auto &character : game_state.characters)
  {
    if (std::string(character.name) == "Combat Dummy" && character.alive)
      found_living_dummy = true;
  }
  if (!found_living_dummy)
  {
    for (int i = 0; i <= 1; i++)
      add_dummy(game_state, map, {1, i, 5});
    set_message("NEEEEEEXTTTT", "", 5);
  }

  update_targets(game_state.characters, game_state.character_targets);
  update_spell_objects(game_state, spell_db, scene);
  update_spell_cooldowns(game_state.spell_cooldowns, dt);
  update_casts(game_state, game_state.character_casts, spell_db, scene, dt);
}