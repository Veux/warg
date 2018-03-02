#pragma once

#include "Globals.h"
#include "Physics.h"
#include "Warg_Event.h"
#include <enet/enet.h>
#include <map>
#include <queue>

using std::queue;
using std::unique_ptr;
using std::vector;

struct Character;

struct Warg_Peer
{
  ENetPeer *peer;
  queue<Warg_Event> *in, *out;
  UID character = -1;
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
  vector<Warg_Event> tick_events;
  std::map<UID, Warg_Peer> peers;
  ENetHost *server;
  bool local = true;

  Map map;
  Scene_Graph scene;
  vector<Triangle> colliders;
  unique_ptr<SpellDB> sdb;
  std::map<UID, Character> chars;
  vector<SpellObjectInst> spell_objs;
};
