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

struct Character
{
  void update_hp(float32 dt);
  void update_mana(float32 dt);
  void update_spell_cooldowns(float32 dt);
  void update_global_cooldown(float32 dt);
  void apply_modifiers();

  UID id;

  Node_Ptr mesh;

  vec3 pos;
  vec3 dir;
  vec3 vel;
  vec3 radius = vec3(0.5f) * vec3(.39, 0.30, 1.61); // avg human in meters
  bool grounded;
  Move_Status move_status = Move_Status::None;
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

Map make_blades_edge();
