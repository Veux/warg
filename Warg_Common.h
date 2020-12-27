#pragma once
#include "Physics.h"
#include "Spell.h"
#include <map>
#include <string>
#include <vector>

struct Map
{
  Node_Index node;
  Mesh_Data mesh;
  Material_Descriptor material;

  vec3 spawn_pos[2];
  quat spawn_orientation[2];
};

struct Blades_Edge : public Map
{
  Blades_Edge(Flat_Scene_Graph &scene);
};

enum Move_Status
{
  None = 0,
  Forwards = 1 << 0,
  Left = 1 << 1,
  Backwards = 1 << 2,
  Right = 1 << 3,
  Jumping = 1 << 4
};

struct Input
{
  bool operator==(const Input &) const;
  bool operator!=(const Input &) const;

  uint32 sequence_number;
  int m = Move_Status::None;
  quat orientation = quat();
};

//struct Input_Buffer
//{
//  Input &operator[](size_t i);
//  Input back();
//  size_t size();
//  void push(Input &input);
//  void pop_older_than(uint32 input_number);
//
//private:
//  const size_t capacity = INPUT_BUFFER_SIZE;
//  std::array<Input, INPUT_BUFFER_SIZE> buffer;
//  size_t start = 0;
//  size_t end = 0;
//};

struct Character_Physics
{
  bool operator==(const Character_Physics &) const;
  bool operator!=(const Character_Physics &) const;

  vec3 position = vec3(0.f);
  quat orientation = quat();
  vec3 velocity = vec3(0.f);
  bool grounded = false;
};

struct Character
{
  UID id;

  Character_Physics physics;
  vec3 radius = vec3(0.5f) * vec3(.39, 0.30, 1.61); // avg human in meters
  std::string name;
  int team;
  Character_Stats base_stats;
};

struct Living_Character
{
  UID id;

  Character_Stats effective_stats;
  float32 hp, hp_max;
  float32 mana, mana_max;
};

struct Character_Target
{
  UID character;

  UID target;
};

struct Character_Cast
{
  UID caster;

  UID target;
  UID spell;
  float32 progress;
};

struct Spell_Cooldown
{
  UID character;
  UID spell;

  float32 cooldown_remaining;
};

struct Character_Spell
{
  UID character;
  UID spell;
};

struct Character_Buff
{
  UID character;
  UID buff_id;
  UID caster;

  float64 duration;
  float64 time_since_last_tick = 0;
};

struct Character_Silencing
{
  UID character;
  UID buff_id;
};

struct Character_GCD
{
  UID character;

  float32 remaining;
};

struct Spell_Object
{
  UID id;

  UID spell_id;
  UID caster;
  UID target;
  vec3 pos;
};

struct Demonic_Circle
{
  UID owner;

  vec3 position;
};

struct Seed_of_Corruption
{
  UID character;
  UID caster;
  float32 damage_taken;
};

struct Game_Static_Data
{
  Map map;
  Flat_Scene_Graph scene;
};

struct Game_State
{
  uint32 tick = 0;

  std::vector<Character> characters;
  std::vector<Living_Character> living_characters;
  std::vector<Character_Target> character_targets;
  std::vector<Character_Spell> character_spells;
  std::vector<Character_Cast> character_casts;
  std::vector<Spell_Cooldown> spell_cooldowns;
  std::vector<Character_Buff> character_buffs;
  std::vector<Character_Silencing> character_silencings;
  std::vector<Character_GCD> character_gcds;
  std::vector<Spell_Object> spell_objects;
  std::vector<Demonic_Circle> demonic_circles;
  std::vector<Seed_of_Corruption> seeds_of_corruption;
};

UID add_dummy(Game_State &game_state, Map &map, vec3 position);
UID add_char(Game_State &game_state, Map &map, int team, std::string_view name);
void move_char(Game_State &gs, Character &character, Input command, Flat_Scene_Graph &scene);
void collide_and_slide_char(
    Character_Physics &phys, vec3 &radius, const vec3 &vel, const vec3 &gravity, Flat_Scene_Graph &scene);
void update_spell_objects(Game_State &game_state, Flat_Scene_Graph &scene);
void update_buffs(std::vector<Character_Buff> &bs, Game_State &gs);
void cast_spell(Game_State &game_state, Flat_Scene_Graph &scene, UID caster_id, UID target_id, UID spell_id);
void update_game(Game_State &game_state, Map &map, Flat_Scene_Graph &scene, std::map<UID, Input> &inputs);
void damage_character(Game_State &gs, UID subject_id, UID object_id, float32 damage);
void end_buff(Game_State &gs, Character_Buff &cb);
Game_State predict(Game_State gs, Map &map, Flat_Scene_Graph &scene, UID character_id, Input input);
bool verify_prediction(Game_State &a, Game_State &b, UID character_id);
void merge_prediction(Game_State &dst, const Game_State &pred, UID character_id);