#pragma once
#include "State.h"
#include "Warg_Common.h"
struct Render_Test_State : protected State
{
  Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  void draw_gui();
  virtual void handle_input_events() final;

  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(0, 1, 0);
  
  Liquid_Surface terrain;
};