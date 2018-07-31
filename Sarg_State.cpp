#include "stdafx.h"
#include "Sarg_State.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "State.h"

Sarg_State::Sarg_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size, &IMGUI)
{
}

void Sarg_State::handle_input_events() {}

Sarg_Client_State::Sarg_Client_State(std::string name, SDL_Window *window, ivec2 window_size)
    : Sarg_State(name, window, window_size)
{
}
void Sarg_Client_State::update()
{
  if (!imgui_this_tick)
    return;

  IMGUI_LOCK lock(this);
  ImGui::Begin(s("Hello from:update():", state_name).c_str(), &test_window);
  ImGui::Text(s("Hello from:update():", state_name).c_str());
  ImGui::End();
}
void Sarg_Client_State::handle_input_events() {}

Sarg_Server_State::Sarg_Server_State(std::string name, SDL_Window *window, ivec2 window_size)
    : Sarg_State(name, window, window_size)
{
}
void Sarg_Server_State::update()
{
  if (!imgui_this_tick)
    return;

  IMGUI_LOCK lock(this);
  ImGui::Begin(s("Hello from:update():", state_name).c_str(), &test_window);
  ImGui::Text(s("Hello from:update():", state_name).c_str());
  ImGui::End();
}
void Sarg_Server_State::handle_input_events() {}
