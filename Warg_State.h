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
  ~Warg_State();
  void update();
  void draw_gui();
  virtual void handle_input_events() final;
  void process_messages();
  void register_move_command(Move_Status m, quat orientation);
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

  Session *session = nullptr;
  Latency_Tracker latency_tracker;

  Map *map;

  UID player_character_id = 0;
  UID target_id = 0;

  Game_State last_recieved_server_state;
  std::deque<Game_State> state_buffer;
  Game_State current_game_state;

  Input_Buffer input_buffer;
  uint32 input_number = 0;

  Spell_Database spell_db;

  // veux: why not put these Node_Index inside of the Character struct?
  std::map<UID, Node_Index> character_nodes;
  std::map<UID, Node_Index> spell_object_nodes;
  std::vector<Animation_Object> animation_objects;

  vec4 character_to_camera;

  Interface_State interface_state;
};

Character *get_character(Game_State *game_state, UID id);


//void warg_input_handler(Character *player, Game_State *state, events) {
//
//  for (auto &_e : events)
//  {
//    if (_e == "b")
//    {
//      player->do(state->spell["blink"]);  
//    }
//  }
//}
 struct Nu_Warg_Client : protected State
  {
  Nu_Warg_Client(std::string name, SDL_Window *window, ivec2 window_size): State(name, window, window_size) {
   // server = connect(port(1337));
   // me = server->send_message("newcharacter");
  }
  void handle_input_events()
  {
   // Character *me_ptr = server_game_state.characters[me];
    for (auto &_e : events_this_tick)
    {}
  // send_input_to_server(server);
  }
  void update()
  {
   // predict_state()
   // client_game_state.update();
  };
  void draw_gui()
  {
    ImGui::Begin(s("Hello from:draw_gui():", state_name).c_str(), &test_window);
    ImGui::Text(s("Hello from:draw_gui():", state_name).c_str());
    ImGui::End();
  
  };

  UID me;
  //Server *server;
  Game_State client_game_state;
};
struct Nu_Warg_Server : protected State
{
  Nu_Warg_Server(std::string name, SDL_Window *window, ivec2 window_size): State(name, window, window_size) {
  
   // network_init(port(1337));
   // me = server_game_state.characters.add_new_character();//-------------------------------------------------------
  }

  void handle_input_events()
  {
   // Character *me_ptr = server_game_state.characters[me];//-------------------------------------------------------
   // warg_input_handler(me_ptr, &server_game_state, events_this_tick);//---------------------------------------------
  }
  void update()
  {
   // input = gather_network_input();
   // server_game_state.update(input);
   // send_state(connected_clients);
  };
  void draw_gui()
  {    ImGui::Begin(s("Hello from:draw_gui():", state_name).c_str(), &test_window);
    ImGui::Text(s("Hello from:draw_gui():", state_name).c_str());
    ImGui::End();
  };
  UID me;//------------------------------------------------------
 // vector<client> connected_clients;
  Game_State server_game_state;
};
