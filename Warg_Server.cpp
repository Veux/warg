#include "Warg_Server.h"
#include "Warg_State.h"
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
        break;
      }
      case Move:
      {
        Move_Event *move = (Move_Event *)ev.event;
        int chr = move->character;
        vec3 v = move->v;

        move_char(chr, v);

        free(move);
        break;
      }
      default:
        break;
    }
    in.pop();
  }
  for (int i = 0; i < chars.size(); i++)
  {
    CharPos_Event *pos = (CharPos_Event *)malloc(sizeof *pos);
    pos->character = i;
    pos->pos = chars[i].pos;
    Warg_Event ev;
    ev.type = CharPos;
    ev.event = pos;
    out->push(ev);
  }
}

void Warg_Server::move_char(int ci, vec3 v)
{
  Character *c = &chars[ci];
  ASSERT(c);
  if (c->casting)
  {
    c->casting = false;
    c->cast_progress = 0;
    set_message("Interrupt:", s(c->name, "'s casting interrupted: moved"),
                3.0f);
  }

  vec3 old_pos = c->pos;

  c->pos += c->e_stats.speed * v;

  for (auto &wall : walls)
  {
    if (intersects(c->pos, c->body, wall))
    {
      if (!intersects(vec3(c->pos.x, old_pos.y, c->pos.z), c->body, wall))
      {
        c->pos.y = old_pos.y;
      }
      else if (!intersects(vec3(old_pos.x, c->pos.y, old_pos.z), c->body, wall))
      {
        c->pos.x = old_pos.x;
      }
      else
      {
        c->pos = old_pos;
      }
    }
  }
}
