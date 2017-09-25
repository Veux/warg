#pragma once

#include "Globals.h"
#include "Warg_Event.h"
#include <queue>

struct Character;
struct Wall;

struct Warg_Server
{
  void process_events();
  void process_event(CharSpawnRequest_Event *req);
  void process_event(Move_Event *mv);
  void process_event(Cast_Event *cast);

  void move_char(int ci, vec3 v);
  CastErrorType cast_viable(int caster_, int target_, Spell *spell);
  void try_cast_spell(int caster, int target, const char *spell);
  void cast_spell(int caster, int target, Spell *spell);
  void begin_cast(int caster_, int target_, Spell *spell);
  void update_cast(int caster_, float32 dt);
  void release_spell(int caster_, int target_, Spell *spell);
  void invoke_spell_effect(SpellEffectInst &effect);
  void invoke_spell_effect_aoe(SpellEffectInst &effect);
  void invoke_spell_effect_apply_buff(SpellEffectInst &effect);
  void invoke_spell_effect_clear_debuffs(SpellEffectInst &effect);
  void invoke_spell_effect_damage(SpellEffectInst &effect);
  void invoke_spell_effect_heal(SpellEffectInst &effect);
  void invoke_spell_effect_interrupt(SpellEffectInst &effect);
  void invoke_spell_effect_object_launch(SpellEffectInst &effect);
  void update_buffs(int character, float32 dt);
  void update_target(int ch);
  void update_mana(int ch);
  void update_gcd(int ch, float32 dt);
  void update_cds(int ch, float32 dt);
  bool update_spell_object(SpellObjectInst *so);
  void apply_char_mods(int ch);

  std::queue<Warg_Event> in;
  std::queue<Warg_Event> *out;

  std::vector<Character> chars;
  std::vector<Wall> walls;
  std::vector<SpellObjectInst> spell_objs;

  std::array<SpellObjectDef, 100> *spell_objects;
  uint32 *nspell_objects = 0;
};
