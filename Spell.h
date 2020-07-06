#pragma once

#include "Globals.h"
#include "Scene_Graph.h"
#include "Physics.h"
#include <string>
#include <vector>

typedef uint32 Spell_Index;

struct Spell_Effect_Formula;
struct BuffDef;
struct Character;
struct Game_State;
struct Spell_Object;
struct Buff;

enum class Spell_ID
{
  Frostbolt,
  Sprint,
  Icy_Veins,
  Blink,
  Shadow_Word_Pain,
  Demonic_Circle_Summon,
  Demonic_Circle_Teleport,
  Corruption,
  Seed_of_Corruption,
  COUNT
};

struct Spell_Object_Formula
{
  Spell_ID _id;
  Spell_Index index;
  std::string name;
  float32 speed;
};

enum class Spell_Targets
{
  Self,
  Ally,
  Hostile,
  Terrain
};

struct Spell_Formula
{
  Spell_ID _id;
  Spell_Index index;
  std::string name;
  Texture_Descriptor icon;
  uint32 mana_cost = 0;
  float32 range = 0.f;
  Spell_Targets targets = Spell_Targets::Self;
  float32 cooldown = 0.f;
  float32 cast_time = 0.f;
  bool on_global_cooldown = true;
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

struct BuffDef
{
  Spell_Index index;
  Spell_ID _id;
  std::string name;
  Texture_Descriptor icon;
  float32 duration;
  float32 tick_freq;
  Character_Stats stats_modifiers;
};

enum class Cast_Error
{
  Success,
  Silenced,
  Global_Cooldown,
  Cooldown,
  Insufficient_Mana,
  Invalid_Target,
  Out_of_Range,
  Already_Casting
};

class Spell_Database
{
public:
  Spell_Database();
  Spell_Formula *get_spell(Spell_Index index);
  Spell_Formula *get_spell(const char *name);
  Spell_Object_Formula *get_spell_object(Spell_Index index);
  Spell_Object_Formula *get_spell_object(Spell_ID id);
  BuffDef *get_buff(Spell_Index index);
  BuffDef *get_buff(Spell_ID id);

private:
  Spell_Formula *add_spell();
  Spell_Object_Formula *add_spell_object();
  BuffDef *add_buff();

  std::vector<Spell_Formula> spells;
  std::vector<Spell_Object_Formula> objects;
  std::vector<BuffDef> buffs;
};

extern Spell_Database SPELL_DB;

void buff_on_end_dispatch(Spell_ID sid, UID c_id, Buff &b, Game_State &gs);
void buff_on_damage_dispatch(Spell_ID sid, UID subject_id, UID object_id, Buff &buff, float32 damage, Game_State &gs);
void buff_on_tick_dispatch(Spell_ID sid, UID c_id, Buff &b, Game_State &gs);
void spell_object_on_hit_dispatch(Spell_ID sid, Spell_Object &so, Game_State &gs, Flat_Scene_Graph &scene);
void spell_on_release_dispatch(Spell_ID sid, UID caster_id, UID target_id, Game_State &gs, Flat_Scene_Graph &scene);
