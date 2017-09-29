#pragma once

#include "Functions.h"
#include "Globals.h"
#include "Warg_Event.h"
#include <queue>

using std::vector;
using std::queue;
using std::unique_ptr;

struct Character;
struct Wall;

struct Warg_Connection
{
  queue<Warg_Event> *in, *out;
};

struct Warg_Server
{
  Warg_Server();
  void update(float32 dt);
  void connect(queue<Warg_Event> *in, queue<Warg_Event> *out);

private:
  void push(Warg_Event ev);

  void process_events();
  void process_event(CharSpawnRequest_Event *req);
  void process_event(Dir_Event *dir);
  void process_event(Move_Event *mv);
  void process_event(Cast_Event *cast);

  void add_char(int team, const char *name);
  void move_char(int ci, float32 dt);
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

  queue<Warg_Event> eq;
  vector<Warg_Connection> connections;

  Map map;
  unique_ptr<SpellDB> sdb;
  vector<Character> chars;
  vector<SpellObjectInst> spell_objs;
};
