#include "Sarg_State.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "State.h"
#include "Third_party/imgui/imgui.h"
#include <atomic>
#include <memory>
#include <sstream>
#include <thread>

Sarg_State::Sarg_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{
}

void Sarg_State::update() {}
void Sarg_State::handle_input_events(
    const std::vector<SDL_Event> &events, bool block_kb, bool block_mouse)
{
}
