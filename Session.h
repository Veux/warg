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

  bool prediction_enabled = true;
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
  Scene_Graph scene;
  std::unique_ptr<Map> map;
  Game_State game_state;
};

struct State_Update
{
  Game_State gs = {0};
  uint32 input_number = 0;
  UID character = 0;
};

struct State_Prediction
{
  Game_State gs = {0};
  uint32 input_number = 0;
};

class Network_Session : Session
{
public:
  Network_Session();
  ~Network_Session();

  void connect(uint32 server_address);
  void send_reliable(Buffer &b);
  void send_unreliable(Buffer &b);

  void get_state(Game_State &gs, UID &pc);
  void request_spawn(std::string_view name, int32 team);
  void move_command(int m, quat orientation, UID target_id);
  void try_cast_spell(UID target, UID spell);
  void send_chat_message(std::string_view chat_message);

private:
  ENetHost *warg_client_host = nullptr;
  ENetPeer *server = nullptr;
  State_Update server_state = {0};
  std::list<State_Prediction> predicted_states;
  std::list<Input> inputs;
  Resource_Manager resource_manager;
  Scene_Graph scene;
  std::unique_ptr<Map> map;
  uint32 input_counter = 0;
};