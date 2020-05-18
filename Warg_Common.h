#pragma once
#include "Physics.h"
#include "Spell.h"
#include <map>
#include <string>
#include <vector>

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

  uint32 number = 0;
  Move_Status m = Move_Status::None;
  quat orientation = quat();
};

struct Input_Buffer
{
  Input &operator[](size_t i);
  Input back();
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

  vec3 position = vec3(0.f);
  quat orientation = quat();
  vec3 velocity = vec3(0.f);
  bool grounded = false;
  uint32 command_number = 0;
  Input command;
};

struct Character
{
  void update_hp(float32 dt);
  void update_mana(float32 dt);
  void update_global_cooldown(float32 dt);
  void take_heal(float32 heal);
  void apply_buff(Buff *buff);
  void apply_debuff(Buff *debuff);
  Buff *find_buff(Spell_ID buff_id);
  Buff *find_debuff(Spell_ID debuff_id);
  void remove_buff(Spell_ID buff_id);
  void remove_debuff(Spell_ID debuff_id);

  UID id;

  Character_Physics physics;
  vec3 radius = vec3(0.5f) * vec3(.39, 0.30, 1.61); // avg human in meters

  char name[MAX_CHARACTER_NAME_LENGTH + 1] = {};
  int team;

  Character_Stats base_stats;
  Character_Stats effective_stats;

  Buff buffs[MAX_BUFFS];
  Buff debuffs[MAX_DEBUFFS];
  uint8 buff_count = 0;
  uint8 debuff_count = 0;

  int32 hp, hp_max;
  float32 mana, mana_max;
  bool alive = true;

  float64 attack_cooldown = 0;
  float64 global_cooldown = 0;

  bool silenced = false;
};

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

struct Character_Target
{
  UID c;

  UID t;
};

struct Character_Cast
{
  UID caster;

  UID target;
  Spell_Index spell;
  float32 progress;
};

struct Spell_Cooldown
{
  UID character;
  Spell_Index spell;

  float32 cooldown_remaining;
};

struct Character_Spell
{
  UID character;
  Spell_Index spell;
};

struct Game_State
{
  Character *get_character(UID id);
  void damage_character(Character *subject, Character *object, float32 damage);

  Map *map = nullptr;

  uint32 tick = 0;
  uint32 input_number = 0;
  std::vector<Character> characters;
  Spell_Object spell_objects[MAX_SPELL_OBJECTS];
  uint8 spell_object_count = 0;

  std::vector<Character_Target> character_targets;
  std::vector<Character_Spell> character_spells;
  std::vector<Character_Cast> character_casts;
  std::vector<Spell_Cooldown> spell_cooldowns;
};

void move_char(Character &character, Input command, Flat_Scene_Graph *scene);
void collide_and_slide_char(
    Character_Physics &phys, vec3 &radius, const vec3 &vel, const vec3 &gravity, Flat_Scene_Graph *scene);
void update_spell_objects(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene);
void update_buffs(Game_State &game_state, Spell_Database &spell_db, UID character_id);
Cast_Error cast_viable(Game_State &game_state, Spell_Database &spell_db, UID caster_id, UID target_id,
    Spell_Index spell_id, bool ignore_gcd, bool ignore_already_casting);
void release_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id,
    UID target_id, Spell_Index spell_id);
void begin_cast(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id, UID target_id,
    Spell_Index spell_id);
void try_cast_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id,
    UID target_id, Spell_Index spell_id);
void try_cast_spell(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene, UID caster_id,
    UID target_id, Spell_Index spell_id);
UID add_dummy(Game_State &game_state, Map *map, vec3 position);
UID add_char(Game_State &game_state, Map *map, Spell_Database &spell_db, int team, const char *name);
void update_game(Game_State &game_state, Map *map, Spell_Database &spell_db, Flat_Scene_Graph &scene, float32 dt);
void update_spell_cooldowns(std::vector<Spell_Cooldown> &spell_cooldowns, float32 dt);