#include "Warg_Event.h"

Warg_Event char_spawn_request_event(const char *name, int team)
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

Warg_Event char_spawn_event(const char *name, int team)
{
  ASSERT(name);

  CharSpawn_Event *spawn = (CharSpawn_Event *)malloc(sizeof *spawn);
  spawn->name = (char *)malloc(strlen(name) + 1);
  strcpy(spawn->name, name);
  spawn->team = team;

  Warg_Event ev;
  ev.type = Warg_Event_Type::CharSpawn;
  ev.event = spawn;
  ev.t = get_real_time();

  return ev;
}

Warg_Event dir_event(int ch, vec3 d)
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

Warg_Event move_event(int ch, Move_Status m)
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

Warg_Event char_pos_event(int ch, vec3 dir, vec3 pos)
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

Warg_Event cast_event(int caster, int target, const char *spell)
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

Warg_Event cast_error_event(int caster, int target, const char *spell, int err)
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

Warg_Event cast_begin_event(int caster, int target, const char *spell)
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

Warg_Event cast_interrupt_event(int caster)
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

Warg_Event char_hp_event(int character, int hp)
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

Warg_Event buff_appl_event(int character, const char *buff)
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

Warg_Event object_launch_event(int object, int caster, int target, vec3 pos)
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

void free_warg_event(Dir_Event *dir) {}

void free_warg_event(Move_Event *ev) {}

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
    case Warg_Event_Type::Dir:
      free_warg_event((Dir_Event *)ev.event);
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
