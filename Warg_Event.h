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

struct Move_Event
{
  int character;
  vec3 v;
};

struct CharPos_Event
{
  int character;
  vec3 pos;
};

enum Warg_Event_Type
{
  CharSpawnRequest,
  CharSpawn,
  Move,
  CharPos
};

struct Warg_Event
{
  Warg_Event_Type type;
  void *event;
};
