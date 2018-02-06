#pragma once
#include "Physics.h"
#include "Spell.h"
#include "State.h"
#include "Warg_Event.h"
#include "Warg_Server.h"
#include <queue>

struct CharStats
{
  float32 gcd;
  float32 speed;
  float32 cast_speed;
  float32 cast_damage;
  float32 mana_regen;
  float32 hp_regen;
  float32 damage_mod;
  float32 atk_dmg;
  float32 atk_speed;
};

struct Character
{
  UID id;

  Node_Ptr mesh;

  vec3 pos;
  vec3 dir;
  vec3 vel;
  vec3 radius = vec3(0.5f) * vec3(.39, 0.30, 1.61); // avg human in meters
  bool grounded;
  Move_Status move_status = Move_Status::None;
  Collision_Packet colpkt;
  int collision_recursion_depth;

  std::string name;
  int team;
  UID target = 0;

  CharStats b_stats, e_stats;

  std::map<std::string, Spell> spellbook;

  std::vector<Buff> buffs, debuffs;

  int32 hp, hp_max;
  float32 mana, mana_max;
  bool alive = true;

  float64 atk_cd = 0;
  float64 gcd = 0;

  bool silenced = false;
  bool casting = false;
  Spell *casting_spell;
  float32 cast_progress = 0;
  UID cast_target = 0;
};

struct Warg_State : protected State
{
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size);
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size,
      const char *address, const char *char_name, uint8_t team);
  void update();
  void handle_input(
      State **current_state, std::vector<State *> available_states);
  void process_packets();
  void process_events();
  void process_event(CharSpawn_Event *ev);
  void process_event(PlayerControl_Event *ev);
  void process_event(CharPos_Event *ev);
  void process_event(CastError_Event *ev);
  void process_event(CastBegin_Event *ev);
  void process_event(CastInterrupt_Event *ev);
  void process_event(CharHP_Event *ev);
  void process_event(BuffAppl_Event *ev);
  void process_event(ObjectLaunch_Event *ev);
  void add_char(UID id, int team, const char *name);

  std::unique_ptr<Warg_Server> server;
  std::queue<Warg_Event> in, out;
  bool local;
  ENetPeer *serverp;
  ENetHost *clientp;
  Map map;
  std::map<UID, Character> chars;
  UID pc = 0;
  unique_ptr<SpellDB> sdb;
  vector<SpellObjectInst> spell_objs;
};

struct Wall
{
  vec3 p1;
  vec2 p2;
  float32 h;
};

Map make_nagrand();
Map make_blades_edge();
