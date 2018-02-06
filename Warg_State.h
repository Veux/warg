#pragma once
#include "Physics.h"
#include "Spell.h"
#include "State.h"
#include "Warg_Event.h"
#include "Warg_Server.h"
#include "Warg_Common.h"
#include <queue>

struct Warg_State : protected State
{
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size);
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size,
      const char *address, const char *char_name, uint8_t team);
  void update();
  void handle_input(
      State **current_state, std::vector<State *> available_states);
  void process_packets();
  void process_events();
  void process_event(CharSpawn_Event *ev);
  void process_event(PlayerControl_Event *ev);
  void process_event(CharPos_Event *ev);
  void process_event(CastError_Event *ev);
  void process_event(CastBegin_Event *ev);
  void process_event(CastInterrupt_Event *ev);
  void process_event(CharHP_Event *ev);
  void process_event(BuffAppl_Event *ev);
  void process_event(ObjectLaunch_Event *ev);
  void add_char(UID id, int team, const char *name);

  std::unique_ptr<Warg_Server> server;
  std::queue<Warg_Event> in, out;
  bool local;
  ENetPeer *serverp;
  ENetHost *clientp;
  Map map;
  std::map<UID, Character> chars;
  UID pc = 0;
  unique_ptr<SpellDB> sdb;
  vector<SpellObjectInst> spell_objs;
};
