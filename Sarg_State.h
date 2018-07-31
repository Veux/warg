#pragma once
#include "Physics.h"
#include "State.h"

struct Sarg_State : protected State
{
  Sarg_State(std::string name, SDL_Window *window, ivec2 window_size);
  virtual void update() = 0;
  virtual void handle_input_events();
};

struct Sarg_Client_State : protected Sarg_State
{
  Sarg_Client_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  virtual void handle_input_events() final;
};

struct Sarg_Server_State : protected Sarg_State
{
  Sarg_Server_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  virtual void handle_input_events() final;
};