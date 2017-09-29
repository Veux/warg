#pragma once

#define SIM_LATENCY 0

#include "Spell.h"

struct CharSpawnRequest_Event
{
  char *name;
  int team;
};

struct CharSpawn_Event
{
  char *name;
  int team;
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
  int character;
  vec3 dir;
};

struct Move_Event
{
  int character;
  Move_Status m;
};

struct CharPos_Event
{
  int character;
  vec3 dir;
  vec3 pos;
};

struct Cast_Event
{
  int character;
  int target;
  char *spell;
};

struct CastError_Event
{
  int caster;
  int target;
  char *spell;
  int err;
};

struct CastBegin_Event
{
  int caster;
  int target;
  char *spell;
};

struct CastInterrupt_Event
{
  int caster;
};

struct CharHP_Event
{
  int character;
  int hp;
};

struct BuffAppl_Event
{
  int character;
  char *buff;
};

struct ObjectLaunch_Event
{
  int object;
  int caster;
  int target;
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
  Dir,
  Move,
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
};

Warg_Event char_spawn_request_event(const char *name, int team);
Warg_Event char_spawn_event(const char *name, int team);
Warg_Event dir_event(int ch, vec3 dir);
Warg_Event move_event(int ch, Move_Status m);
Warg_Event char_pos_event(int ch, vec3 dir, vec3 pos);
Warg_Event cast_event(int caster, int target, const char *spell);
Warg_Event cast_error_event(int caster, int target, const char *spell, int err);
Warg_Event cast_begin_event(int caster, int target, const char *spell);
Warg_Event cast_interrupt_event(int caster);
Warg_Event char_hp_event(int caster, int hp);
Warg_Event buff_appl_event(int character, const char *buff);
Warg_Event object_launch_event(int object, int caster, int target, vec3 pos);

void free_warg_event(Warg_Event ev);
