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
  UID id;

  Character_Physics physics;
  vec3 radius = vec3(0.5f) * vec3(.39, 0.30, 1.61); // avg human in meters

  char name[MAX_CHARACTER_NAME_LENGTH + 1] = {};
  int team;

  Character_Stats base_stats;
  Character_Stats effective_stats;

  float32 hp, hp_max;
  float32 mana, mana_max;
  bool alive = true;

  float32 attack_cooldown = 0;
  float32 global_cooldown = 0;

  bool silenced = false;
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

struct Character_Buff
{
  UID character;
  Buff buff;
};

struct Game_State
{
  Character *get_character(UID id);

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
  std::vector<Character_Buff> character_buffs;
  std::vector<Character_Buff> character_debuffs;
};

void move_char(Character &character, Input command, Flat_Scene_Graph *scene);
void collide_and_slide_char(
    Character_Physics &phys, vec3 &radius, const vec3 &vel, const vec3 &gravity, Flat_Scene_Graph *scene);
void update_spell_objects(Game_State &game_state, Spell_Database &spell_db, Flat_Scene_Graph &scene);
void update_buffs(Game_State &game_state, Spell_Database &spell_db);
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
void remove_debuff(Game_State &gs, Character &c, Spell_ID debuff_id);
void remove_buff(Game_State &gs, Character &c, Spell_ID buff_id);
Buff *find_buff(Game_State &gs, Character &c, Spell_ID debuff_id);
Buff *find_debuff(Game_State &gs, Character &c, Spell_ID debuff_id);
void apply_buff(Game_State &gs, Character &c, Buff *buff);
void apply_debuff(Game_State &gs, Character &c, Buff *debuff);
void regen_characters_hp(std::vector<Character> &cs, float32 dt);
void regen_characters_mana(std::vector<Character> &cs, float32 dt);
void update_global_cooldowns(std::vector<Character> &cs, float32 dt);
void heal_character(Character &c, int32 heal);
void damage_character(Game_State &gs, Character *subject, Character *object, float32 damage);