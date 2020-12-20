#include "stdafx.h"
#include "Warg_Server.h"

using std::make_unique;
using std::queue;
using std::unique_ptr;
using std::vector;

Warg_Server::Warg_Server() : scene(&resource_manager)
{
  map = new Blades_Edge(scene);
  spell_db = Spell_Database();
  for (int i = 0; i < 1; i++)
    add_dummy(game_state, *map, {1, i, 5});
}

Warg_Server::~Warg_Server()
{
  delete map;
}

void Warg_Server::update()
{
  time += dt;
  tick += 1;

  process_messages();

  update_game(game_state, *map, scene);

  for (auto &character : game_state.characters)
  {
    Input last_input;
    for (auto &peer_ : peers)
    {
      std::shared_ptr<Peer> peer = peer_.second;
      if (peer->character == character.id)
        last_input = peer->last_input;
    }

    vec3 old_pos = character.physics.position;
    move_char(game_state, character, last_input, scene);
    if (_isnan(character.physics.position.x) || _isnan(character.physics.position.y) ||
        _isnan(character.physics.position.z))
      character.physics.position = map->spawn_pos[character.team];
    if (character.physics.position.z < -50)
      character.physics.position = map->spawn_pos[character.team];
    if (any_of(game_state.living_characters, [&](auto &lc) { return lc.id == character.id; }))
    {
      if (character.physics.position != old_pos)
      {
        erase_if(game_state.character_casts, [&](auto &cc) { return cc.caster == character.id; });
        erase_if(game_state.character_gcds, [&](auto &cg) { return cg.character == character.id; });
      }
    }
    else
    {
      character.physics.orientation = angleAxis(-half_pi<float32>(), vec3(1.f, 0.f, 0.f));
    }
  }

  for (auto &p : peers)
  {
    shared_ptr<Peer> peer = p.second;
    peer->push_to_peer(make_unique<State_Message>(peer->character, &game_state));
  }
}

void Warg_Server::process_messages()
{
  for (auto &peer : peers)
  {
    for (auto &message : peer.second->pull_from_peer())
    {
      message->peer = peer.first;
      message->handle(*this);
    }
  }
}

void Char_Spawn_Request_Message::handle(Warg_Server &server)
{
  UID character = add_char(server.game_state, *server.map,  team, name);
  ASSERT(server.peers.count(peer));
  server.peers[peer]->character = character;
}

void Input_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];

  Input command;
  command.number = input_number;
  command.orientation = orientation;
  command.m = move_status;
  peer_->last_input = command;
  auto t = std::find_if(server.game_state.character_targets.begin(), server.game_state.character_targets.end(),
      [&](auto &t) { return t.character == peer_->character; });
  if (t != server.game_state.character_targets.end())
    t->target = target_id;
  else
    server.game_state.character_targets.push_back({peer_->character, target_id});
}

void Cast_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];

  cast_spell(server.game_state, server.scene, peer_->character, _target_id, _spell_id);
}

void Warg_Server::connect(std::shared_ptr<Peer> peer)
{
  peers[uid()] = peer;
}
