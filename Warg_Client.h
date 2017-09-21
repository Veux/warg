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

  vec3 spawnloc0 = vec3(8, 2, 0.5f);
  vec3 spawnloc1 = vec3(8, 20, 0.5f);
  vec3 spawndir0 = vec3(0, 1, 0);
  vec3 spawndir1 = vec3(0, -1, 0);
};
