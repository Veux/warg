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
  Input last_input = {0};
  float64 last_time = 0;
  ENetPeer *peer = nullptr;
};

struct Chat_Message
{
  std::string name;
  std::string message;
};

void game_server(const char *wargspy_address);
