#include "Warg_Server.h"
#include <cstring>
#include <memory>

void Warg_Server::process_events()
{
  while (!in.empty())
  {
    Warg_Event ev = in.front();
    switch (ev.type)
    {
      case CharSpawnRequest:
      {
        CharSpawnRequest_Event *req = (CharSpawnRequest_Event *)ev.event;
        char *name = req->name;
        int team = req->team;

        CharSpawn_Event *spawn = (CharSpawn_Event *)malloc(sizeof *spawn);
        spawn->name = (char *)malloc(strlen(name) + 1);
        strcpy(spawn->name, name);
        spawn->team = team;

        Warg_Event out_ev;
        ev.type = CharSpawn;
        ev.event = (void *)spawn;
        out->push(ev);

        free(name);
        free(req);
      }
      default:
        break;
    }
    in.pop();
  }
}
