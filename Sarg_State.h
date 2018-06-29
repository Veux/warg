#pragma once
#include "Physics.h"
#include "State.h"

struct Sarg_State : protected State
{
  Sarg_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  virtual void handle_input_events(const std::vector<SDL_Event> &events,
      bool block_kb, bool block_mouse) final;
};