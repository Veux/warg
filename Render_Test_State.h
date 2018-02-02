#pragma once
#include "State.h"

struct Render_Test_State : protected State
{
  Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  void handle_input(State **current_state,
    std::vector<State *> available_states);

  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(0, 1, 0);

  Node_Ptr ground;
  Node_Ptr sky_sphere;
  Node_Ptr cube_star;
  Node_Ptr cube_planet;
  Node_Ptr cube_moon;
  Node_Ptr sphere;
  Node_Ptr tiger;
  Node_Ptr cone_light1;
  Node_Ptr small_light;
  Node_Ptr sun_light;

  std::vector<Node_Ptr> chests;
};