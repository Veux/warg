#pragma once

#include "Globals.h"
#include "Physics.h"
#include "Warg_Event.h"
#include "Warg_Common.h"
#include "Peer.h"
#include <enet/enet.h>
#include <map>
#include <queue>

struct Peer
{
  UID character = 0;
  Input last_input;
  ENetPeer *peer;
};

struct Chat_Message
{
  std::string name;
  std::string message;
};

struct Warg_Server
{
  Warg_Server();
  void run(const char *wargspy_address);


  std::map<UID, Peer> peers;
  std::unique_ptr<Map> map;
  Resource_Manager resource_manager;
  Flat_Scene_Graph scene;
  Game_State game_state;

  std::vector<Chat_Message> chat_log;
};
