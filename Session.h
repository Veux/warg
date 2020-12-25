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
  virtual void get_state(Game_State &gs, UID &pc) = 0;
  virtual void request_spawn(std::string_view name, int32 team) = 0;
  virtual void move_command(int m, quat orientation, UID target_id) = 0;
  virtual void try_cast_spell(UID target, UID spell) = 0;
  virtual void send_chat_message(std::string_view chat_message) = 0;
  std::vector<Chat_Message> get_chat_log();

protected:
  std::vector<Chat_Message> chat_log;
};

class Local_Session : Session
{
public:
  Local_Session();

  void get_state(Game_State &gs, UID &pc);
  void request_spawn(std::string_view name, int32 team);
  void move_command(int m, quat orientation, UID target_id);
  void try_cast_spell(UID target, UID spell);
  void send_chat_message(std::string_view chat_message);

  UID character = 0;
  Input last_input;
  Resource_Manager resource_manager;
  Flat_Scene_Graph scene;
  std::unique_ptr<Map> map;
  Game_State game_state;
};

class Network_Session : Session
{
public:
  Network_Session();
  ~Network_Session();

  void connect(const char *wargspy_address);
  void send_reliable(Buffer &b);
  void send_unreliable(Buffer &b);

  void get_state(Game_State &gs, UID &pc);
  void request_spawn(std::string_view name, int32 team);
  void move_command(int m, quat orientation, UID target_id);
  void try_cast_spell(UID target, UID spell);
  void send_chat_message(std::string_view chat_message);
  void merge_states(Game_State &gs);

private:
  ENetHost *client = nullptr;
  ENetPeer *server = nullptr;
  Game_State last_state;
  Game_State predicted_state;
  UID character = 0;
  Input last_input;
  Resource_Manager resource_manager;
  Flat_Scene_Graph scene;
  std::unique_ptr<Map> map;
};