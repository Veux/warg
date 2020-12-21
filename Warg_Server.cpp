#include "stdafx.h"
#include "Warg_Server.h"

Warg_Server::Warg_Server() : scene(&resource_manager)
{
  map = std::make_unique<Map>(Blades_Edge(scene));
  for (int i = 0; i < 1; i++)
    add_dummy(game_state, *map, {1, i, 5});
}

Warg_Server::~Warg_Server()
{
  enet_host_destroy(server);
}

void Warg_Server::run(int32 port)
{
  /* Bind the server to the default localhost.     */
  /* A specific host address can be specified by   */
  /* enet_address_set_host (& address, "x.x.x.x"); */
  /* Bind the server to port 1234. */
  addr.host = ENET_HOST_ANY;
  addr.port = 1234;
  server = enet_host_create(
    &addr /* the address to bind the server host to */,
    32 /* allow up to 32 clients and/or outgoing connections */,
    2 /* allow up to 2 channels to be used, 0 and 1 */,
    0 /* assume any amount of incoming bandwidth */,
    0 /* assume any amount of outgoing bandwidth */
  );
  ASSERT(server);
  
  float64 current_time = 0.0;
  float64 last_time = get_real_time() - dt;
  float64 elapsed_time = 0.0;
  while (true)
  {
    const float64 time = get_real_time();
    elapsed_time = time - current_time;
    last_time = current_time;
    while (current_time + dt < last_time + elapsed_time)
    {
      current_time += dt;
      ENetEvent event;
      while (enet_host_service(server, &event, 0) > 0)
      {
        switch (event.type)
        {
          case ENET_EVENT_TYPE_CONNECT:
            printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
            /* Store any relevant client information here. */
            {
              Peer p;
              p.peer = event.peer;
              UID peer_id = uid();
              peers[peer_id] = p;
              event.peer->data = (void *)peer_id;
            }
            break;
          case ENET_EVENT_TYPE_RECEIVE:
            {
              Buffer b;
              b.insert(event.packet->data, event.packet->dataLength);
              int32 message_type;
              deserialize(b, message_type);
              switch (message_type)
              {
                case SPAWN_MESSAGE:
                {
                  std::string name;
                  int32 team;
                  deserialize(b, name);
                  deserialize(b, team);
                  Peer &peer = peers[(UID)event.peer->data];
                  if (none_of(game_state.living_characters, [&](auto& lc) { return lc.id == peer.character; }))
                  {
                    UID character = add_char(game_state, *map, team, name);
                    peer.character = character;
                  }
                  break;
                }
                case MOVE_MESSAGE:
                {
                  int m;
                  quat orientation;
                  UID target_id;
                  deserialize(b, m);
                  deserialize(b, orientation);
                  deserialize(b, target_id);
                  Input command;
                  command.number = 0;
                  command.orientation = orientation;
                  command.m = m;
                  Peer &peer = peers[(UID)event.peer->data];
                  UID character = peer.character;
                  peer.last_input = command;
                  auto t = std::find_if(game_state.character_targets.begin(), game_state.character_targets.end(),
                      [&](auto &t) { return t.character == character; });
                  if (t != game_state.character_targets.end())
                    t->target = target_id;
                  else
                    game_state.character_targets.push_back({character, target_id});
                  break;
                }
                case CAST_MESSAGE:
                {
                  UID target, spell;
                  deserialize(b, target);
                  deserialize(b, spell);
                  Peer &peer = peers[(UID)event.peer->data];
                  UID character = peer.character;
                  cast_spell(game_state, scene, character, target, spell);
                  break;
                }
                case CHAT_MESSAGE:
                {
                  std::string chat_message;
                  deserialize(b, chat_message);
                  Peer &peer = peers[(UID)event.peer->data];
                  auto character = find_if(game_state.characters, [&](auto &c) { return c.id == peer.character; });
                  Chat_Message cm;
                  if (character != game_state.characters.end())
                    cm.name = character->name;
                  else
                    cm.name = "Unknown";
                  cm.message = chat_message;

                  Buffer b;
                  serialize_(b, CHAT_MESSAGE_RELAY);
                  serialize_(b, cm.name);
                  serialize_(b, cm.message);
                  for (auto &p : peers)
                  {
                    ENetPacket *packet = enet_packet_create(b.data.data(), b.data.size(), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(p.second.peer, 0, packet);
                    enet_host_flush(server);
                  }
                }
              }
            }

            enet_packet_destroy(event.packet);

            break;

          case ENET_EVENT_TYPE_DISCONNECT:
            printf("peer %u disconnected.\n", (UID)event.peer->data);
            std::cout << std::endl;
            /* Reset the peer's client information. */
            peers.erase((UID)event.peer->data);
            event.peer->data = NULL;
            break;
        }
      }

      std::map<UID, Input> inputs;
      for (auto &p : peers)
        inputs[p.second.character] = p.second.last_input;
      update_game(game_state, *map, scene, inputs);

      for (auto &p : peers)
      {
        Buffer b;
        serialize_(b, STATE_MESSAGE);
        serialize_(b, game_state);
        serialize_(b, p.second.character);

        ENetPacket *packet = enet_packet_create(b.data.data(), b.data.size(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(p.second.peer, 0, packet);
        enet_host_flush(server);
      }
    }
    SDL_Delay(5);
  }
  std::cout << "closing like an idiot " << std::endl;
}