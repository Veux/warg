#pragma once

#define SIM_LATENCY 100

#include "Spell.h"
#include "Warg_Common.h"
#include <enet/enet.h>
#include <map>

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
  virtual void handle(Warg_State &state)
  {
    ASSERT(false);
  };
  virtual void serialize(Buffer &b);

  std::string name;
  uint8_t team;
};

struct Input_Message : Message
{
  Input_Message(uint32_t i_, Move_Status move_status, quat orientation, UID target_id);
  Input_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state)
  {
    ASSERT(false);
  };
  virtual void serialize(Buffer &b);

  uint32_t input_number;
  Move_Status move_status;
  quat orientation;
  UID target_id;
};

struct Cast_Message : Message
{
  Cast_Message(UID target_id, Spell_Index spell_index);
  Cast_Message(Buffer &b);
  virtual void handle(Warg_Server &server);
  virtual void handle(Warg_State &state)
  {
    ASSERT(false);
  };
  virtual void serialize(Buffer &b);

  UID _target_id;
  Spell_Index _spell_index;
};

struct Input;

struct State_Message : Message
{
  State_Message(UID pc, Game_State *game_state_);
  State_Message(Buffer &b);
  virtual void handle(Warg_Server &server)
  {
    ASSERT(false);
  };
  virtual void handle(Warg_State &state);
  virtual void serialize(Buffer &b);

  UID pc;
  Game_State game_state;
};

std::unique_ptr<Message> deserialize_message(Buffer &b);
