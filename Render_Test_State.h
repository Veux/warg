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
  std::vector<std::pair<Node_Index,float32>> billboards;
  bool update(State* owning_state, vec3 target);
  float32 speed = 14.0f;
  uint32 light_index = 0;
  float32 spacing = 0.1f;
  float32 rotation = 0.f;
  Node_Index billboard_spawn_source;
  uint32 particle_emitter_index = 0;
};

struct Frostbolt_Effect_2
{
  Frostbolt_Effect_2(State* state, uint32 available_light_index);
  Node_Index crystal;
  Node_Index billboard_spawn_source;
  bool update(State* owning_state, vec3 target);
  float32 speed = 14.0f;
  uint32 light_index = 0;
  float32 rotation_inversion = 1.f;
  float32 spawn_rotation = 0.f;
  uint32 particle_emitter_index = 0;
};