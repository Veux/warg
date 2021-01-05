#include "stdafx.h"
#include "Session.h"
#include "Warg_Common.h"

Local_Session::Local_Session() : scene(&resource_manager)
{  
  map = std::make_unique<Map>(Blades_Edge(scene));
  for (int i = 0; i < 1; i++)
    add_dummy(game_state, *map, {1, i, 5});
}

void Local_Session::get_state(Game_State &gs, UID &pc)
{
  std::map<UID, Input> inputs;
  inputs[character] = last_input;
  update_game(game_state, *map, scene, inputs);

  gs = game_state;
  pc = character;
}

void Local_Session::request_spawn(std::string_view name, int team)
{
  character = add_char(game_state, *map, team, name);
}

void Local_Session::move_command(int m, quat orientation, UID target_id)
{
  Input command;
  command.orientation = orientation;
  command.m = m;
  last_input = command;
  auto t = find_if(game_state.character_targets, [&](auto &t) { return t.character == character; });
  if (t != game_state.character_targets.end())
    t->target = target_id;
  else
    game_state.character_targets.push_back({character, target_id});
}

void Local_Session::try_cast_spell(UID target, UID spell)
{
  cast_spell(game_state, scene, character, target, spell);
}

void Local_Session::send_chat_message(std::string_view chat_message)
{
  auto c = find_if(game_state.characters, [&](auto &c) { return c.id == character; });
  Chat_Message cm;
  if (c != game_state.characters.end())
    cm.name = c->name;
  else
    cm.name = "Unknown";
  cm.message = chat_message;
  chat_log.push_back(cm);
}

std::vector<Chat_Message> Session::get_chat_log()
{
  return chat_log;
}

Network_Session::Network_Session() : scene(&resource_manager)
{  
  map = std::make_unique<Map>(Blades_Edge(scene));
}

Network_Session::~Network_Session()
{
  enet_host_destroy(warg_client_host);
}

void Network_Session::connect(uint32 server_address)
{
  warg_client_host = enet_host_create(NULL, 1, 2, 0, 0);
  ASSERT(warg_client_host);

  ENetAddress addr = { .host = server_address, .port = GAME_PORT };
  server = enet_host_connect(warg_client_host, &addr, 2, 0);
  ASSERT(server);

  ENetEvent event;
  int res = enet_host_service(warg_client_host, &event, 5000);
  //res == 0
  ASSERT(res > 0 && event.type == ENET_EVENT_TYPE_CONNECT);
}

void Network_Session::send_reliable(Buffer &b)
{
  ENetPacket *packet = enet_packet_create(b.data.data(), b.data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(server, 0, packet);
  enet_host_flush(warg_client_host);
}

void Network_Session::send_unreliable(Buffer &b)
{
  ENetPacket *packet = enet_packet_create(b.data.data(), b.data.size(), ENET_PACKET_FLAG_UNSEQUENCED);
  enet_peer_send(server, 0, packet);
  enet_host_flush(warg_client_host);
}

void Network_Session::get_state(Game_State &gs, UID &pc)
{
  ENetEvent event;
  while (enet_host_service(warg_client_host, &event, 0) > 0)
  {
    switch (event.type)
    {
      case ENET_EVENT_TYPE_RECEIVE:
      {
        Buffer b;
        b.insert(event.packet->data, event.packet->dataLength);
        int32 message_type;
        deserialize(b, message_type);

        switch (message_type)
        {
          case STATE_MESSAGE:
          {
            State_Update su;
            deserialize(b, su.gs);
            deserialize(b, su.character);
            deserialize(b, su.input_number);

            server_state = su;

            break;
          }

          case CHAT_MESSAGE_RELAY:
          {
            Chat_Message cm;
            deserialize(b, cm.name);
            deserialize(b, cm.message);
            chat_log.push_back(cm);
            break;
          }
        }

        enet_packet_destroy(event.packet);

        break;
      }
    }
  }


  {
    gs = server_state.gs;
    pc = server_state.character;

    // pop old inputs
    while (inputs.size() && inputs.front().sequence_number <= server_state.input_number)
      inputs.pop_front();

    if (prediction_enabled)
    {
      // starting from the latest server state
      // reapply all newer inputs to the server state
      for (auto &input : inputs)
        gs = predict(gs, *map, scene, pc, input);
    }




    //std::cout << s("num inputs: ", inputs.size(), "\n");


    //while (predicted_states.size() && predicted_states.front().input_number < server_state.input_number)
    //  predicted_states.pop_front();

    //if (predicted_states.empty() || !verify_prediction(predicted_states.front().gs, server_state.gs, pc))
    //{
    //  std::cout << "incorrect on input number: " << server_state.input_number << '\n';

    //  while (predicted_states.size())
    //    predicted_states.pop_front();
    //  predicted_states.push_back(State_Prediction{server_state.gs, server_state.input_number});
    //}

    //auto start_input_it = find_if(inputs,
    //  [&](auto &in){ return in.sequence_number == predicted_states.back().input_number + 1; });

    //for (auto it = start_input_it; it != inputs.end(); it++)
    //{
    //  predicted_states.push_back(State_Prediction{
    //    .gs = predict(predicted_states.back().gs, *map, scene, pc, *it),
    //    .input_number = it->sequence_number
    //  });
    //}

    //if (predicted_states.size())
    //  merge_prediction(gs, predicted_states.back().gs, pc);
  }
}

void Network_Session::request_spawn(std::string_view name, int32 team)
{
  Buffer b;
  serialize_(b, SPAWN_MESSAGE);
  serialize_(b, name);
  serialize_(b, team);
  send_reliable(b);
}

void Network_Session::move_command(int m, quat orientation, UID target_id)
{
  Input in = {
    .sequence_number = input_counter++,
    .m = m,
    .orientation = orientation
  };

  inputs.push_back(in);

  Buffer b;
  serialize_(b, MOVE_MESSAGE);
  serialize_(b, in.sequence_number);
  serialize_(b, in.m);
  serialize_(b, in.orientation);
  serialize_(b, target_id);
  send_unreliable(b);
}

void Network_Session::try_cast_spell(UID target, UID spell) {
  Buffer b;
  serialize_(b, CAST_MESSAGE);
  serialize_(b, target);
  serialize_(b, spell);
  send_reliable(b);
}

void Network_Session::send_chat_message(std::string_view chat_message) {
  Buffer b;
  serialize_(b, CHAT_MESSAGE);
  serialize_(b, chat_message);
  send_reliable(b);
}
