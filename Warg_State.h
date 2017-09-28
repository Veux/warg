#pragma once
#include "Spell.h"
#include "State.h"
#include "Warg_Client.h"
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
  Cylinder body;
  Node_Ptr mesh;

  vec3 pos;
  vec3 dir;

  std::string name;
  int team;
  int target = -1;

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
  int cast_target = -1;
};

struct Warg_State : protected State
{
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  void handle_input(
      State **current_state, std::vector<State *> available_states);
  void add_char(int team, const char *name);
  void process_client_events();

  Warg_Client client;
  Warg_Server server;
  std::queue<Warg_Event> in, out;
  std::vector<Node_Ptr> map_meshes;
};

Map make_nagrand();
