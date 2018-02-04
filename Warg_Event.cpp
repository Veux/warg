#include "Warg_Event.h"

Warg_Event char_spawn_request_event(const char *name, uint8_t team)
{
  ASSERT(name);

  CharSpawnRequest_Event *spawn =
      (CharSpawnRequest_Event *)malloc(sizeof *spawn);
  spawn->name = (char *)malloc(strlen(name) + 1);
  strcpy(spawn->name, name);
  spawn->team = team;

  Warg_Event ev;
  ev.type = Warg_Event_Type::CharSpawnRequest;
  ev.event = spawn;
  ev.t = get_real_time();

  return ev;
}

Warg_Event char_spawn_event(UID id, const char *name, uint8_t team)
{
  ASSERT(name);

  CharSpawn_Event *spawn = (CharSpawn_Event *)malloc(sizeof *spawn);
  spawn->id = id;
  spawn->name = (char *)malloc(strlen(name) + 1);
  strcpy(spawn->name, name);
  spawn->team = team;

  Warg_Event ev;
  ev.type = Warg_Event_Type::CharSpawn;
  ev.event = spawn;
  ev.t = get_real_time();

  return ev;
}

Warg_Event player_control_event(UID character)
{
  PlayerControl_Event *pc = (PlayerControl_Event *)malloc(sizeof *pc);
  pc->character = character;

  Warg_Event ev;
  ev.type = Warg_Event_Type::PlayerControl;
  ev.event = pc;
  ev.t = get_real_time();

  return ev;
}

Warg_Event dir_event(uint8_t ch, vec3 d)
{
  Dir_Event *dir;
  dir = (Dir_Event *)malloc(sizeof *dir);
  dir->character = ch;
  dir->dir = d;

  Warg_Event ev;
  ev.type = Warg_Event_Type::Dir;
  ev.event = dir;
  ev.t = get_real_time();

  return ev;
}

Warg_Event move_event(uint8_t ch, Move_Status m)
{
  Move_Event *mv;
  mv = (Move_Event *)malloc(sizeof *mv);
  mv->character = ch;
  mv->m = m;

  Warg_Event ev;
  ev.type = Warg_Event_Type::Move;
  ev.event = mv;
  ev.t = get_real_time();

  return ev;
}

Warg_Event jump_event(uint8_t ch)
{
  Jump_Event *jump;
  jump = (Jump_Event *)malloc(sizeof *jump);
  jump->character = ch;

  Warg_Event ev;
  ev.type = Warg_Event_Type::Jump;
  ev.event = jump;
  ev.t = get_real_time();

  return ev;
}

Warg_Event char_pos_event(uint8_t ch, vec3 dir, vec3 pos)
{
  CharPos_Event *posev = (CharPos_Event *)malloc(sizeof *posev);
  posev->character = ch;
  posev->dir = dir;
  posev->pos = pos;

  Warg_Event ev;
  ev.type = Warg_Event_Type::CharPos;
  ev.event = posev;
  ev.t = get_real_time();

  return ev;
}

Warg_Event cast_event(uint8_t caster, uint8_t target, const char *spell)
{
  ASSERT(spell);

  Cast_Event *cast = (Cast_Event *)malloc(sizeof *cast);
  cast->character = caster;
  cast->target = target;
  cast->spell = (char *)malloc(strlen(spell) + 1);
  strcpy(cast->spell, spell);

  Warg_Event ev;
  ev.type = Warg_Event_Type::Cast;
  ev.event = cast;
  ev.t = get_real_time();

  return ev;
}

Warg_Event cast_error_event(
    uint8_t caster, uint8_t target, const char *spell, uint8_t err)
{
  ASSERT(spell);

  CastError_Event *errev = (CastError_Event *)malloc(sizeof *errev);
  errev->caster = caster;
  errev->target = target;
  errev->spell = (char *)malloc(strlen(spell) + 1);
  strcpy(errev->spell, spell);
  errev->err = err;

  Warg_Event ev;
  ev.type = Warg_Event_Type::CastError;
  ev.event = errev;
  ev.t = get_real_time();

  return ev;
}

Warg_Event cast_begin_event(uint8_t caster, uint8_t target, const char *spell)
{
  ASSERT(spell);

  CastBegin_Event *begin = (CastBegin_Event *)malloc(sizeof *begin);
  begin->caster = caster;
  begin->target = target;
  begin->spell = (char *)malloc(strlen(spell) + 1);
  strcpy(begin->spell, spell);

  Warg_Event ev;
  ev.type = Warg_Event_Type::CastBegin;
  ev.event = begin;
  ev.t = get_real_time();

  return ev;
}

Warg_Event cast_interrupt_event(uint8_t caster)
{
  CastInterrupt_Event *interrupt =
      (CastInterrupt_Event *)malloc(sizeof *interrupt);

  interrupt->caster = caster;

  Warg_Event ev;
  ev.type = Warg_Event_Type::CastInterrupt;
  ev.event = interrupt;
  ev.t = get_real_time();

  return ev;
}

Warg_Event char_hp_event(uint8_t character, int hp)
{
  CharHP_Event *chhp = (CharHP_Event *)malloc(sizeof *chhp);
  chhp->character = character;
  chhp->hp = hp;

  Warg_Event ev;
  ev.type = Warg_Event_Type::CharHP;
  ev.event = chhp;
  ev.t = get_real_time();

  return ev;
}

Warg_Event buff_appl_event(uint8_t character, const char *buff)
{
  ASSERT(buff);

  BuffAppl_Event *appl = (BuffAppl_Event *)malloc(sizeof *appl);
  appl->character = character;
  appl->buff = (char *)malloc(strlen(buff) + 1);
  strcpy(appl->buff, buff);

  Warg_Event ev;
  ev.type = Warg_Event_Type::BuffAppl;
  ev.event = appl;
  ev.t = get_real_time();

  return ev;
}

Warg_Event object_launch_event(
    int object, uint8_t caster, uint8_t target, vec3 pos)
{
  ObjectLaunch_Event *launch = (ObjectLaunch_Event *)malloc(sizeof *launch);
  launch->object = object;
  launch->caster = caster;
  launch->target = target;
  launch->pos = pos;

  Warg_Event ev;
  ev.type = Warg_Event_Type::ObjectLaunch;
  ev.event = launch;
  ev.t = get_real_time();

  return ev;
}

void free_warg_event(CharSpawnRequest_Event *ev) { free(ev->name); }

void free_warg_event(CharSpawn_Event *ev) { free(ev->name); }

void free_warg_event(PlayerControl_Event *ev) {}

void free_warg_event(Dir_Event *dir) {}

void free_warg_event(Move_Event *ev) {}

void free_warg_event(Jump_Event *jump) {}

void free_warg_event(CharPos_Event *ev) {}

void free_warg_event(Cast_Event *ev) { free(ev->spell); }

void free_warg_event(CastError_Event *ev) { free(ev->spell); }

void free_warg_event(CastBegin_Event *ev) { free(ev->spell); }

void free_warg_event(CastInterrupt_Event *ev) {}

void free_warg_event(CharHP_Event *ev) {}

void free_warg_event(BuffAppl_Event *ev) { free(ev->buff); }

void free_warg_event(ObjectLaunch_Event *ev) {}

void free_warg_event(Warg_Event ev)
{
  ASSERT(ev.event);

  switch (ev.type)
  {
    case Warg_Event_Type::CharSpawnRequest:
      free_warg_event((CharSpawnRequest_Event *)ev.event);
      break;
    case Warg_Event_Type::CharSpawn:
      free_warg_event((CharSpawn_Event *)ev.event);
      break;
    case Warg_Event_Type::PlayerControl:
      free_warg_event((PlayerControl_Event *)ev.event);
      break;
    case Warg_Event_Type::Dir:
      free_warg_event((Dir_Event *)ev.event);
      break;
    case Warg_Event_Type::Jump:
      free_warg_event((Jump_Event *)ev.event);
      break;
    case Warg_Event_Type::Move:
      free_warg_event((Move_Event *)ev.event);
      break;
    case Warg_Event_Type::CharPos:
      free_warg_event((CharPos_Event *)ev.event);
      break;
    case Warg_Event_Type::Cast:
      free_warg_event((Cast_Event *)ev.event);
      break;
    case Warg_Event_Type::CastError:
      free_warg_event((CastError_Event *)ev.event);
      break;
    case Warg_Event_Type::CastBegin:
      free_warg_event((CastBegin_Event *)ev.event);
      break;
    case Warg_Event_Type::CastInterrupt:
      free_warg_event((CastInterrupt_Event *)ev.event);
      break;
    case Warg_Event_Type::CharHP:
      free_warg_event((CharHP_Event *)ev.event);
      break;
    case Warg_Event_Type::BuffAppl:
      free_warg_event((BuffAppl_Event *)ev.event);
      break;
    case Warg_Event_Type::ObjectLaunch:
      free_warg_event((ObjectLaunch_Event *)ev.event);
      break;
    default:
      ASSERT(false);
  }

  free(ev.event);
}

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

void serialize(Buffer &b, float32_t a) { b.insert(&a, sizeof(a)); }

void serialize(Buffer &b, vec3 v)
{
  serialize(b, v.x);
  serialize(b, v.y);
  serialize(b, v.z);
}

void serialize(Buffer &b, CharSpawnRequest_Event ev)
{
  serialize(b, ev.name);
  serialize(b, ev.team);
}

void serialize(Buffer &b, CharSpawn_Event ev)
{
  serialize(b, ev.id);
  serialize(b, ev.name);
  serialize(b, ev.team);
}

void serialize(Buffer &b, PlayerControl_Event ev)
{
  serialize(b, ev.character);
}

void serialize(Buffer &b, Move_Event ev)
{
  serialize(b, ev.character);
  serialize(b, (uint8_t)ev.m);
}

void serialize(Buffer &b, Dir_Event ev)
{
  serialize(b, ev.character);
  serialize(b, ev.dir);
}

void serialize(Buffer &b, Jump_Event ev) { serialize(b, ev.character); }

void serialize(Buffer &b, CharPos_Event ev)
{
  serialize(b, ev.character);
  serialize(b, ev.dir);
  serialize(b, ev.pos);
}

void serialize(Buffer &b, Cast_Event ev)
{
  serialize(b, ev.character);
  serialize(b, ev.target);
  serialize(b, ev.spell);
}

void serialize(Buffer &b, CastError_Event ev)
{
  serialize(b, ev.caster);
  serialize(b, ev.target);
  serialize(b, ev.spell);
  serialize(b, ev.err);
}

void serialize(Buffer &b, CastBegin_Event ev)
{
  serialize(b, ev.caster);
  serialize(b, ev.target);
  serialize(b, ev.spell);
}

void serialize(Buffer &b, CastInterrupt_Event ev) { serialize(b, ev.caster); }

void serialize(Buffer &b, CharHP_Event ev)
{
  serialize(b, ev.character);
  serialize(b, ev.hp);
}

void serialize(Buffer &b, BuffAppl_Event ev)
{
  serialize(b, ev.character);
  serialize(b, ev.buff);
}

void serialize(Buffer &b, ObjectLaunch_Event ev)
{
  serialize(b, ev.object);
  serialize(b, ev.caster);
  serialize(b, ev.target);
  serialize(b, ev.pos);
}

void serialize(Buffer &b, ObjectDestroy_Event ev) {}

void serialize(Buffer &b, Warg_Event ev)
{
  serialize(b, (uint8_t)ev.type);
  switch (ev.type)
  {
    case Warg_Event_Type::CharSpawnRequest:
      serialize(b, *(CharSpawnRequest_Event *)ev.event);
      break;
    case Warg_Event_Type::CharSpawn:
      serialize(b, *(CharSpawn_Event *)ev.event);
      break;
    case Warg_Event_Type::PlayerControl:
      serialize(b, *(PlayerControl_Event *)ev.event);
      break;
    case Warg_Event_Type::Move:
      serialize(b, *(Move_Event *)ev.event);
      break;
    case Warg_Event_Type::Dir:
      serialize(b, *(Dir_Event *)ev.event);
      break;
    case Warg_Event_Type::Jump:
      serialize(b, *(Jump_Event *)ev.event);
      break;
    case Warg_Event_Type::CharPos:
      serialize(b, *(CharPos_Event *)ev.event);
      break;
    case Warg_Event_Type::Cast:
      serialize(b, *(Cast_Event *)ev.event);
      break;
    case Warg_Event_Type::CastError:
      serialize(b, *(CastError_Event *)ev.event);
      break;
    case Warg_Event_Type::CastBegin:
      serialize(b, *(CastBegin_Event *)ev.event);
      break;
    case Warg_Event_Type::CastInterrupt:
      serialize(b, *(CastInterrupt_Event *)ev.event);
      break;
    case Warg_Event_Type::CharHP:
      serialize(b, *(CharHP_Event *)ev.event);
      break;
    case Warg_Event_Type::BuffAppl:
      serialize(b, *(BuffAppl_Event *)ev.event);
      break;
    case Warg_Event_Type::ObjectLaunch:
      serialize(b, *(ObjectLaunch_Event *)ev.event);
      break;
    case Warg_Event_Type::ObjectDestroy:
      serialize(b, *(ObjectDestroy_Event *)ev.event);
      break;
    default:
      ASSERT(false);
      break;
  }
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

CharSpawnRequest_Event deserialize_char_spawn_request(Buffer &b)
{
  CharSpawnRequest_Event ev;
  ev.name = deserialize_string(b);
  ev.team = deserialize_uint8(b);

  return ev;
}

CharSpawn_Event deserialize_char_spawn(Buffer &b)
{
  CharSpawn_Event ev;
  ev.id = deserialize_uint32(b);
  ev.name = deserialize_string(b);
  ev.team = deserialize_uint8(b);

  return ev;
}

PlayerControl_Event deserialize_player_control(Buffer &b)
{
  PlayerControl_Event ev;
  ev.character = deserialize_int32(b);

  return ev;
}

Dir_Event deserialize_dir(Buffer &b)
{
  Dir_Event ev;
  ev.character = deserialize_uint8(b);
  ev.dir = deserialize_vec3(b);

  return ev;
}

Move_Event deserialize_move(Buffer &b)
{
  Move_Event ev;
  ev.character = deserialize_uint8(b);
  ev.m = (Move_Status)deserialize_uint8(b);

  return ev;
}

Jump_Event deserialize_jump(Buffer &b)
{
  Jump_Event ev;
  ev.character = deserialize_uint8(b);

  return ev;
}

CharPos_Event deserialize_char_pos(Buffer &b)
{
  CharPos_Event ev;
  ev.character = deserialize_uint8(b);
  ev.dir = deserialize_vec3(b);
  ev.pos = deserialize_vec3(b);

  return ev;
}

Cast_Event deserialize_cast(Buffer &b)
{
  Cast_Event ev;
  ev.character = deserialize_uint8(b);
  ev.target = deserialize_uint8(b);
  ev.spell = deserialize_string(b);

  return ev;
}

CastError_Event deserialize_cast_error(Buffer &b)
{
  CastError_Event ev;
  ev.caster = deserialize_uint8(b);
  ev.target = deserialize_uint8(b);
  ev.spell = deserialize_string(b);
  ev.err = deserialize_uint8(b);

  return ev;
}

CastBegin_Event deserialize_cast_begin(Buffer &b)
{
  CastBegin_Event ev;
  ev.caster = deserialize_uint8(b);
  ev.target = deserialize_uint8(b);
  ev.spell = deserialize_string(b);

  return ev;
}

CastInterrupt_Event deserialize_cast_interrupt(Buffer &b)
{
  CastInterrupt_Event ev;
  ev.caster = deserialize_uint8(b);

  return ev;
}

CharHP_Event deserialize_char_hp(Buffer &b)
{
  CharHP_Event ev;
  ev.character = deserialize_uint8(b);
  ev.hp = deserialize_int32(b);

  return ev;
}

BuffAppl_Event deserialize_buff_appl(Buffer &b)
{
  BuffAppl_Event ev;
  ev.character = deserialize_uint8(b);
  ev.buff = deserialize_string(b);

  return ev;
}

ObjectLaunch_Event deserialize_object_launch(Buffer &b)
{
  ObjectLaunch_Event ev;
  ev.object = deserialize_int32(b);
  ev.caster = deserialize_uint8(b);
  ev.target = deserialize_uint8(b);
  ev.pos = deserialize_vec3(b);

  return ev;
}

ObjectDestroy_Event deserialize_object_destroy(Buffer &b)
{
  ObjectDestroy_Event ev;

  return ev;
}

Warg_Event deserialize_event(Buffer &b)
{
  Warg_Event ev;

  ev.type = (Warg_Event_Type)deserialize_uint8(b);
  switch (ev.type)
  {
    case Warg_Event_Type::CharSpawnRequest:
      ev.event = malloc(sizeof(CharSpawnRequest_Event));
      *(CharSpawnRequest_Event *)ev.event = deserialize_char_spawn_request(b);
      break;
    case Warg_Event_Type::CharSpawn:
      ev.event = malloc(sizeof(CharSpawn_Event));
      *(CharSpawn_Event *)ev.event = deserialize_char_spawn(b);
      break;
    case Warg_Event_Type::PlayerControl:
      ev.event = malloc(sizeof(PlayerControl_Event));
      *(PlayerControl_Event *)ev.event = deserialize_player_control(b);
      break;
    case Warg_Event_Type::Move:
      ev.event = malloc(sizeof(Move_Event));
      *(Move_Event *)ev.event = deserialize_move(b);
      break;
    case Warg_Event_Type::Dir:
      ev.event = malloc(sizeof(Dir_Event));
      *(Dir_Event *)ev.event = deserialize_dir(b);
      break;
    case Warg_Event_Type::Jump:
      ev.event = malloc(sizeof(Jump_Event));
      *(Jump_Event *)ev.event = deserialize_jump(b);
      break;
    case Warg_Event_Type::CharPos:
      ev.event = malloc(sizeof(CharPos_Event));
      *(CharPos_Event *)ev.event = deserialize_char_pos(b);
      break;
    case Warg_Event_Type::Cast:
      ev.event = malloc(sizeof(Cast_Event));
      *(Cast_Event *)ev.event = deserialize_cast(b);
      break;
    case Warg_Event_Type::CastError:
      ev.event = malloc(sizeof(CastError_Event));
      *(CastError_Event *)ev.event = deserialize_cast_error(b);
      break;
    case Warg_Event_Type::CastBegin:
      ev.event = malloc(sizeof(CastBegin_Event));
      *(CastBegin_Event *)ev.event = deserialize_cast_begin(b);
      break;
    case Warg_Event_Type::CastInterrupt:
      ev.event = malloc(sizeof(CastInterrupt_Event));
      *(CastInterrupt_Event *)ev.event = deserialize_cast_interrupt(b);
      break;
    case Warg_Event_Type::CharHP:
      ev.event = malloc(sizeof(CharHP_Event));
      *(CharHP_Event *)ev.event = deserialize_char_hp(b);
      break;
    case Warg_Event_Type::BuffAppl:
      ev.event = malloc(sizeof(BuffAppl_Event));
      *(BuffAppl_Event *)ev.event = deserialize_buff_appl(b);
      break;
    case Warg_Event_Type::ObjectLaunch:
      ev.event = malloc(sizeof(ObjectLaunch_Event));
      *(ObjectLaunch_Event *)ev.event = deserialize_object_launch(b);
      break;
    case Warg_Event_Type::ObjectDestroy:
      ev.event = malloc(sizeof(ObjectDestroy_Event));
      *(ObjectDestroy_Event *)ev.event = deserialize_object_destroy(b);
      break;
    default:
      ASSERT(false);
      break;
  }
  ev.t = get_real_time();

  return ev;
}
