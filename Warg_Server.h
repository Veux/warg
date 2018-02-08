#pragma once

#include "Globals.h"
#include "Physics.h"
#include "Warg_Event.h"
#include "Warg_Common.h"
#include <enet/enet.h>
#include <map>
#include <queue>

using std::vector;
using std::queue;
using std::unique_ptr;

struct Warg_Peer
{
  ENetPeer *peer;
  queue<Warg_Event> *in, *out;
  UID character = 0;
};

struct Warg_Server
{
  Warg_Server(bool local);
  void update(float32 dt);
  void connect(queue<Warg_Event> *in, queue<Warg_Event> *out);

private:
  void push(Warg_Event ev);
  void send_event(Warg_Peer &p, Warg_Event ev);
  void send_events();
  void process_packets();

  void process_event(Warg_Event ev);
  void process_events();
  void process_char_spawn_request_event(Warg_Event ev);
  void process_dir_event(Warg_Event ev);
  void process_move_event(Warg_Event ev);
  void process_jump_event(Warg_Event ev);
  void process_cast_event(Warg_Event ev);

  UID add_char(int team, const char *name);
  CastErrorType cast_viable(UID caster_, UID target_, Spell *spell);
  void try_cast_spell(Character &caster, UID target, const char *spell);
  void cast_spell(UID caster, UID target, Spell *spell);
  void begin_cast(UID caster_, UID target_, Spell *spell);
  void interrupt_cast(UID caster_);
  void update_cast(UID caster_, float32 dt);
  void release_spell(UID caster_, UID target_, Spell *spell);
  void invoke_spell_effect(SpellEffectInst &effect);
  void invoke_spell_effect_aoe(SpellEffectInst &effect);
  void invoke_spell_effect_apply_buff(SpellEffectInst &effect);
  void invoke_spell_effect_clear_debuffs(SpellEffectInst &effect);
  void invoke_spell_effect_damage(SpellEffectInst &effect);
  void invoke_spell_effect_heal(SpellEffectInst &effect);
  void invoke_spell_effect_interrupt(SpellEffectInst &effect);
  void invoke_spell_effect_object_launch(SpellEffectInst &effect);
  void update_buffs(UID character, float32 dt);
  void update_target(UID ch);
  bool update_spell_object(SpellObjectInst *so);

  queue<Warg_Event> eq;
  vector<Warg_Event> tick_events;
  std::map<UID, Warg_Peer> peers;
  ENetHost *server;
  bool local = true;

  Map map;
  vector<Triangle> collider_cache;
  Scene_Graph scene;
  unique_ptr<SpellDB> sdb;
  std::map<UID, Character> chars;
  vector<SpellObjectInst> spell_objs;
};
