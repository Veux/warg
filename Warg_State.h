#pragma once
#include "Physics.h"
#include "Session.h"
#include "Spell.h"
#include "State.h"
#include "Warg_Common.h"
#include "Warg_Event.h"
#include "Warg_Server.h"
#include <deque>
struct Frostbolt_Effect_2;
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
  bool icons_loaded = false;
  bool icon_setup_complete = false;
  std::vector<Texture> action_bar_textures_sources;
  std::vector<Texture> action_bar_textures_with_cooldown;
  Framebuffer duration_spiral_fbo;
  Shader duration_spiral_shader;
  bool focus_chat = false;
};

struct WargSpy_State
{
  ENetHost *host = nullptr;
  ENetPeer *wargspy = nullptr;
  uint32 game_server_address = 0;
  bool connecting = false;
  bool asking = false;
  double time_till_next_ask = 0;
};

struct Warg_State : protected State
{
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size, std::string_view character_name, int32 team);
  ~Warg_State();
  void session_swapper();
  void initialize_warg_state();
  void update();
  void draw_gui();
  virtual void handle_input_events() final;
  Node_Index add_character_mesh(UID character_id);
  void set_camera_geometry();
  void update_character_nodes();
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
  void draw_chat_box();
  void update_wargspy();
  void send_wargspy_request();

  Session *session = nullptr;

  Map *map = nullptr;

  UID player_character_id = 0;
  UID target_id = 0;

  std::deque<Game_State> state_buffer;
  Game_State current_game_state;

  // veux: why not put these Node_Index inside of the Character struct?
  std::map<UID, Node_Index> character_nodes;
  std::map<UID, Node_Index> spell_object_nodes;
  std::vector<Animation_Object> animation_objects;

  vec4 character_to_camera;

  Interface_State interface_state;
  WargSpy_State wargspy_state;

  Liquid_Surface terrain;

  std::string character_name;
  int32 team;


  bool want_set_intro_scene = true;
  std::unique_ptr<Frostbolt_Effect_2> frostbolt_effect2;

};
