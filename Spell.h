#pragma once

#include "Globals.h"
#include "Scene_Graph.h"
#include <string>
#include <vector>

struct SpellEffect;
struct BuffDef;

struct SpellObjectDef
{
  std::string name;
  float32 speed;
  std::vector<SpellEffect *> effects;
};

struct SpellObjectInst
{
  SpellObjectDef def;
  int caster;
  int target;
  vec3 pos;
};

enum class SpellTargets
{
  Self,
  Ally,
  Hostile,
  Terrain
};

struct SpellDef
{
  std::string name;
  int mana_cost;
  float32 range;
  SpellTargets targets;
  float32 cooldown;
  float32 cast_time;
  std::vector<SpellEffect *> effects;
};

struct Spell
{
  SpellDef *def;
  float32 cd_remaining;
};

struct DamageEffect
{
  int n;
  bool pierce_absorb;
  bool pierce_mod;
};

struct HealEffect
{
  int n;
};

struct ApplyBuffEffect
{
  BuffDef *buff;
};

struct ApplyDebuffEffect
{
  BuffDef *debuff;
};

struct AoEEffect
{
  SpellTargets targets;
  float32 radius;
  SpellEffect *effect;
};

struct ClearDebuffsEffect
{
};

struct ObjectLaunchEffect
{
  int object;
};

struct InterruptEffect
{
  float32 lockout;
};

enum class SpellEffectType
{
  Heal,
  Damage,
  ApplyBuff,
  ApplyDebuff,
  AoE,
  ClearDebuffs,
  ObjectLaunch,
  Interrupt
};

struct SpellEffect
{
  std::string name;
  SpellEffectType type;
  union {
    HealEffect heal;
    DamageEffect damage;
    ApplyBuffEffect applybuff;
    ApplyDebuffEffect applydebuff;
    AoEEffect aoe;
    ClearDebuffsEffect cleardebuffs;
    ObjectLaunchEffect objectlaunch;
    InterruptEffect interrupt;
  };
};

struct SpellEffectInst
{
  SpellEffect def;
  int caster;
  int target;
  vec3 pos;
};

struct DamageTakenMod
{
  float32 n;
};

struct SpeedMod
{
  float32 m;
};

struct CastSpeedMod
{
  float32 m;
};

struct SilenceMod
{
};

enum class CharModType
{
  DamageTaken,
  Speed,
  CastSpeed,
  Silence
};

struct CharMod
{
  CharModType type;
  union {
    DamageTakenMod damage_taken;
    SpeedMod speed;
    CastSpeedMod cast_speed;
    SilenceMod silence;
  };
};

struct BuffDef
{
  std::string name;
  float32 duration;
  float32 tick_freq;
  std::vector<SpellEffect *> tick_effects;
  std::vector<CharMod *> char_mods;
};

struct Buff
{
  BuffDef def;
  float64 duration;
  uint32 ticks_left = 0;
};

enum class CastErrorType
{
  Success,
  Silenced,
  GCD,
  SpellCD,
  NotEnoughMana,
  InvalidTarget,
  OutOfRange,
  AlreadyCasting
};

struct SpellDB
{
  std::vector<SpellDef> spells;
  std::vector<SpellEffect> effects;
  std::vector<SpellObjectDef> objects;
  std::vector<BuffDef> buffs;
  std::vector<CharMod> char_mods;
};

std::unique_ptr<SpellDB> make_spell_db();
