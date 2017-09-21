#pragma once

#include "Globals.h"
#include "Warg_Event.h"
#include <queue>

struct Character;
struct Wall;

struct Warg_Server
{
  void process_events();
  std::queue<Warg_Event> in;
  std::queue<Warg_Event> *out;
  std::vector<Character> chars;
  uint32 nchars = 0;
  std::vector<Wall> walls;

  void move_char(int ci, vec3 v);
};
