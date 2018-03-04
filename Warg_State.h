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
  virtual void handle_input_events(const std::vector<SDL_Event> &events,
      bool block_kb, bool block_mouse) final;
  void process_packets();
  void process_events();
  void add_char(UID id, int team, const char *name);

  unique_ptr<Warg_Server> server;
  queue<unique_ptr<Message>> in, out;
  bool local;
  ENetPeer *serverp;
  ENetHost *clientp;
  Map map;
  std::map<UID, Character> chars;
  UID pc = 0;
  unique_ptr<SpellDB> sdb;
  vector<SpellObjectInst> spell_objs;
  uint32 tick = 0;
  uint32 server_tick = 0;
  float64 last_ping_sent;
  float64 last_latency = 0;
};
