#pragma once
#include "State.h"

struct Render_Test_State : protected State
{
  Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  virtual void handle_input_events(const std::vector<SDL_Event> &events, bool block_kb, bool block_mouse) final;

  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(0, 1, 0);

  Node_Index ground;
  Node_Index skybox;
  Node_Index grabbycube;
  Node_Index cube_planet;
  Node_Index cube_moon;
  Node_Index sphere;
  Node_Index tiger;
  Node_Index gun;
  Node_Index cone_light1;
  Node_Index small_light;
  Node_Index sun_light;
  Node_Index testobjects;
  Node_Index tiger2;
  Node_Index tiger1;
  Node_Index arm_test;
  Node_Index shoulder_joint;
  std::vector<Node_Index> chests;
};