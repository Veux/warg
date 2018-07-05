#pragma once
#include "Physics.h"
#include "Spell.h"
#include "State.h"
#include "Warg_Common.h"
#include "Warg_Event.h"
#include "Warg_Server.h"
#include <deque>

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

struct HP_Bar_Nodes
{
  Node_Ptr max;
  Node_Ptr current;
};

struct Warg_State : protected State
{
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size);
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size, const char *address, const char *char_name,
      uint8_t team);
  void update();
  virtual void handle_input_events(const std::vector<SDL_Event> &events, bool block_kb, bool block_mouse) final;
  void process_packets();
  void process_events();
  void push(unique_ptr<Message> msg);
  void add_char(UID id, int team, const char *name);
  void register_move_command(Move_Status m, vec3 dir);
  void add_character_mesh(UID character_id);
  void set_camera_geometry();
  void update_character_nodes();
  void update_prediction_ghost();
  void update_stats_bar();
  void predict_player_character();
  void send_messages();
  void process_messages();
  void send_ping();
  void predict_state();
  void update_meshes();
  void update_spell_object_nodes();
  void update_hp_bar(UID character_id);
  void animate_character(UID character_id);


  unique_ptr<Warg_Server> server;
  queue<unique_ptr<Message>> in, out;
  bool local;
  ENetPeer *serverp;
  ENetHost *clientp;
  Latency_Tracker latency_tracker;

  Map map;
  vector<Triangle> collider_cache;

  UID pc = 0;

  Game_State server_state;
  std::deque<Game_State> state_buffer;
  Game_State game_state;

  Input_Buffer input_buffer;
  uint32 input_number = 0;

  unique_ptr<SpellDB> sdb;

  std::map<UID, Node_Ptr> character_nodes;
  std::map<UID, Node_Ptr> spell_object_nodes;
  std::map<UID, HP_Bar_Nodes> hp_bar_nodes;

  vec4 cam_rel;
};
