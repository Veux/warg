#pragma once

#include "Globals.h"
#include "Warg_Event.h"
#include "Warg_Server.h"
#include "Peer.h"
#include <memory>
#include <vector>


class Session
{
public:
  virtual void push(std::unique_ptr<Message> ev) = 0;
  virtual std::vector<std::unique_ptr<Message>> pull() = 0;
  virtual void end() = 0;
};

class Local_Session : Session
{
public:
  Local_Session();
  void push(std::unique_ptr<Message> ev);
  std::vector<std::unique_ptr<Message>> pull();
  void end();


  std::shared_ptr<Warg_Server> server;
  std::shared_ptr<Local_Peer> peer;
  std::thread server_thread;
};