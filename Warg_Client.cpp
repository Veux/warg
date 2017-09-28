#include "Warg_Client.h"
#include "Warg_State.h"
#include <cstring>
#include <memory>

Warg_Client::Warg_Client()
{
  map = make_nagrand();
  sdb = make_spell_db();
}

void Warg_Client::set_in_queue(std::queue<Warg_Event> *queue) { in = queue; }

void Warg_Client::process_events()
{
  while (!in->empty())
  {
    Warg_Event ev = in->front();
    ASSERT(ev.event);
    switch (ev.type)
    {
      case Warg_Event_Type::CharSpawn:
        process_event((CharSpawn_Event *)ev.event);
        break;
      case Warg_Event_Type::CharPos:
        process_event((CharPos_Event *)ev.event);
        break;
      case Warg_Event_Type::CastError:
        process_event((CastError_Event *)ev.event);
        break;
      case Warg_Event_Type::CastBegin:
        process_event((CastBegin_Event *)ev.event);
        break;
      case Warg_Event_Type::CastInterrupt:
        process_event((CastInterrupt_Event *)ev.event);
        break;
      case Warg_Event_Type::CharHP:
        process_event((CharHP_Event *)ev.event);
        break;
      case Warg_Event_Type::BuffAppl:
        process_event((BuffAppl_Event *)ev.event);
        break;
      case Warg_Event_Type::ObjectLaunch:
        process_event((ObjectLaunch_Event *)ev.event);
        break;
      default:
        break;
    }
    free_warg_event(ev);
    in->pop();
  }
}

void Warg_Client::process_event(CharSpawn_Event *spawn)
{
  char *name = spawn->name;
  int team = spawn->team;

  add_char(team, name);
}

void Warg_Client::process_event(CharPos_Event *ev) {}
void Warg_Client::process_event(CastError_Event *ev) {}
void Warg_Client::process_event(CastBegin_Event *ev) {}
void Warg_Client::process_event(CastInterrupt_Event *ev) {}
void Warg_Client::process_event(CharHP_Event *ev) {}
void Warg_Client::process_event(BuffAppl_Event *ev) {}
void Warg_Client::process_event(ObjectLaunch_Event *ev) {}

void Warg_Client::add_char(int team, const char *name) {}
