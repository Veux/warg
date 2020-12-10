#pragma once

#include "Globals.h"
#include "Physics.h"
#include "Warg_Event.h"
#include "Warg_Common.h"
#include "Peer.h"
#include <enet/enet.h>
#include <map>
#include <queue>


struct Warg_Server
{
  Warg_Server();
  ~Warg_Server();
  void update(float32 dt);
  void connect(std::shared_ptr<Peer> peer);
  void process_messages();

  UID add_char(int team, const char *name);
  UID add_dummy(vec3 position);
  Cast_Error cast_viable(UID caster_, UID target_, Spell_Status *spell, bool ignore_gcd);
  void try_cast_spell(Character &caster, UID target_id, Spell_Index spell_formula_index);
  void cast_spell(UID caster, UID target, Spell_Status *spell);
  void begin_cast(UID caster_, UID target_, Spell_Status *spell);
  void interrupt_cast(UID caster_);
  void update_cast(UID caster_, float32 dt);
  void release_spell(UID caster_, UID target_, Spell_Status *spell);
  void update_buffs(UID character);
  void update_target(UID ch);
  bool update_spell_object(Spell_Object *so);
  Character *get_character(UID id);

  std::map<UID, std::shared_ptr<Peer>> peers;
  float64 time = 0;
  uint32 tick = 0;
  std::atomic<bool> running = false;
  Map *map;
  Resource_Manager resource_manager;
  Scene_Graph scene;
  Spell_Database spell_db;
  Game_State game_state;
};
