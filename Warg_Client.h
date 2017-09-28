#pragma once

#include "Functions.h"
#include "Globals.h"
#include "Warg_Event.h"
#include <queue>

using std::queue;
using std::unique_ptr;
using std::vector;

struct Character;

struct Warg_Client
{
  Warg_Client();
  void update(float32 dt);
  void set_in_queue(std::queue<Warg_Event> *queue);

  queue<Warg_Event> *in;

  Map map;
  unique_ptr<SpellDB> sdb;
  vector<Character> chars;
  vector<SpellObjectInst> spell_objs;
  int pc = -1;

private:
  void process_events();
  void process_event(CharSpawn_Event *ev);
  void process_event(CharPos_Event *ev);
  void process_event(CastError_Event *ev);
  void process_event(CastBegin_Event *ev);
  void process_event(CastInterrupt_Event *ev);
  void process_event(CharHP_Event *ev);
  void process_event(BuffAppl_Event *ev);
  void process_event(ObjectLaunch_Event *ev);

  void add_char(int team, const char *name);
};
