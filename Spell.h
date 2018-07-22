#pragma once

#include "Globals.h"
#include "Scene_Graph.h"
#include <string>
#include <vector>

typedef uint32 Spell_Index;

struct Spell_Effect_Formula;
struct BuffDef;

struct Spell_Object_Formula
{
  Spell_Index index;
  std::string name;
  float32 speed;
  std::vector<size_t> effects;
};

struct Spell_Object
{
  UID id;
  Spell_Index formula_index;
  UID caster;
  UID target;
  vec3 pos;
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
  Spell_Index index;
  std::string name;
  Texture icon;
  uint32 mana_cost = 0;
  float32 range = 0.f;
  Spell_Targets targets = Spell_Targets::Self;
  float32 cooldown = 0.f;
  float32 cast_time = 0.f;
  bool on_global_cooldown = true;
  std::vector<size_t> effects;
};

struct Damage_Effect
{
  uint32 amount;
  bool pierce_absorb;
  bool pierce_mod;
};

struct Heal_Effect
{
  uint32 amount;
};

struct Apply_Buff_Effect
{
  size_t buff_formula;
};

struct Apply_Debuff_Effect
{
  size_t debuff_formula;
};

struct Area_Effect
{
  Spell_Targets targets;
  float32 radius;
  size_t effect_formula;
};

struct Clear_Debuffs_Effect
{
};

struct Object_Launch_Effect
{
  Spell_Index object_formula;
};

struct Interrupt_Effect
{
  float32 lockout_duration;
};

struct Blink_Effect
{
  float32 distance;
};

enum class Spell_Effect_Type
{
  Heal,
  Damage,
  Apply_Buff,
  Apply_Debuff,
  Area,
  Clear_Debuffs,
  Object_Launch,
  Interrupt,
  Blink
};

struct Spell_Effect_Formula
{
  Spell_Index index;
  std::string name;
  Spell_Effect_Type type;
  union {
    Heal_Effect heal;
    Damage_Effect damage;
    Apply_Buff_Effect apply_buff;
    Apply_Debuff_Effect apply_debuff;
    Area_Effect area;
    Clear_Debuffs_Effect clear_debuffs;
    Object_Launch_Effect object_launch;
    Interrupt_Effect interrupt;
    Blink_Effect blink;
  };
};

struct Spell_Effect
{
  Spell_Index formula_index;
  UID caster;
  UID target;
  vec3 position;
};

struct Damage_Taken_Modifier
{
  float32 factor;
};

struct Speed_Modifier
{
  float32 factor;
};

struct Cast_Speed_Modifier
{
  float32 factor;
};

struct Silence_Modifier
{
};

enum class Character_Modifier_Type
{
  DamageTaken,
  Speed,
  CastSpeed,
  Silence
};

struct CharMod
{
  Spell_Index index;
  Character_Modifier_Type type;
  union {
    Damage_Taken_Modifier damage_taken;
    Speed_Modifier speed;
    Cast_Speed_Modifier cast_speed;
    Silence_Modifier silence;
  };
};

struct BuffDef
{
  Spell_Index index;
  std::string name;
  Texture icon;
  float32 duration;
  float32 tick_freq;
  std::vector<Spell_Index> tick_effects;
  std::vector<Spell_Index> character_modifiers;
};

struct Buff
{
  Spell_Index formula_index;
  float64 duration;
  float64 time_since_last_tick = 0;
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
  Spell_Effect_Formula *get_spell_effect(Spell_Index index);
  Spell_Object_Formula *get_spell_object(Spell_Index index);
  BuffDef *get_buff(Spell_Index index);
  CharMod *get_character_modifier(Spell_Index index);

private:
  Spell_Formula *add_spell();
  Spell_Effect_Formula *add_spell_effect();
  Spell_Object_Formula *add_spell_object();
  BuffDef *add_buff();
  CharMod *add_character_modifier();

  std::vector<Spell_Formula> spells;
  std::vector<Spell_Effect_Formula> effects;
  std::vector<Spell_Object_Formula> objects;
  std::vector<BuffDef> buffs;
  std::vector<CharMod> char_mods;
};
