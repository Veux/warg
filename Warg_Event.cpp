#include "Warg_Event.h"
#include "Warg_Common.h"

void Buffer::reserve(size_t size) { data.reserve(wnext + size); }

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

void serialize_(Buffer &b, uint8_t n) { b.insert(&n, sizeof(n)); }

void serialize_(Buffer &b, uint16_t n) { b.insert(&n, sizeof(n)); }

void serialize_(Buffer &b, int32_t n) { b.insert(&n, sizeof(n)); }

void serialize_(Buffer &b, uint32_t n) { b.insert(&n, sizeof(n)); }

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

void serialize_(Buffer &b, std::string s) { serialize_(b, s.c_str()); }

void serialize_(Buffer &b, float32_t a) { b.insert(&a, sizeof(a)); }

void serialize_(Buffer &b, vec3 v)
{
  serialize_(b, v.x);
  serialize_(b, v.y);
  serialize_(b, v.z);
}

void serialize_(Buffer &b, Warg_Event_Type type) { serialize_(b, (uint8_t)type); }

void Char_Spawn_Request_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::CharSpawnRequest);
  serialize_(b, name);
  serialize_(b, team);
}

void Input_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::PlayerMovement);
  serialize_(b, input_number);
  serialize_(b, (uint8_t)move_status);
  serialize_(b, dir);
}

void Cast_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::Cast);
  serialize_(b, target);
  serialize_(b, spell);
}

void Cast_Error_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::CastError);
  serialize_(b, caster);
  serialize_(b, target);
  serialize_(b, spell);
  serialize_(b, err);
}

void Cast_Begin_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::CastBegin);
  serialize_(b, caster);
  serialize_(b, target);
  serialize_(b, spell);
}

void Cast_Interrupt_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::CastInterrupt);
  serialize_(b, caster);
}

void Char_HP_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::CharHP);
  serialize_(b, character);
  serialize_(b, hp);
}

void Buff_Application_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::BuffAppl);
  serialize_(b, character);
  serialize_(b, buff);
}

void Object_Launch_Message::serialize(Buffer &b)
{
  serialize_(b, Warg_Event_Type::ObjectLaunch);
  serialize_(b, object);
  serialize_(b, caster);
  serialize_(b, target);
  serialize_(b, pos);
}

void State_Message::serialize(Buffer &b)
{
  // sponge
}

void Ping_Message::serialize(Buffer &b) { serialize_(b, Warg_Event_Type::Ping); }

void Ack_Message::serialize(Buffer &b) { serialize_(b, Warg_Event_Type::Ack); }

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

UID deserialize_uid(Buffer &b) { return deserialize_uint32(b); }

Char_Spawn_Request_Message::Char_Spawn_Request_Message(Buffer &b)
{
  name = deserialize_string(b);
  team = deserialize_uint8(b);
}

Input_Message::Input_Message(Buffer &b)
{
  input_number = deserialize_uint32(b);
  move_status = (Move_Status)deserialize_uint8(b);
  dir = deserialize_vec3(b);
}

State_Message::State_Message(Buffer &b)
{
  // sponge
}

Cast_Message::Cast_Message(Buffer &b)
{
  target = deserialize_uid(b);
  spell = deserialize_string(b);
}

Cast_Error_Message::Cast_Error_Message(Buffer &b)
{
  caster = deserialize_uid(b);
  target = deserialize_uid(b);
  spell = deserialize_string(b);
  err = deserialize_uint8(b);
}

Cast_Begin_Message::Cast_Begin_Message(Buffer &b)
{
  caster = deserialize_uid(b);
  target = deserialize_uid(b);
  spell = deserialize_string(b);
}

Cast_Interrupt_Message::Cast_Interrupt_Message(Buffer &b) { caster = deserialize_uid(b); }

Char_HP_Message::Char_HP_Message(Buffer &b)
{
  character = deserialize_uid(b);
  hp = deserialize_int32(b);
}

Buff_Application_Message::Buff_Application_Message(Buffer &b)
{
  character = deserialize_uid(b);
  buff = deserialize_string(b);
}

Object_Launch_Message::Object_Launch_Message(Buffer &b)
{
  object = deserialize_uid(b);
  caster = deserialize_uid(b);
  target = deserialize_uid(b);
  pos = deserialize_vec3(b);
}

Ping_Message::Ping_Message(Buffer &b) {}

Ack_Message::Ack_Message(Buffer &b) {}

std::unique_ptr<Message> deserialize_message(Buffer &b)
{
  auto type = (Warg_Event_Type)deserialize_uint8(b);

  std::unique_ptr<Message> msg = nullptr;
  switch (type)
  {
    case Warg_Event_Type::CharSpawnRequest:
      msg = std::make_unique<Char_Spawn_Request_Message>(b);
      break;
    case Warg_Event_Type::PlayerMovement:
      msg = std::make_unique<Input_Message>(b);
      break;
    case Warg_Event_Type::PlayerGeometry:
      msg = std::make_unique<State_Message>(b);
      break;
    case Warg_Event_Type::Cast:
      msg = std::make_unique<Cast_Message>(b);
      break;
    case Warg_Event_Type::CastError:
      msg = std::make_unique<Cast_Error_Message>(b);
      break;
    case Warg_Event_Type::CastBegin:
      msg = std::make_unique<Cast_Begin_Message>(b);
      break;
    case Warg_Event_Type::CastInterrupt:
      msg = std::make_unique<Cast_Interrupt_Message>(b);
      break;
    case Warg_Event_Type::CharHP:
      msg = std::make_unique<Char_HP_Message>(b);
      break;
    case Warg_Event_Type::BuffAppl:
      msg = std::make_unique<Buff_Application_Message>(b);
      break;
    case Warg_Event_Type::ObjectLaunch:
      msg = std::make_unique<Object_Launch_Message>(b);
      break;
    case Warg_Event_Type::Ping:
      msg = std::make_unique<Ping_Message>(b);
      break;
    case Warg_Event_Type::Ack:
      msg = std::make_unique<Ack_Message>(b);
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

Input_Message::Input_Message(uint32_t i_, Move_Status move_status_, vec3 dir_)
{
  reliable = false;
  input_number = i_;
  move_status = move_status_;
  dir = dir_;
}

Cast_Message::Cast_Message(UID target_, const char *spell_)
{
  target = target_;
  spell = spell_;
}

Cast_Error_Message::Cast_Error_Message(UID caster_, UID target_, const char *spell_, uint8_t err_)
{
  caster = caster_;
  target = target_;
  spell = spell_;
  err = err_;
}

Cast_Begin_Message::Cast_Begin_Message(UID caster_, UID target_, const char *spell_)
{
  caster = caster_;
  target = target_;
  spell = spell_;
}

Cast_Interrupt_Message::Cast_Interrupt_Message(UID caster_) { caster = caster_; }

Char_HP_Message::Char_HP_Message(UID character_, int hp_)
{
  character = character_;
  hp = hp_;
}

Buff_Application_Message::Buff_Application_Message(UID character_, const char *buff_)
{
  character = character_;
  buff = buff_;
}

Object_Launch_Message::Object_Launch_Message(UID object_, UID caster_, UID target_, vec3 pos_)
{
  object = object_;
  caster = caster_;
  target = target_;
  pos = pos_;
}

State_Message::State_Message(UID pc_, std::map<UID, Character> &chars_, uint32 tick_, uint32 input_number_)
{
  reliable = false;

  tick = tick_;
  input_number = input_number_;
  pc = pc_;
  characters = chars_;
}

Ping_Message::Ping_Message() {}

Ack_Message::Ack_Message() {}
