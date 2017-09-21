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
  void handle_input(State **current_state,
                    std::vector<State *> available_states);
  void add_char(int team, const char *name);
  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(0, 1, 0);

  Warg_Server server;
  Warg_Client client;

  void process_client_events();

  int pc = -1;

  std::vector<SpellObjectInst> spell_objs;

  vec3 ground_pos = vec3(8, 11, 0); // -0.1f); ??
  vec3 ground_dim = vec3(16, 22, 0.05);
  vec3 ground_dir = vec3(0, 1, 0);
  Node_Ptr ground_mesh;
  void add_wall(vec3 p1, vec2 p2, float32 h);
  std::vector<Wall> walls;
  std::vector<Node_Ptr> wall_meshes;

  vec3 spawnpos[2] = {{8, 2, 0.5f}, {8, 20, 0.5f}};
  vec3 spawndir[2] = {{0, 1, 0}, {0, -1, 0}};

  std::array<CharMod, 100> char_mods;
  std::array<BuffDef, 100> buffs;
  std::array<SpellDef, 100> spells;
  std::array<SpellEffect, 100> spell_effects;
  std::array<SpellObjectDef, 100> spell_objects;
  uint32 nchar_mods = 0;
  uint32 nbuffs = 0;
  uint32 nspell_effects = 0;
  uint32 nspell_objects = 0;
  uint32 nspells = 0;
};
