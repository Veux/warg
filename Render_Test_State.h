#pragma once
#include "State.h"

struct Render_Test_State : protected State
{
  Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  virtual void handle_input_events(const std::vector<SDL_Event> &events, bool block_kb, bool block_mouse) final;

  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(0, 1, 0);

  Node_Ptr ground;
  Node_Ptr skybox;
  Node_Ptr cube_star;
  Node_Ptr cube_planet;
  Node_Ptr cube_moon;
  Node_Ptr sphere;
  Node_Ptr tiger;
  Node_Ptr gun;
  Node_Ptr cone_light1;
  Node_Ptr small_light;
  Node_Ptr sun_light;
  Node_Ptr testobjects;

  std::vector<Node_Ptr> chests;
};