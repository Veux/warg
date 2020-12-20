#include "stdafx.h"
#include "Warg_Event.h"
#include "Warg_Common.h"

void Buffer::reserve(size_t size)
{
  data.reserve(wnext + size);
}

void Buffer::insert(void *d, size_t size)
{
  data.resize(wnext + size);
  memcpy(&data[wnext], d, size);
  wnext += size;
}

void *Buffer::read(size_t size)
{
  ASSERT(rnext + size <= data.size());
  uint8_t *p = &data[rnext];
  rnext += size;
  return p;
}

void serialize_(Buffer &b, uint8_t n)
{
  b.insert(&n, sizeof(n));
}

void serialize_(Buffer &b, uint16_t n)
{
  b.insert(&n, sizeof(n));
}

void serialize_(Buffer &b, int32_t n)
{
  b.insert(&n, sizeof(n));
}

void serialize_(Buffer &b, uint32_t n)
{
  b.insert(&n, sizeof(n));
}

void serialize_(Buffer &b, const char *s)
{
  ASSERT(s);
  size_t len = strlen(s);
  ASSERT(len <= UINT16_MAX);
  uint16 len_ = (uint16_t)len;
  b.reserve(sizeof(len) + len);
  b.insert(&len, sizeof(len));
  b.insert((void *)s, len);
}

void serialize_(Buffer &b, std::string s)
{
  serialize_(b, s.c_str());
}

void serialize_(Buffer &b, float32_t a)
{
  b.insert(&a, sizeof(a));
}

void serialize_(Buffer &b, vec3 v)
{
  serialize_(b, v.x);
  serialize_(b, v.y);
  serialize_(b, v.z);
}

void serialize_(Buffer &b, Warg_Event_Type type)
{
  serialize_(b, (uint8_t)type);
}

void Char_Spawn_Request_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::Spawn_Request);
  serialize_(b, name);
  serialize_(b, team);
}

void Input_Message::serialize(Buffer &b)
{
  // serialize_(b, Warg_Event_Type::PlayerMovement);
  // serialize_(b, input_number);
  // serialize_(b, (uint8_t)move_status);
  // serialize_(b, dir);
}

void Cast_Message::serialize(Buffer &b)
{
  //serialize_(b, Warg_Event_Type::Cast);
  //serialize_(b, target);
  //serialize_(b, spell);
}

void State_Message::serialize(Buffer &b)
{
  // sponge
}

uint8_t deserialize_uint8(Buffer &b)
{
  uint8_t n = *(uint8_t *)b.read(sizeof(n));
  return n;
}

uint16_t deserialize_uint16(Buffer &b)
{
  uint16_t n = *(uint16_t *)b.read(sizeof(n));
  return n;
}

int32_t deserialize_int32(Buffer &b)
{
  int32_t n = *(int32_t *)b.read(sizeof(n));
  return n;
}

uint32_t deserialize_uint32(Buffer &b)
{
  int n = *(uint32_t *)b.read(sizeof(n));
  return n;
}

std::string deserialize_string(Buffer &b)
{
  uint16_t len = deserialize_uint16(b);
  return std::string((char *)b.read(len), len);
}

float32_t deserialize_float32(Buffer &b)
{
  float32_t a = *(float32_t *)b.read(sizeof(float32));
  return a;
}

vec3 deserialize_vec3(Buffer &b)
{
  float32_t x = deserialize_float32(b);
  float32_t y = deserialize_float32(b);
  float32_t z = deserialize_float32(b);

  vec3 v = vec3(x, y, z);

  return v;
}

UID deserialize_uid(Buffer &b)
{
  return deserialize_uint32(b);
}

Char_Spawn_Request_Message::Char_Spawn_Request_Message(Buffer &b)
{
  name = deserialize_string(b);
  team = deserialize_uint8(b);
}

Input_Message::Input_Message(Buffer &b)
{
  // input_number = deserialize_uint32(b);
  // move_status = (Move_Status)deserialize_uint8(b);
  // dir = deserialize_vec3(b);
}

State_Message::State_Message(Buffer &b)
{
  // sponge
}

Cast_Message::Cast_Message(Buffer &b)
{
  //target = deserialize_uid(b);
  //spell = deserialize_string(b);
}

std::unique_ptr<Message> deserialize_message(Buffer &b)
{
  auto type = (Warg_Event_Type)deserialize_uint8(b);

  std::unique_ptr<Message> msg = nullptr;
  switch (type)
  {
    case Warg_Event_Type::Spawn_Request:
      msg = std::make_unique<Char_Spawn_Request_Message>(b);
      break;
    case Warg_Event_Type::Input:
      msg = std::make_unique<Input_Message>(b);
      break;
    case Warg_Event_Type::State:
      msg = std::make_unique<State_Message>(b);
      break;
    case Warg_Event_Type::Cast:
      msg = std::make_unique<Cast_Message>(b);
      break;
    default:
      ASSERT(false);
  }

  return msg;
}

Char_Spawn_Request_Message::Char_Spawn_Request_Message(const char *name_, uint8_t team_)
{
  name = name_;
  team = team_;
}

Input_Message::Input_Message(uint32_t i_, Move_Status move_status_, quat orientation_, UID target_id_)
{
  reliable = false;
  input_number = i_;
  move_status = move_status_;
  orientation = orientation_;
  target_id = target_id_;
}

Cast_Message::Cast_Message(UID target_id, UID spell_id)
{
  _target_id = target_id;
  _spell_id = spell_id;
}

State_Message::State_Message(UID pc_, Game_State *game_state_)
{
  reliable = false;

  pc = pc_;
  game_state = *game_state_;
}
