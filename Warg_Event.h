#pragma once

#define SIM_LATENCY 0

#include "Spell.h"
#include <enet/enet.h>

typedef uint32_t UID;

struct CharSpawnRequest_Event
{
  char *name;
  uint8_t team;
};

struct CharSpawn_Event
{
  UID id;
  char *name;
  uint8_t team;
};

struct PlayerControl_Event
{
  UID character;
};

enum Move_Status
{
  None = 0,
  Forwards = 1 << 0,
  Left = 1 << 1,
  Backwards = 1 << 2,
  Right = 1 << 3
};

struct Dir_Event
{
  uint8_t character;
  vec3 dir;
};

struct Move_Event
{
  uint8_t character;
  Move_Status m;
};

struct Jump_Event
{
  int character;
};

struct CharPos_Event
{
  uint8_t character;
  vec3 dir;
  vec3 pos;
};

struct Cast_Event
{
  uint8_t character;
  uint8_t target;
  char *spell;
};

struct CastError_Event
{
  uint8_t caster;
  uint8_t target;
  char *spell;
  uint8_t err;
};

struct CastBegin_Event
{
  uint8_t caster;
  uint8_t target;
  char *spell;
};

struct CastInterrupt_Event
{
  uint8_t caster;
};

struct CharHP_Event
{
  uint8_t character;
  int hp;
};

struct BuffAppl_Event
{
  uint8_t character;
  char *buff;
};

struct ObjectLaunch_Event
{
  int object;
  uint8_t caster;
  uint8_t target;
  vec3 pos;
};

struct ObjectDestroy_Event
{
  // ????
};

enum class Warg_Event_Type
{
  CharSpawnRequest,
  CharSpawn,
  PlayerControl,
  Dir,
  Move,
  Jump,
  CharPos,
  Cast,
  CastError,
  CastBegin,
  CastInterrupt,
  CharHP,
  BuffAppl,
  ObjectLaunch,
  ObjectDestroy
};

struct Warg_Event
{
  Warg_Event_Type type;
  void *event;
  float64 t;
  UID peer;
};

enum class Data_Type
{
  Int,
  Char,
  Array
};

struct Buffer
{
  void reserve(size_t size);
  void insert(void *d, size_t size);
  void *read(size_t size);

  std::vector<uint8_t> data;
  size_t wnext = 0, rnext = 0;
};

Warg_Event char_spawn_request_event(const char *name, uint8_t team);
Warg_Event char_spawn_event(UID id, const char *name, uint8_t team);
Warg_Event player_control_event(UID character);
Warg_Event dir_event(uint8_t ch, vec3 dir);
Warg_Event move_event(uint8_t ch, Move_Status m);
Warg_Event jump_event(uint8_t ch);
Warg_Event char_pos_event(uint8_t ch, vec3 dir, vec3 pos);
Warg_Event cast_event(uint8_t caster, uint8_t target, const char *spell);
Warg_Event cast_error_event(
    uint8_t caster, uint8_t target, const char *spell, uint8_t err);
Warg_Event cast_begin_event(uint8_t caster, uint8_t target, const char *spell);
Warg_Event cast_interrupt_event(uint8_t caster);
Warg_Event char_hp_event(uint8_t caster, int hp);
Warg_Event buff_appl_event(uint8_t character, const char *buff);
Warg_Event object_launch_event(
    int object, uint8_t caster, uint8_t target, vec3 pos);

void free_warg_event(Warg_Event ev);
void serialize(Buffer &b, Warg_Event ev);
Warg_Event deserialize_event(Buffer &b);
