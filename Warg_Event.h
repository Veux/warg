#pragma once

#define SIM_LATENCY 100

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
  Spawn_Request,
  Input,
  State,
  Cast
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
  Input_Message(uint32_t i_, Move_Status move_status, quat orientation, UID target_id);
  Input_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state) { ASSERT(false); };
  virtual void serialize(Buffer &b);

  uint32_t input_number;
  Move_Status move_status;
  quat orientation;
  UID target_id;
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

struct Input;

struct State_Message : Message
{
  State_Message(UID pc, std::map<UID, Character> &chars, std::map<UID, Spell_Object> spell_objects, uint32 tick_, uint32 input_number_);
  State_Message(Buffer &b);
  virtual void handle(Warg_Server &server) { ASSERT(false); };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID pc;
  uint32 tick;
  uint32 input_number;
  std::map<UID, Character> characters;
  std::map<UID, Spell_Object> spell_objects;
};

std::unique_ptr<Message> deserialize_message(Buffer &b);
