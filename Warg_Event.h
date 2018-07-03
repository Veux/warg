#pragma once

#define SIM_LATENCY 500

#include "Spell.h"
#include <enet/enet.h>
#include <map>

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
  Ack
};

struct Message
{
  virtual void handle(Warg_Server &server) = 0;
  virtual void handle(Warg_State &state) = 0;
  virtual void serialize(Buffer &buffer) = 0;

  UID peer;
  bool reliable = true;
  float64_t t;
};

struct Char_Spawn_Request_Message : Message
{
  Char_Spawn_Request_Message(const char *name, uint8_t team);
  Char_Spawn_Request_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize(Buffer &b);

  std::string name;
  uint8_t team;
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

struct Input_Message : Message
{
  Input_Message(uint32_t i_, Move_Status move_status, vec3 dir);
  Input_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize(Buffer &b);

  uint32_t input_number;
  Move_Status move_status;
  vec3 dir;
};

struct Cast_Message : Message
{
  Cast_Message(UID target, const char *spell);
  Cast_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize(Buffer &b);

  UID target;
  std::string spell;
};

struct Cast_Error_Message : Message
{
  Cast_Error_Message(UID caster, UID target, const char *spell, uint8_t err);
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
  Cast_Begin_Message(UID caster, UID target, const char *spell);
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
  Cast_Interrupt_Message(UID caster);
  Cast_Interrupt_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID caster;
};

struct Char_HP_Message : Message
{
  Char_HP_Message(UID character, int hp);
  Char_HP_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID character;
  int hp;
};

struct Buff_Application_Message : Message
{
  Buff_Application_Message(UID character, const char *buff);
  Buff_Application_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID character;
  std::string buff;
};

struct Object_Launch_Message : Message
{
  Object_Launch_Message(UID object, UID caster, UID target, vec3 pos);
  Object_Launch_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID object;
  UID caster;
  UID target;
  vec3 pos;
};

struct Input;

struct State_Message : Message
{
  State_Message(UID pc, std::map<UID, Character> &chars, uint32 tick_, uint32 input_number_);
  State_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID pc;
  uint32 tick;
  uint32 input_number;
  std::map<UID, Character> characters;
};

struct Ping_Message : Message
{
  Ping_Message();
  Ping_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);
};

struct Ack_Message : Message
{
  Ack_Message();
  Ack_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);
};

std::unique_ptr<Message> deserialize_message(Buffer &b);
