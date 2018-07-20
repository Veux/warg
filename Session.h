#pragma once

#include "Globals.h"
#include "Warg_Event.h"
#include "Warg_Server.h"
#include "Peer.h"
#include <memory>
#include <vector>

using std::unique_ptr;
using std::shared_ptr;

class Session
{
public:
  virtual void push(unique_ptr<Message> ev) = 0;
  virtual std::vector<unique_ptr<Message>> pull() = 0;
};

class Local_Session : Session
{
public:
  Local_Session();
  void push(unique_ptr<Message> ev);
  std::vector<unique_ptr<Message>> pull();

private:
  shared_ptr<Warg_Server> server;
  shared_ptr<Local_Peer> peer;
  std::thread server_thread;
};