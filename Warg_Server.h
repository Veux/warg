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
  uint32 last_input_number;
  ENetPeer *peer;
};

struct Chat_Message
{
  std::string name;
  std::string message;
};

void game_server(const char *wargspy_address);
