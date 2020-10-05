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

struct Frostbolt_Effect
{
  Frostbolt_Effect(State* state, uint32 available_light_index);
  Node_Index crystal;
  std::vector<Node_Index> billboards;
  Particle_Emitter emitter;
  bool update(State* owning_state, vec3 target);
  float32 speed = 4.0f;
  uint32 light_index = 0;
};
