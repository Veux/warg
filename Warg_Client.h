#pragma once

#include "Globals.h"
#include "Warg_Event.h"
#include <queue>

struct Character;

struct Warg_Client
{
  std::queue<Warg_Event> in;
  std::queue<Warg_Event> *out;
  std::vector<Character> chars;
  uint32 nchars = 0;
};
