#include "Warg_Event.h"

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

void serialize(Buffer &b, uint8_t n) { b.insert(&n, sizeof(n)); }

void serialize(Buffer &b, uint16_t n) { b.insert(&n, sizeof(n)); }

void serialize(Buffer &b, int32_t n) { b.insert(&n, sizeof(n)); }

void serialize(Buffer &b, uint32_t n) { b.insert(&n, sizeof(n)); }

void serialize(Buffer &b, char *s)
{
  ASSERT(s);
  uint16_t len = strlen(s);
  ASSERT(len <= UINT16_MAX);
  b.reserve(sizeof(len) + len);
  b.insert(&len, sizeof(len));
  b.insert(s, len);
}

void serialize(Buffer &b, std::string s)
{
  serialize(b, s.c_str());
}

void serialize(Buffer &b, float32_t a) { b.insert(&a, sizeof(a)); }

void serialize(Buffer &b, vec3 v)
{
  serialize(b, v.x);
  serialize(b, v.y);
  serialize(b, v.z);
}

void serialize(Buffer &b, Warg_Event_Type type)
{
  serialize(b, (uint8_t)type);
}

void Char_Spawn_Request_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::CharSpawnRequest);
  serialize(b, name);
  serialize(b, team);
}

void Char_Spawn_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::CharSpawn);
  serialize(b, id);
  serialize(b, name);
  serialize(b, team);
}

void Player_Control_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::PlayerControl);
  serialize(b, character);
}

void Player_Movement_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::PlayerMovement);
  serialize(b, (uint8_t)move_status);
  serialize(b, dir);
}

void Jump_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::Jump);
}

void Char_Pos_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::CharPos);
  serialize(b, character);
  serialize(b, dir);
  serialize(b, pos);
}

void Cast_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::Cast);
  serialize(b, target);
  serialize(b, spell);
}

void Cast_Error_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::CastError);
  serialize(b, caster);
  serialize(b, target);
  serialize(b, spell);
  serialize(b, err);
}

void Cast_Begin_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::CastBegin);
  serialize(b, caster);
  serialize(b, target);
  serialize(b, spell);
}

void Cast_Interrupt_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::CastInterrupt);
  serialize(b, caster);
}

void Char_HP_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::CharHP);
  serialize(b, character);
  serialize(b, hp);
}

void Buff_Application_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::BuffAppl);
  serialize(b, character);
  serialize(b, buff);
}

void Object_Launch_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::ObjectLaunch);
  serialize(b, object);
  serialize(b, caster);
  serialize(b, target);
  serialize(b, pos);
}

void Object_Destroy_Message::serialize_(Buffer &b)
{
  serialize(b, Warg_Event_Type::ObjectDestroy);
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

char *deserialize_string(Buffer &b)
{
  uint16_t len = deserialize_uint16(b);
  char *s = (char *)malloc(len + 1);
  memcpy(s, b.read(len), len);
  s[len] = '\0';
  return s;
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

Char_Spawn_Message::Char_Spawn_Message(Buffer &b)
{
  id = deserialize_uid(b);
  name = deserialize_string(b);
  team = deserialize_uint8(b);
}

Player_Control_Message::Player_Control_Message(Buffer &b)
{
  character = deserialize_uid(b);
}

Player_Movement_Message::Player_Movement_Message(Buffer &b)
{
  move_status = (Move_Status)deserialize_uint8(b);
  dir = deserialize_vec3(b);
}

Jump_Message::Jump_Message(Buffer &b)
{
}

Char_Pos_Message::Char_Pos_Message(Buffer &b)
{
  character = deserialize_uid(b);
  dir = deserialize_vec3(b);
  pos = deserialize_vec3(b);
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

Cast_Interrupt_Message::Cast_Interrupt_Message(Buffer &b)
{
  caster = deserialize_uid(b);
}

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

Object_Destroy_Message::Object_Destroy_Message(Buffer &b)
{
}

std::unique_ptr<Message> deserialize_message(Buffer &b)
{
  auto type = (Warg_Event_Type)deserialize_uint8(b);
  switch (type)
  {
    case Warg_Event_Type::CharSpawnRequest:
      return std::make_unique<Char_Spawn_Request_Message>(b);
    case Warg_Event_Type::CharSpawn:
      return std::make_unique<Char_Spawn_Message>(b);
    case Warg_Event_Type::PlayerControl:
      return std::make_unique<Player_Control_Message>(b);
    case Warg_Event_Type::PlayerMovement:
      return std::make_unique<Player_Movement_Message>(b);
    case Warg_Event_Type::Jump:
      return std::make_unique<Jump_Message>(b);
    case Warg_Event_Type::CharPos:
      return std::make_unique<Char_Pos_Message>(b);
    case Warg_Event_Type::Cast:
      return std::make_unique<Cast_Message>(b);
    case Warg_Event_Type::CastError:
      return std::make_unique<Cast_Error_Message>(b);
    case Warg_Event_Type::CastBegin:
      return std::make_unique<Cast_Begin_Message>(b);
    case Warg_Event_Type::CastInterrupt:
      return std::make_unique<Cast_Interrupt_Message>(b);
    case Warg_Event_Type::CharHP:
      return std::make_unique<Char_HP_Message>(b);
    case Warg_Event_Type::BuffAppl:
      return std::make_unique<Buff_Application_Message>(b);
    case Warg_Event_Type::ObjectLaunch:
      return std::make_unique<Object_Launch_Message>(b);
    case Warg_Event_Type::ObjectDestroy:
      return std::make_unique<Object_Destroy_Message>(b);
  }
  ASSERT(false);
  return nullptr;
}

Char_Spawn_Request_Message::Char_Spawn_Request_Message(
  const char *name_, uint8_t team_)
{
  t = get_real_time();
  name = name_;
  team = team_;
}

Char_Spawn_Message::Char_Spawn_Message(UID id_, const char *name_, uint8_t team_)
{
  t = get_real_time();
  id = id_;
  name = name_;
  team = team_;
}

Player_Control_Message::Player_Control_Message(UID character_)
{
  t = get_real_time();
  character = character_;
}

Player_Movement_Message::Player_Movement_Message
  (Move_Status move_status_, vec3 dir_)
{
  t = get_real_time();
  reliable = false;
  move_status = move_status_;
  dir = dir_;
}

Jump_Message::Jump_Message()
{
  t = get_real_time();
}

Char_Pos_Message::Char_Pos_Message(UID character_, vec3 dir_, vec3 pos_)
{
  t = get_real_time();
  character = character_;
  dir = dir_;
  pos = pos_;
}

Cast_Message::Cast_Message(UID target_, const char *spell_)
{
  t = get_real_time();
  target = target_;
  spell = spell_;
}

Cast_Error_Message::Cast_Error_Message
  (UID caster_, UID target_, const char *spell_, uint8_t err_)
{
  t = get_real_time();
  caster = caster_;
  target = target_;
  spell = spell_;
  err = err_;
}

Cast_Begin_Message::Cast_Begin_Message
  (UID caster_, UID target_, const char *spell_)
{
  t = get_real_time();
  caster = caster_;
  target = target_;
  spell = spell_;
}

Cast_Interrupt_Message::Cast_Interrupt_Message(UID caster_)
{
  t = get_real_time();
  caster = caster_;
}

Char_HP_Message::Char_HP_Message(UID character_, int hp_)
{
  t = get_real_time();
  character = character_;
  hp = hp_;
}

Buff_Application_Message::Buff_Application_Message
  (UID character_, const char *buff_)
{
  t = get_real_time();
  character = character_;
  buff = buff_;
}

Object_Launch_Message::Object_Launch_Message
  (UID object_, UID caster_, UID target_, vec3 pos_)
{
  t = get_real_time();
  object = object_;
  caster = caster_;
  target = target_;
  pos = pos_;
}

Object_Destroy_Message::Object_Destroy_Message()
{
  t = get_real_time();
}
