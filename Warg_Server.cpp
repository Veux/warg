#include "stdafx.h"
#include "Warg_Server.h"

UID uid_of(const ENetPeer const *peer)
{
  return peer ? (UID)peer->data : 0;
}

void game_server(const char *wargspy_address)
{
  ENetHost *server;
  ENetPeer *wargspy;
  Game_State game_state;
  std::vector<Chat_Message> chat_log;
  std::unique_ptr<Map> map;
  std::map<UID, Peer> peers;

  auto resource_manager = new Resource_Manager();
  auto scene = new Scene_Graph(resource_manager);

  {
    map = std::make_unique<Map>(Blades_Edge(*scene));
    for (int i = 0; i < 1; i++)
      add_dummy(game_state, *map, {1, i, 5});
  }

  {
    ENetAddress addr = { .host = ENET_HOST_ANY, .port = GAME_PORT };
    server = enet_host_create(&addr, 32, 2, 0, 0);
  }

  {
    ENetAddress addr = { .port = WARGSPY_PORT };
    enet_address_set_host(&addr, wargspy_address);
    wargspy = enet_host_connect(server, &addr, 2, 0);
  }

  {
    ENetEvent event;
    enet_host_service(server, &event, 5000);
    // > 0 && event.type == ENET_EVENT_TYPE_CONNECT);
  }
  
  {
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
      
        {
          uint8 byte = 1;
          ENetPacket *packet = enet_packet_create(&byte, 1, 0);
          enet_peer_send(wargspy, 0, packet);
        }

        {
          ENetEvent event;
          while (enet_host_service(server, &event, 0) > 0)
          {
            switch (event.type)
            {
              case ENET_EVENT_TYPE_CONNECT:
              {
                UID peer_id = uid();
                event.peer->data = (void *)peer_id;
                peers[peer_id].peer = event.peer;
                break;
              }
              
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
                    Peer &peer = peers[uid_of(event.peer)];
                    if (none_of(game_state.living_characters, [&](auto& lc) { return lc.id == peer.character; }))
                    {
                      UID character = add_char(game_state, *map, team, name);
                      peer.character = character;
                    }
                    break;
                  }
                  case MOVE_MESSAGE:
                  {
                    uint32 input_number;
                    int m;
                    quat orientation;
                    UID target_id;
                    deserialize(b, input_number);
                    deserialize(b, m);
                    deserialize(b, orientation);
                    deserialize(b, target_id);
                    Input command;
                    command.sequence_number = input_number;
                    command.orientation = orientation;
                    command.m = m;
                    Peer &peer = peers[uid_of(event.peer)];
                    if (peer.last_input.sequence_number < command.sequence_number)
                    {
                      UID character = peer.character;
                      peer.last_input = command;
                      auto t = find_if(game_state.character_targets, [&](auto &t) { return t.character == character; });
                      if (t != game_state.character_targets.end())
                        t->target = target_id;
                      else
                        game_state.character_targets.push_back({character, target_id});
                    }
                    break;
                  }
                  case CAST_MESSAGE:
                  {
                    UID target, spell;
                    deserialize(b, target);
                    deserialize(b, spell);
                    Peer &peer = peers[uid_of(event.peer)];
                    UID character = peer.character;
                    cast_spell(game_state, *scene, character, target, spell);
                    break;
                  }
                  case CHAT_MESSAGE:
                  {
                    std::string chat_message;
                    deserialize(b, chat_message);
                    Peer &peer = peers[uid_of(event.peer)];
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
                    }
                  }
                }

                enet_packet_destroy(event.packet);
                break;
              }

              case ENET_EVENT_TYPE_DISCONNECT:
              {
                peers.erase(uid_of(event.peer));
                break;
              }
            }
          }
        }

        {
          std::map<UID, Input> inputs;
          for (auto &p : peers)
            inputs[p.second.character] = p.second.last_input;
          update_game(game_state, *map, *scene, inputs);
        }

        for (auto &p : peers)
        {
          Buffer b;
          serialize_(b, STATE_MESSAGE);
          serialize_(b, game_state);
          serialize_(b, p.second.character);
          serialize_(b, p.second.last_input.sequence_number);

          ENetPacket *packet = enet_packet_create(b.data.data(), b.data.size(), ENET_PACKET_FLAG_UNSEQUENCED);
          enet_peer_send(p.second.peer, 0, packet);
        }
        enet_host_flush(server);
      }
      SDL_Delay(5);
    }
  }

  enet_host_destroy(server);
  delete scene;
  delete resource_manager;
}
