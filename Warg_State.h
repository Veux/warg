#pragma once
#include "Physics.h"
#include "Session.h"
#include "Spell.h"
#include "State.h"
#include "Warg_Common.h"
#include "Warg_Event.h"
#include "Warg_Server.h"
#include <deque>

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
};

struct Warg_State : protected State
{
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size, std::string_view character_name, int32 team);
  ~Warg_State();
  void set_session(Session *session);
  void session_swapper();
  void update();
  void draw_gui();
  virtual void handle_input_events() final;
  void process_messages();
  void add_character_mesh(UID character_id);
  void add_girl_character_mesh(UID character_id);
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
  void draw_chat_box();

  Session *session = nullptr;

  Map *map = nullptr;

  UID player_character_id = 0;
  UID target_id = 0;

  std::deque<Game_State> state_buffer;
  Game_State current_game_state;

  Input_Buffer input_buffer;
  uint32 input_number = 0;

  // veux: why not put these Node_Index inside of the Character struct?
  std::map<UID, Node_Index> character_nodes;
  std::map<UID, Node_Index> spell_object_nodes;
  std::vector<Animation_Object> animation_objects;

  vec4 character_to_camera;

  Interface_State interface_state;

  std::string character_name;
  int32 team;


};