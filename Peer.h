#pragma once

#include "Globals.h"
#include "Warg_Event.h"
#include "Warg_Common.h"
#include <memory>
#include <vector>

using std::unique_ptr;
using std::shared_ptr;

class Peer
{
public:
  virtual void push_to_peer(unique_ptr<Message> message) = 0;
  virtual std::vector<unique_ptr<Message>> pull_from_peer() = 0;

  UID character = 0;
  Input last_input;
};

class Local_Peer : public Peer
{
public:
  void push_to_peer(unique_ptr<Message> message);
  std::vector<unique_ptr<Message>> pull_from_peer();

  void push_to_server(unique_ptr<Message> message);
  std::vector<unique_ptr<Message>> pull_from_server();

private:
  std::queue<unique_ptr<Message>> from_server;
  std::queue<unique_ptr<Message>> to_server;
  std::mutex from_server_mutex;
  std::mutex to_server_mutex;
};
