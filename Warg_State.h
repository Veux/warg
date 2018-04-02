#pragma once
#include "Physics.h"
#include "Spell.h"
#include "State.h"
#include "Warg_Event.h"
#include "Warg_Server.h"
#include "Warg_Common.h"
#include <queue>

class Latency_Tracker
{
public:
  bool should_send_ping();
  void ack_received();
  uint32 get_latency();
private:
  float64 last_ping = 0;
  float64 last_ack = 0;
  float64 last_latency = 0;
  bool acked = true;
};

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
  void push(unique_ptr<Message> msg);
  void add_char(UID id, int team, const char *name);
  void register_move_command(Move_Status m, vec3 dir);

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

  Latency_Tracker latency_tracker;

  uint32 move_cmd_n = 0;
};
