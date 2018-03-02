#pragma once

#define SIM_LATENCY 0

#include "Spell.h"
#include <enet/enet.h>

typedef uint32_t UID;

struct Warg_Server;
struct Warg_State;

struct Buffer
{
  void reserve(size_t size);
  void insert(void *d, size_t size);
  void *read(size_t size);

  std::vector<uint8_t> data;
  size_t wnext = 0, rnext = 0;
};

enum class Warg_Event_Type
{
  CharSpawnRequest,
  CharSpawn,
  PlayerControl,
  PlayerMovement,
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

struct Message
{
  virtual void handle(Warg_Server &server) = 0;
  virtual void handle(Warg_State &state) = 0;
  virtual void serialize_(Buffer &buffer) = 0;

  float64 t = 0;
  UID peer = 0;
  bool reliable = true;
};

struct Char_Spawn_Request_Message : Message
{
  Char_Spawn_Request_Message(const char *name, uint8_t team);
  Char_Spawn_Request_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize_(Buffer &b);

  std::string name;
  uint8_t team;
};

struct Char_Spawn_Message : Message
{
  Char_Spawn_Message(UID id, const char *name_, uint8_t team_);
  Char_Spawn_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

  UID id;
  std::string name;
  uint8_t team;
};

struct Player_Control_Message : Message
{
  Player_Control_Message(UID character_);
  Player_Control_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

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

struct Player_Movement_Message : Message
{
  Player_Movement_Message(Move_Status move_status, vec3 dir);
  Player_Movement_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize_(Buffer &b);

  Move_Status move_status;
  vec3 dir;
};

struct Jump_Message : Message 
{
  Jump_Message();
  Jump_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize_(Buffer &b);
};

struct Char_Pos_Message : Message
{
  Char_Pos_Message(UID character, vec3 dir, vec3 pos);
  Char_Pos_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

  UID character;
  vec3 dir;
  vec3 pos;
};

struct Cast_Message : Message
{
  Cast_Message(UID target, const char *spell);
  Cast_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize_(Buffer &b);

  UID target;
  std::string spell;
};

struct Cast_Error_Message : Message
{
  Cast_Error_Message(UID caster, UID target, const char *spell, uint8_t err);
  Cast_Error_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

  UID caster;
  UID target;
  std::string spell;
  uint8_t err;
};

struct Cast_Begin_Message : Message
{
  Cast_Begin_Message(UID caster, UID target, const char *spell);
  Cast_Begin_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

  UID caster;
  UID target;
  std::string spell;
};

struct Cast_Interrupt_Message : Message
{
  Cast_Interrupt_Message(UID caster);
  Cast_Interrupt_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

  UID caster;
};

struct Char_HP_Message : Message
{
  Char_HP_Message(UID character, int hp);
  Char_HP_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

  UID character;
  int hp;
};

struct Buff_Application_Message : Message
{
  Buff_Application_Message(UID character, const char *buff);
  Buff_Application_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

  UID character;
  std::string buff;
};

struct Object_Launch_Message : Message
{
  Object_Launch_Message(UID object, UID caster, UID target, vec3 pos);
  Object_Launch_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize_(Buffer &b);

  UID object;
  UID caster;
  UID target;
  vec3 pos;
};

struct Object_Destroy_Message : Message
{
  Object_Destroy_Message();
  Object_Destroy_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize_(Buffer &b);
};

std::unique_ptr<Message> deserialize_message(Buffer &b);