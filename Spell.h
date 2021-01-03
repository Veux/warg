#pragma once

#include "Globals.h"
#include "Scene_Graph.h"
#include "Physics.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

enum class Spell_Targets
{
  Self,
  Ally,
  Hostile
};

struct Character_Stats
{
  void operator*=(const Character_Stats &b);
  friend Character_Stats operator*(const Character_Stats &a, const Character_Stats &b);

  float32 global_cooldown = 1.f;
  float32 speed = 1.f;
  float32 cast_speed = 1.f;
  float32 cast_damage = 1.f;
  float32 mana_regen = 1.f;
  float32 hp_regen = 1.f;
  float32 damage_mod = 1.f;
  float32 atk_dmg = 1.f;
  float32 atk_speed = 1.f;
};

struct Spell_Database
{
  Spell_Database();

  std::unordered_map<std::string_view, UID> by_name;
  std::unordered_map<UID, std::string> name;
  std::unordered_map<UID, Texture> icon;
  std::unordered_map<UID, Spell_Targets> targets;
  std::unordered_map<UID, int32> mana_cost;
  std::unordered_map<UID, float32> range;
  std::unordered_map<UID, float32> cast_time;
  std::unordered_set<UID> on_gcd;
  std::unordered_map<UID, float32> cooldown;
  std::unordered_map<UID, std::vector<UID>> spell_release_buff_application;
  std::unordered_set<UID> spell_release_blink;
  std::unordered_set<UID> spell_release_demonic_circle_summon;
  std::unordered_set<UID> spell_release_demonic_circle_teleport;
  std::unordered_map<UID, UID> spell_release_object_creation;

  std::unordered_map<UID, std::string> sobj_name;
  std::unordered_map<std::string_view, UID> sobj_by_name;
  std::unordered_map<UID, float32> sobj_speed;
  std::unordered_map<UID, int32> sobj_hit_damage;
  std::unordered_set<UID> sobj_hit_seed_of_corruption;
  std::unordered_map<UID, UID> sobj_hit_buff_application;

  std::unordered_map<UID, std::string> buff_name;
  std::unordered_map<std::string_view, UID> buff_by_name;
  std::unordered_map<UID, Texture> buff_icon;
  std::unordered_map<UID, float32> buff_duration;
  std::unordered_map<UID, float32> buff_tick_freq;
  std::unordered_map<UID, Character_Stats> buff_stats_modifiers;
  std::unordered_set<UID> buff_is_debuff;
  std::unordered_map<UID, int32> buff_tick_damage;
  std::unordered_set<UID> buff_end_demonic_circle_destroy;
  std::unordered_set<UID> buff_end_seed_of_corruption;
  std::unordered_set<UID> buff_damage_seed_of_corruption;
};

extern Spell_Database SPELL_DB;
