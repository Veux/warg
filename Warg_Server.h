#pragma once

#include "Globals.h"
#include "Physics.h"
#include "Warg_Event.h"
#include "Warg_Common.h"
#include "Peer.h"
#include <enet/enet.h>
#include <map>
#include <queue>

struct Warg_Server
{
  Warg_Server();
  ~Warg_Server();
  void update(float32 dt);
  void connect(std::shared_ptr<Peer> peer);
  void process_messages();

  std::map<UID, std::shared_ptr<Peer>> peers;
  float64 time = 0;
  uint32 tick = 0;
  std::atomic<bool> running = false;
  Map *map;
  Resource_Manager resource_manager;
  Flat_Scene_Graph scene;
  Spell_Database spell_db;
  Game_State game_state;
};
