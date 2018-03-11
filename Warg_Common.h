#pragma once
#include "Physics.h"
#include "Spell.h"
#include "Warg_Event.h"
#include <map>
#include <string>
#include <vector>

struct CharStats
{
  float32 gcd;
  float32 speed;
  float32 cast_speed;
  float32 cast_damage;
  float32 mana_regen;
  float32 hp_regen;
  float32 damage_mod;
  float32 atk_dmg;
  float32 atk_speed;
};

struct Character_Physics
{
  bool operator==(const Character_Physics &) const;
  bool operator!=(const Character_Physics &) const;

  vec3 pos = vec3(0), dir = vec3(0), vel = vec3(0);
  bool grounded = false;
};

struct Movement_Command
{
  uint32 i;
  Move_Status m;
  vec3 dir;
};

struct Character
{
  void update_hp(float32 dt);
  void update_mana(float32 dt);
  void update_spell_cooldowns(float32 dt);
  void update_global_cooldown(float32 dt);
  void apply_modifiers();
  void move(float32 dt, const std::vector<Triangle> &colliders);

  UID id;

  Node_Ptr mesh;

  Character_Physics physics;
  vec3 radius = vec3(0.5f) * vec3(.39, 0.30, 1.61); // avg human in meters
  Movement_Command last_movement_command;
  Collision_Packet colpkt;
  int collision_recursion_depth;

  std::string name;
  int team;
  UID target = 0;

  CharStats b_stats, e_stats;

  std::map<std::string, Spell> spellbook;

  std::vector<Buff> buffs, debuffs;

  int32 hp, hp_max;
  float32 mana, mana_max;
  bool alive = true;

  float64 atk_cd = 0;
  float64 gcd = 0;

  bool silenced = false;
  bool casting = false;
  Spell *casting_spell;
  float32 cast_progress = 0;
  UID cast_target = 0;

private:
  void apply_modifier(CharMod &modifier);
};

struct Map
{
  Node_Ptr node;
  Mesh_Data mesh;
  Material_Descriptor material;

  vec3 spawn_pos[2];
  vec3 spawn_dir[2];
};

Map make_blades_edge();
std::vector<Triangle> collect_colliders(Scene_Graph &scene);