#pragma once
#include "Physics.h"
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
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size,
      const char *address, const char *char_name, uint8_t team);
  void update();
  virtual void handle_input_events(const std::vector<SDL_Event> &events,
      bool block_kb, bool block_mouse) final;

  std::unique_ptr<Warg_Client> client;
  std::unique_ptr<Warg_Server> server;
  std::queue<Warg_Event> in, out;
  bool local;
  ENetPeer *serverp;
  ENetHost *clientp;
};

struct Wall
{
  vec3 p1;
  vec2 p2;
  float32 h;
};

Map make_nagrand();
Map make_blades_edge();
