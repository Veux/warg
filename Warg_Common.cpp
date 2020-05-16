#include "stdafx.h"
#include "Warg_Common.h"

vec3 collide_char_with_world(Collision_Packet &colpkt, int &collision_recursion_depth, const vec3 &pos, const vec3 &vel,
    const std::vector<Triangle> &colliders);

Blades_Edge::Blades_Edge(Flat_Scene_Graph &scene)
{
  spawn_pos[0] = { 0, 0, 15 };
  spawn_pos[1] = { 45, 45, 5 };
  spawn_orientation[0] = angleAxis(0.f, vec3(0, 1, 0));
  spawn_orientation[1] = angleAxis(0.f, vec3(0, -1, 0));

  material.albedo.wrap_s = GL_TEXTURE_WRAP_S;
  material.albedo.wrap_t = GL_TEXTURE_WRAP_T;
  material.metalness.mod = vec4(0);
  if (CONFIG.render_simple)
    material.albedo.mod = vec4(0.4f);

  node = scene.add_aiscene("Blades_Edge/bea2.fbx","Blades Edge");

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

void Character::update_spell_cooldowns(float32 dt)
{
  for (size_t i = 0; i < spell_set.spell_count; i++)
  {
    Spell_Status *spell_status = &spell_set.spell_statuses[i];
    if (spell_status->cooldown_remaining > 0)
      spell_status->cooldown_remaining -= dt;
    if (spell_status->cooldown_remaining < 0)
      spell_status->cooldown_remaining = 0;
  }
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

Buff * Character::find_debuff(Spell_ID debuff_id)
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

void move_char(Character &character, Input command, Flat_Scene_Graph* scene)
{
  //return;
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

  //vel.z = 0;
  //pos.z = 0;
 

  collide_and_slide_char(character.physics, character.radius, v * dt, vec3(0, 0, vel.z) * dt, scene);
}

//std::vector<Triangle> collect_colliders(Flat_Scene_Graph &scene)
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



void check_collision(Collision_Packet &colpkt, Flat_Scene_Graph* scene)
{

  AABB box(colpkt.pos_r3);
  push_aabb(box, colpkt.pos_r3 - (1.51f*colpkt.e_radius)-(1.f*colpkt.vel_r3));
  push_aabb(box, colpkt.pos_r3 + (1.51f*colpkt.e_radius)+(1.f*colpkt.vel_r3));
  uint32 counter = 0;
  std::vector<Triangle_Normal> colliders = scene->collision_octree.test_all(box, &counter);
  
  //set_message("trianglecounter:", s(counter), 1.0f);


  for (auto &surface : colliders)
  {
    
    Triangle t;
    t.a = surface.a / colpkt.e_radius;
    t.b = surface.b / colpkt.e_radius;//intentionally reversed, winding order mismatch somewhere, not sure
    t.c = surface.c / colpkt.e_radius;
    check_triangle(&colpkt, t);
  }
}

vec3 collide_char_with_world(Collision_Packet &colpkt, int &collision_recursion_depth, const vec3 &pos, const vec3 &vel,
    Flat_Scene_Graph* scene)
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

void collide_and_slide_char(Character_Physics &physics, vec3 &radius, const vec3 &vel, const vec3 &gravity,
    Flat_Scene_Graph* scene)
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

bool Character_Physics::operator!=(const Character_Physics &b) const { return !(*this == b); }

bool Character_Physics::operator<(const Character_Physics &b) const { return command_number < b.command_number; }

bool Input::operator==(const Input &b) const
{
  auto &a = *this;
  return a.number == b.number && a.orientation == b.orientation && a.m == b.m;
}

bool Input::operator!=(const Input &b) const { return !(*this == b); }

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

void character_copy(Character *dst, Character *src)
{
  *dst = *src;
  strncpy(dst->name, src->name, MAX_CHARACTER_NAME_LENGTH);
}

void game_state_copy(Game_State *dst, Game_State *src)
{
  *dst = *src;
  for (size_t i = 0; i < src->character_count; i++)
    character_copy(dst->characters + i, src->characters + i);
  for (size_t i = 0; i < src->spell_object_count; i++)
    dst->spell_objects[i] = src->spell_objects[i];
  
}

Spell_Index get_casting_spell_formula_index(Character *character)
{
  return character->spell_set.spell_statuses[character->casting_spell_status_index].formula_index;
}

Character *Game_State::get_character(UID id)
{
  for (size_t i = 0; i < character_count; i++)
  {
    Character *character = &characters[i];
    if (character->id == id)
      return character;
  }
  return nullptr;
}

void Game_State::damage_character(Character *subject, Character *object, float32 damage)
{
  ASSERT(characters <= object && object <= &characters[character_count - 1]);

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
