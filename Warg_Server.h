#pragma once

#include "Globals.h"
#include "Physics.h"
#include "Warg_Event.h"
#include "Warg_Common.h"
#include "Peer.h"
#include <enet/enet.h>
#include <map>
#include <queue>

using std::vector;
using std::queue;
using std::unique_ptr;
using std::make_unique;


// todo: change all built in dynamic types to static types, eg int, float
// todo: change all instances of dt to use float64 for identical precision with game loop
struct Warg_Server
{
  Warg_Server();
  void update(float32 dt);
  void connect(std::shared_ptr<Peer> peer);
  void process_messages();

  UID add_char(int team, const char *name);
  UID add_dummy();
  Cast_Error cast_viable(UID caster_, UID target_, Spell_Status *spell);
  void try_cast_spell(Character &caster, UID target_id, Spell_Index spell_formula_index);
  void cast_spell(UID caster, UID target, Spell_Status *spell);
  void begin_cast(UID caster_, UID target_, Spell_Status *spell);
  void interrupt_cast(UID caster_);
  void update_cast(UID caster_, float32 dt);
  void release_spell(UID caster_, UID target_, Spell_Status *spell);
  void invoke_spell_effect(Spell_Effect &effect);
  void invoke_spell_effect_aoe(Spell_Effect &effect);
  void invoke_spell_effect_apply_buff(Spell_Effect &effect);
  void invoke_spell_effect_clear_debuffs(Spell_Effect &effect);
  void invoke_spell_effect_damage(Spell_Effect &effect);
  void invoke_spell_effect_heal(Spell_Effect &effect);
  void invoke_spell_effect_interrupt(Spell_Effect &effect);
  void invoke_spell_effect_object_launch(Spell_Effect &effect);
  void invoke_spell_effect_blink(Spell_Effect &effect);
  void update_buffs(UID character);
  void update_target(UID ch);
  bool update_spell_object(Spell_Object *so);
  Character *get_character(UID id);
  void apply_character_modifiers(Character *character);

  std::map<UID, std::shared_ptr<Peer>> peers;
  float64 time = 0;
  uint32 tick = 0;

  Map map;
  Flat_Scene_Graph scene;
  Spell_Database spell_db;
  Game_State game_state;
  vector<Triangle> collider_cache;
};
