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
    add_dummy(game_state, map, {1, i, 5});
}

Warg_Server::~Warg_Server()
{
  delete map;
}

void Warg_Server::update(float32 dt)
{
  time += dt;
  tick += 1;

  set_message("Server update() ", s("Time:", time, " Tick:", tick, " Tick*dt:", tick * dt), 1.0f);

  process_messages();

  set_message("server character count:", s(game_state.characters.size()), 5);

  update_game(game_state, map, spell_db, scene, dt);

  for (auto &character : game_state.characters)
  {
    Input last_input;
    for (auto &peer_ : peers)
    {
      std::shared_ptr<Peer> peer = peer_.second;
      if (peer->character == character.id)
        last_input = peer->last_input;
    }
  }

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
    move_char(character, last_input, &scene);
    if (_isnan(character.physics.position.x) || _isnan(character.physics.position.y) ||
        _isnan(character.physics.position.z))
      character.physics.position = map->spawn_pos[character.team];
    if (character.alive)
    {
      if (character.physics.position != old_pos)
        game_state.character_casts.erase(
            std::remove_if(game_state.character_casts.begin(), game_state.character_casts.end(),
                [&](auto &cast) { return cast.caster == character.id; }),
            game_state.character_casts.end());
      update_buffs(game_state, spell_db, character.id);
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
  UID character = add_char(server.game_state, server.map, server.spell_db, team, name.c_str());
  ASSERT(server.peers.count(peer));
  server.peers[peer]->character = character;
}

void Input_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
  Character *character = server.game_state.get_character(peer_->character);
  ASSERT(character);

  Input command;
  command.number = input_number;
  command.orientation = orientation;
  command.m = move_status;
  peer_->last_input = command;
  auto t = std::find_if(server.game_state.character_targets.begin(), server.game_state.character_targets.end(),
      [&](auto &t) { return t.c == character->id; });
  if (t != server.game_state.character_targets.end())
    t->t = target_id;
  else
    server.game_state.character_targets.push_back({character->id, target_id});
}

void Cast_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
  Character *character = server.game_state.get_character(peer_->character);
  ASSERT(character);

  try_cast_spell(server.game_state, server.spell_db, server.scene, peer_->character, _target_id, _spell_index);
}

void Warg_Server::connect(std::shared_ptr<Peer> peer)
{
  peers[uid()] = peer;
}
