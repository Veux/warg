#pragma once
#include "Physics.h"
#include "Session.h"
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
  Node_Index max;
  Node_Index current;
};

struct Animation_Object
{
  Node_Index node;
  Node_Index collision_node;
  Character_Physics physics;
  vec3 radius;
};

struct Interface_State
{
  std::vector<Texture> action_bar_textures;
};

struct Warg_State : protected State
{
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size, Session *session);
  void update();
  virtual void handle_input_events(const std::vector<SDL_Event> &events, bool block_kb, bool block_mouse) final;
  void process_messages();
  void register_move_command(Move_Status m, quat orientation);
  void add_character_mesh(UID character_id);
  void set_camera_geometry();
  void update_character_nodes();
  void update_prediction_ghost();
  void update_stats_bar();
  void predict_state();
  void update_meshes();
  void update_spell_object_nodes();
  void update_hp_bar(UID character_id);
  void animate_character(UID character_id);
  void update_game_interface();
  void update_cast_bar();
  void update_unit_frames();
  void update_animation_objects();
  void update_action_bar();
  void update_icons();
  void update_buff_indicators();

  Session *session = nullptr;
  Latency_Tracker latency_tracker;

  Map map;
  vector<Triangle> collider_cache;

  UID pc = 0;
  UID target_id = 0;

  Game_State server_state;
  std::deque<Game_State> state_buffer;
  Game_State game_state;

  Input_Buffer input_buffer;
  uint32 input_number = 0;

  unique_ptr<Spell_Database> sdb;

  std::map<UID, Node_Index> character_nodes;
  std::map<UID, Node_Index> spell_object_nodes;
  std::vector<Animation_Object> animation_objects;

  vec4 character_to_camera;

  Interface_State interface_state;
};
