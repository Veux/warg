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
using std::make_unique;

struct Warg_Peer
{
  ENetPeer *peer;
  queue<unique_ptr<Message>> *in, *out;
  UID character = 0;
  vector<unique_ptr<Message>> tick_events;
  Input last_input;
};
//todo: change all built in dynamic types to static types, eg int, float
//todo: change all instances of dt to use float64 for identical precision with game loop
struct Warg_Server
{
  Warg_Server(bool local);
  void update(float32 dt);
  void connect(queue<unique_ptr<Message>> *in, queue<unique_ptr<Message>> *out);

  void push(unique_ptr<Message> ev);
  void push_to(unique_ptr<Message> ev, Warg_Peer &peer);
  void send_event(Warg_Peer &p, unique_ptr<Message> &ev);
  void send_events();
  void process_packets();
  void process_events();

  UID add_char(int team, const char *name);
  UID add_dummy();
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

  queue<unique_ptr<Message>> eq;
  vector<unique_ptr<Message>> tick_events;
  std::map<UID, Warg_Peer> peers;
  ENetHost *server;
  bool local = true;
  float64 time = 0;
  uint32 tick = 0;

  Map map;
  vector<Triangle> collider_cache;
  Scene_Graph scene;
  unique_ptr<SpellDB> sdb;
  Game_State game_state;

  UID dummy_id = 0;
};
