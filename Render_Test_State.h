#pragma once
#include "State.h"

struct Render_Test_State : protected State
{
  Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  void draw_gui()
  {
    IMGUI_LOCK lock(this);
    scene.draw_imgui(state_name);
  }
  virtual void handle_input_events() final;

  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(0, 1, 0);

  Node_Index ground;
  Node_Index underwater;
  Node_Index memewater;
  Node_Index waterglow1;
  Node_Index waterglow2;
  Node_Index waterglow3;
  Node_Index waterglow4;
  Node_Index grabbycube;
  Node_Index cube_planet;
  Node_Index cube_moon;
  Node_Index sphere;
  Node_Index tiger;
  Node_Index gun;
  Node_Index testobjects;
  Node_Index tiger2;
  Node_Index tiger1;
  Node_Index arm_test;
  Node_Index shoulder_joint;
  Node_Index sphere_raycast_test;
  Node_Index fire_particle;
  std::vector<Node_Index> chests;
};