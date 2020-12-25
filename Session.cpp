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
  command.number = 0;
  command.orientation = orientation;
  command.m = m;
  last_input = command;
  auto t = std::find_if(game_state.character_targets.begin(), game_state.character_targets.end(),
      [&](auto &t) { return t.character == character; });
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
  enet_host_destroy(client);
}

void Network_Session::connect(const char *wargspy_address)
{
  client = enet_host_create(NULL, 1, 2, 0, 0);
  ASSERT(client);


  // get game server ip address from wargspy
  ENetAddress addr;
  {
    ENetPeer *wargspy;
    enet_address_set_host(&addr, wargspy_address);
    addr.port = WARGSPY_PORT;
    wargspy = enet_host_connect(client, &addr, 2, 0);

    ENetEvent event;
    ASSERT(enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT);

    {
      uint8 byte = 2;
      ENetPacket *packet = enet_packet_create(&byte, 1, 0);
      enet_peer_send(wargspy, 0, packet);
      enet_host_flush(client);
    }

    enet_host_service(client, &event, 10000);

    ASSERT(event.type == ENET_EVENT_TYPE_RECEIVE);

    Buffer b;
    b.insert(event.packet->data, event.packet->dataLength);

    uint8 type;
    deserialize(b, type);
    ASSERT(type == 1);
    uint32 host;
    deserialize(b, host);
    addr.host = host;
    addr.port = GAME_PORT;

    enet_peer_disconnect_now(wargspy, 0);
  }

  // connect to game server
  {
    ENetEvent event;
    server = enet_host_connect(client, &addr, 2, 0);
    ASSERT(server);

    int res = enet_host_service(client, &event, 5000);
    ASSERT(res > 0 && event.type == ENET_EVENT_TYPE_CONNECT);
  }
}

void Network_Session::send_reliable(Buffer &b)
{
  ENetPacket *packet = enet_packet_create(b.data.data(), b.data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(server, 0, packet);
  enet_host_flush(client);
}

void Network_Session::send_unreliable(Buffer &b)
{
  ENetPacket *packet = enet_packet_create(b.data.data(), b.data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(server, 0, packet);
  enet_host_flush(client);
}

void Network_Session::get_state(Game_State &gs, UID &pc)
{
  ENetEvent event;
  while (enet_host_service(client, &event, 0) > 0)
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
            deserialize(b, last_state);
            deserialize(b, character);
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

  if (pc == 0)
    predicted_state = last_state;
  pc = character;
  merge_states(gs);
}

void Network_Session::merge_states(Game_State &gs)
{
  gs = last_state;

  std::map<UID, Input> inputs;
  inputs[character] = last_input;
  update_game(predicted_state, *map, scene, inputs);

  auto sc = find_if(gs.characters, [&](auto &c) { return c.id == character; });
  if (sc == gs.characters.end())
    return;

  auto pc = find_if(predicted_state.characters, [&](auto &c) { return c.id == character; });
  if (pc == predicted_state.characters.end())
    return;

  sc->physics = pc->physics;
  //predicted_state = gs;
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
  Buffer b;
  serialize_(b, MOVE_MESSAGE);
  serialize_(b, m);
  serialize_(b, orientation);
  serialize_(b, target_id);
  send_unreliable(b);

  Input command;
  command.number = 0;
  command.orientation = orientation;
  command.m = m;
  last_input = command;
  auto t = find_if(predicted_state.character_targets, [&](auto &t) { return t.character == character; });
  if (t != predicted_state.character_targets.end())
    t->target = target_id;
  else
    predicted_state.character_targets.push_back({character, target_id});
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
