#pragma once

#include "Globals.h"
#include "Warg_Event.h"
#include <queue>

struct Warg_Server
{
  void process_events();
  std::queue<Warg_Event> in;
  std::queue<Warg_Event> *out;
};