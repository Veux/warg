#pragma once
#include "Physics.h"
#include "Spell.h"
#include "Warg_Event.h"
#include <map>
#include <string>
#include <vector>

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

struct Input
{
  bool operator==(const Input &) const;
  bool operator!=(const Input &) const;

  uint32 number = 0;
  Move_Status m = Move_Status::None;
  vec3 dir = vec3(0, 1, 0);
};

struct Input_Buffer
{
  Input &operator[](size_t i);
  size_t size();
  void push(Input &input);
  void pop_older_than(uint32 input_number);

private:
  const size_t capacity = INPUT_BUFFER_SIZE;
  std::array<Input, INPUT_BUFFER_SIZE> buffer;
  size_t start = 0;
  size_t end = 0;
};

struct Character_Physics
{
  bool operator==(const Character_Physics &) const;
  bool operator!=(const Character_Physics &) const;
  bool operator<(const Character_Physics &) const;

  vec3 pos = vec3(0), dir = vec3(0), vel = vec3(0);
  bool grounded = false;
  uint32 cmdn = 0;
  Input command;
};

struct Character
{
  void update_hp(float32 dt);
  void update_mana(float32 dt);
  void update_spell_cooldowns(float32 dt);
  void update_global_cooldown(float32 dt);
  void apply_modifiers();

  UID id;

  Character_Physics physics;
  vec3 radius = vec3(0.5f) * vec3(1.f); /*vec3(.39, 0.30, 1.61);*/ // avg human in meters

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

private:
  void apply_modifier(CharMod &modifier);
};

struct Map
{
  Node_Ptr node;
  Mesh_Data mesh;
  Material_Descriptor material;

  vec3 spawn_pos[2];
  vec3 spawn_dir[2];
};

struct Game_State
{
  uint32 tick = 0;
  uint32 input_number = 0;
  std::map<UID, Character> characters;
  std::map<UID, SpellObjectInst> spell_objects;
};

Map make_blades_edge();
std::vector<Triangle> collect_colliders(Scene_Graph &scene);
void move_char(Character &character, Input command, std::vector<Triangle> colliders);