#pragma once

#include "Globals.h"
#include "Physics.h"
#include "Warg_Event.h"
#include <queue>

using std::vector;
using std::queue;
using std::unique_ptr;

struct Character;

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
  void process_event(Jump_Event *jump);
  void process_event(Cast_Event *cast);

  void add_char(int team, const char *name);
  void update_colliders();
  void collide_and_slide_char(int ci, const vec3 &vel, const vec3 &gravity);
  vec3 collide_char_with_world(int ci, const vec3 &pos, const vec3 &vel);
  vec3 sphere_sweep(int ci, vec3 pos, vec3 vel);
  void check_collision(Collision_Packet &colpkt);
  void move_char(int ci, float32 dt);
  CastErrorType cast_viable(int caster_, int target_, Spell *spell);
  void try_cast_spell(int caster, int target, const char *spell);
  void cast_spell(int caster, int target, Spell *spell);
  void begin_cast(int caster_, int target_, Spell *spell);
  void interrupt_cast(int caster_);
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
  void update_hp(int ch, float32 dt);
  void update_mana(int ch, float32 dt);
  void update_gcd(int ch, float32 dt);
  void update_cds(int ch, float32 dt);
  bool update_spell_object(SpellObjectInst *so);
  void apply_char_mods(int ch);

  queue<Warg_Event> eq;
  vector<Warg_Connection> connections;

  Map map;
  Scene_Graph scene;
  vector<Triangle> colliders;
  unique_ptr<SpellDB> sdb;
  vector<Character> chars;
  vector<SpellObjectInst> spell_objs;
};
