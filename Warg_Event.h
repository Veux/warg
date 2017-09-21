#pragma once

struct CharSpawnRequest_Event
{
  char *name;
  int team;
};

struct CharSpawn_Event
{
  char *name;
  int team;
};

enum Warg_Event_Type
{
  CharSpawnRequest,
  CharSpawn
};

struct Warg_Event
{
  Warg_Event_Type type;
  void *event;
};
