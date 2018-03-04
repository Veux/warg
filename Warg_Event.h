#pragma once

#define SIM_LATENCY 200

#include "Spell.h"
#include <map>
#include <enet/enet.h>

typedef uint32_t UID;

struct Warg_Server;
struct Warg_State;
struct Character;

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
  PlayerGeometry,
  Jump,
  Cast,
  CastError,
  CastBegin,
  CastInterrupt,
  CharHP,
  BuffAppl,
  ObjectLaunch,
  Ping,
  Ack,
  Connection
};

struct Message
{
  virtual void handle(Warg_Server &server) = 0;
  virtual void handle(Warg_State &state) = 0;
  virtual void serialize(Buffer &buffer) = 0;

  uint32_t tick;
  UID peer;
  bool reliable = true;
  float64_t t;
};

struct Connection_Message : Message
{
  Connection_Message(uint32_t tick);
  Connection_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);
};

struct Char_Spawn_Request_Message : Message
{
  Char_Spawn_Request_Message(uint32_t tick, const char *name, uint8_t team);
  Char_Spawn_Request_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize(Buffer &b);

  std::string name;
  uint8_t team;
};

struct Char_Spawn_Message : Message
{
  Char_Spawn_Message(uint32_t tick, UID id, const char *name_, uint8_t team_);
  Char_Spawn_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID id;
  std::string name;
  uint8_t team;
};

struct Player_Control_Message : Message
{
  Player_Control_Message(uint32_t tick, UID character_);
  Player_Control_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID character;
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

struct Player_Movement_Message : Message
{
  Player_Movement_Message(uint32_t tick, Move_Status move_status, vec3 dir);
  Player_Movement_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize(Buffer &b);

  Move_Status move_status;
  vec3 dir;
};

struct Cast_Message : Message
{
  Cast_Message(uint32_t tick, UID target, const char *spell);
  Cast_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize(Buffer &b);

  UID target;
  std::string spell;
};

struct Cast_Error_Message : Message
{
  Cast_Error_Message(uint32_t tick, UID caster, UID target, const char *spell, uint8_t err);
  Cast_Error_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID caster;
  UID target;
  std::string spell;
  uint8_t err;
};

struct Cast_Begin_Message : Message
{
  Cast_Begin_Message(uint32_t tick, UID caster, UID target, const char *spell);
  Cast_Begin_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID caster;
  UID target;
  std::string spell;
};

struct Cast_Interrupt_Message : Message
{
  Cast_Interrupt_Message(uint32_t tick, UID caster);
  Cast_Interrupt_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID caster;
};

struct Char_HP_Message : Message
{
  Char_HP_Message(uint32_t tick, UID character, int hp);
  Char_HP_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID character;
  int hp;
};

struct Buff_Application_Message : Message
{
  Buff_Application_Message(uint32_t tick, UID character, const char *buff);
  Buff_Application_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID character;
  std::string buff;
};

struct Object_Launch_Message : Message
{
  Object_Launch_Message(uint32_t tick, UID object, UID caster, UID target, vec3 pos);
  Object_Launch_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID object;
  UID caster;
  UID target;
  vec3 pos;
};

struct Player_Geometry_Message : Message
{
  Player_Geometry_Message(uint32_t tick, std::map<UID, Character> &chars);
  Player_Geometry_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  std::vector<UID> character_ids;
  std::vector<vec3> character_pos;
  std::vector<vec3> character_dir;
  std::vector<vec3> character_vel;
};

struct Ping_Message : Message
{
  Ping_Message(uint32_t tick);
  Ping_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);
};

struct Ack_Message : Message
{
  Ack_Message(uint32_t tick);
  Ack_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);
};

std::unique_ptr<Message> deserialize_message(Buffer &b);