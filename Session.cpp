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

void Local_Session::send_chat_message(std::string_view message) {
  return;
}

std::vector<Chat_Message> Local_Session::get_chat_log()
{
  return {};
}

Network_Session::~Network_Session()
{
  enet_host_destroy(client);
}

void Network_Session::connect(const char *wargspy_address)
{
  client = enet_host_create(
    NULL, /* create a client host */
    1 /* only allow 1 outgoing connection */,
    2 /* allow up 2 channels to be used, 0 and 1 */,
    0 /* assume any amount of incoming bandwidth */,
    0 /* assume any amount of outgoing bandwidth */
  );
  ASSERT(client);

  ENetAddress addr;
  ENetEvent event;

  {
    ENetPeer *wargspy;
    enet_address_set_host(&addr, wargspy_address);
    addr.port = WARGSPY_PORT;
    wargspy = enet_host_connect(client, &addr, 2, 0);

    ASSERT(enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT);

    {
      uint8 byte = 2;
      ENetPacket *packet = enet_packet_create(&byte, 1, 0);
      enet_peer_send(wargspy, 0, packet);
      enet_host_flush(client);
    }

    ENetEvent event;
    enet_host_service(client, &event, 10000);

    ASSERT(event.type == ENET_EVENT_TYPE_RECEIVE);

    Buffer b;
    b.insert(event.packet->data, event.packet->dataLength);

    uint8 type;
    deserialize(b, type);
    if (type == 1)
    {
      uint32 host;
      deserialize(b, host);
      addr.host = host;
      addr.port = GAME_PORT;
    }
    else
    {
      return;
    }

    enet_peer_disconnect_now(wargspy, 0);
  }

  {
    /* Initiate the connection, allocating the two channels 0 and 1. */
    server = enet_host_connect(client, &addr, 2, 0);
    ASSERT(server);

    /* Wait up to 5 seconds for the connection attempt to succeed. */
    int res = enet_host_service(client, &event, 5000);
    ASSERT(res > 0 && event.type == ENET_EVENT_TYPE_CONNECT);
  }
}

void Network_Session::send(Buffer &b)
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
            gs = last_state;
            pc = character;
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
}

void Network_Session::request_spawn(std::string_view name, int32 team)
{
  Buffer b;
  serialize_(b, SPAWN_MESSAGE);
  serialize_(b, name);
  serialize_(b, team);
  send(b);
}

void Network_Session::move_command(int m, quat orientation, UID target_id)
{
  Buffer b;
  serialize_(b, MOVE_MESSAGE);
  serialize_(b, m);
  serialize_(b, orientation);
  serialize_(b, target_id);
  send(b);
}

void Network_Session::try_cast_spell(UID target, UID spell) {
  Buffer b;
  serialize_(b, CAST_MESSAGE);
  serialize_(b, target);
  serialize_(b, spell);
  send(b);
}

void Network_Session::send_chat_message(std::string_view chat_message) {
  Buffer b;
  serialize_(b, CHAT_MESSAGE);
  serialize_(b, chat_message);
  send(b);
}

std::vector<Chat_Message> Network_Session::get_chat_log() {
  return chat_log;
}