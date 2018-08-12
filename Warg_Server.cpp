#include "stdafx.h"
#include "Warg_Server.h"

Warg_Server::Warg_Server() : scene(&resource_manager)
{
  resource_manager.init();
  map = new Blades_Edge(scene);
  spell_db = Spell_Database();
  add_dummy();
}

Warg_Server::~Warg_Server()
{
  delete map;
}

void Warg_Server::update(float32 dt)
{
  time += dt;
  tick += 1;

  // set_message(s("Server update(). Time:", time, " Tick:", tick, " Tick*dt:", tick * dt), "", 1.0f);

  process_messages();

  set_message("server character count:", s(game_state.character_count), 5);

  bool found_living_dummy = false;
  for (size_t i = 0; i < game_state.character_count; i++)
  {
    Character *character = &game_state.characters[i];
    if (std::string(character->name) == "Combat Dummy" && character->alive)
      found_living_dummy = true;
  }
  if (!found_living_dummy)
  {
    add_dummy();
    set_message("NEEEEEEXTTTT", "", 5);
  }

  PERF_TIMER.start();
  for (size_t i = 0; i < game_state.character_count; i++)
  {
    auto &cid = game_state.characters[i].id;
    auto &ch = game_state.characters[i];

    Input last_input;
    for (auto &peer_ : peers)
    {
      std::shared_ptr<Peer> peer = peer_.second;
      if (peer->character == cid)
        last_input = peer->last_input;
    }

    vec3 old_pos = ch.physics.position;
    move_char(ch, last_input, map->colliders);
    if (_isnan(ch.physics.position.x) || _isnan(ch.physics.position.y) || _isnan(ch.physics.position.z))
      ch.physics.position = map->spawn_pos[ch.team];
    if (ch.alive)
    {
      if (ch.physics.position != old_pos)
        interrupt_cast(cid);
      update_buffs(cid);
      update_cast(cid, dt);
      update_target(cid);
      ch.update_hp(dt);
      ch.update_mana(dt);
      ch.update_spell_cooldowns(dt);
      ch.update_global_cooldown(dt);
    }
    else
    {
      ch.physics.orientation = angleAxis(-half_pi<float32>(), vec3(1.f, 0.f, 0.f));
    }
  }
  PERF_TIMER.stop();

  for (auto &p : peers)
  {
    shared_ptr<Peer> peer = p.second;
    peer->push_to_peer(make_unique<State_Message>(peer->character, &game_state));
  }

  for (size_t i = 0; i < game_state.spell_object_count; i++)
  {
    Spell_Object *spell_object = &game_state.spell_objects[i];
    if (update_spell_object(spell_object))
    {
      Spell_Object *last_object = &game_state.spell_objects[game_state.spell_object_count - 1];
      game_state.spell_objects[i] = *last_object;
      game_state.spell_object_count--;
    }
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

bool Warg_Server::update_spell_object(Spell_Object *spell_object)
{
  Spell_Object_Formula *object_formula = spell_db.get_spell_object(spell_object->formula_index);

  ASSERT(spell_object->target);
  Character *target = get_character(spell_object->target);
  ASSERT(target);

  vec3 a = normalize(target->physics.position - spell_object->pos);
  a *= vec3(object_formula->speed * dt);
  spell_object->pos += a;

  float d = length(target->physics.position - spell_object->pos);
  float epsilon = object_formula->speed * dt / 2.f * 1.05f;
  if (d < epsilon)
  {
    spell_object_on_hit_dispatch(object_formula, spell_object, &game_state, &map->colliders);
    return true;
  }

  return false;
}

void Char_Spawn_Request_Message::handle(Warg_Server &server)
{
  UID character = server.add_char(team, name.c_str());
  ASSERT(server.peers.count(peer));
  server.peers[peer]->character = character;
}

void Input_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
  Character *character = server.get_character(peer_->character);
  ASSERT(character);

  Input command;
  command.number = input_number;
  command.orientation = orientation;
  command.m = move_status;
  peer_->last_input = command;
  character->target_id = target_id;
}

void Cast_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
  Character *character = server.get_character(peer_->character);
  ASSERT(character);

  server.try_cast_spell(*character, _target_id, _spell_index);
}

void Warg_Server::try_cast_spell(Character &caster, UID target_id, Spell_Index spell_formula_index)
{
  Spell_Formula *spell_formula = spell_db.get_spell(spell_formula_index);

  bool caster_has_spell = false;
  Spell_Status *spell_status = nullptr;
  for (size_t i = 0; i < caster.spell_set.spell_count; i++)
    if (caster.spell_set.spell_statuses[i].formula_index == spell_formula_index)
    {
      caster_has_spell = true;
      spell_status = &caster.spell_set.spell_statuses[i];
    }
  if (!caster_has_spell)
    return;

  Character *target = nullptr;
  if (spell_formula->targets == Spell_Targets::Self)
    target = &caster;
  else if (spell_formula->targets == Spell_Targets::Terrain)
    ASSERT(target_id == 0);
  else if (target_id && get_character(target_id))
    target = get_character(target_id);

  Cast_Error err;
  if (static_cast<int>(err = cast_viable(caster.id, target_id, spell_status, false)))
    /*    push_all(new Cast_Error_Message(caster.id, target_, spell_, (int)err))*/;
  else
    cast_spell(caster.id, target ? target->id : 0, spell_status);
}

void Warg_Server::cast_spell(UID caster_id, UID target_id, Spell_Status *spell_status)
{
  ASSERT(spell_status);

  Character *caster = game_state.get_character(caster_id);
  caster->cast_target = target_id;

  Spell_Formula *formula = spell_db.get_spell(spell_status->formula_index);
  if (formula->cast_time > 0)
    begin_cast(caster_id, target_id, spell_status);
  else
    release_spell(caster_id, target_id, spell_status);
}

void Warg_Server::begin_cast(UID caster_id, UID target_id, Spell_Status *spell_status)
{
  ASSERT(spell_status);
  Spell_Formula *formula = spell_db.get_spell(spell_status->formula_index);
  ASSERT(formula->cast_time > 0);
  ASSERT(get_character(target_id));

  Character *caster = get_character(caster_id);
  ASSERT(caster);

  caster->casting = true;
  caster->casting_spell_status_index = spell_status->index;
  caster->cast_progress = 0;
  caster->cast_target = target_id;
  if (formula->on_global_cooldown)
    caster->global_cooldown = caster->effective_stats.global_cooldown;
}

void Warg_Server::interrupt_cast(UID character_id)
{
  ASSERT(character_id);
  Character *character = get_character(character_id);
  ASSERT(character);

  if (!character->casting)
    return;

  ASSERT(character->casting_spell_status_index < character->spell_set.spell_count);
  Spell_Status *casting_spell_status = &character->spell_set.spell_statuses[character->casting_spell_status_index];
  Spell_Formula *casting_spell_formula = spell_db.get_spell(casting_spell_status->formula_index);

  character->casting = false;
  character->cast_progress = 0;
  if (casting_spell_formula->on_global_cooldown)
    character->global_cooldown = 0;
}

void Warg_Server::update_cast(UID caster_id, float32 dt)
{
  Character *caster = get_character(caster_id);
  ASSERT(caster);

  if (!caster->casting)
    return;

  ASSERT(caster->casting_spell_status_index < caster->spell_set.spell_count);
  Spell_Status *casting_spell_status = &caster->spell_set.spell_statuses[caster->casting_spell_status_index];
  ASSERT(casting_spell_status);

  Spell_Formula *formula = spell_db.get_spell(casting_spell_status->formula_index);
  caster->cast_progress += caster->effective_stats.cast_speed * dt;
  if (caster->cast_progress >= formula->cast_time)
  {
    set_message(s("releasing cast on tick ", tick), "", 20);

    caster->cast_progress = 0;
    caster->casting = false;
    release_spell(caster_id, caster->cast_target, casting_spell_status);
    caster->cast_target = 0;
  }
}

Cast_Error Warg_Server::cast_viable(UID caster_id, UID target_id, Spell_Status *spell_status, bool ignore_gcd)
{
  ASSERT(spell_status);
  Spell_Formula *spell_formula = spell_db.get_spell(spell_status->formula_index);
  ASSERT(spell_formula);

  Character *caster = get_character(caster_id);
  ASSERT(caster);

  Character *target = get_character(target_id);
  if (spell_formula->targets == Spell_Targets::Self)
    target = caster;

  if (!target)
    return Cast_Error::Invalid_Target;
  if (!caster->alive || !target->alive)
    return Cast_Error::Invalid_Target;
  if (caster->silenced)
    return Cast_Error::Silenced;
  if (!ignore_gcd && caster->global_cooldown > 0 && spell_formula->on_global_cooldown)
    return Cast_Error::Global_Cooldown;
  if (spell_status->cooldown_remaining > 0)
    return Cast_Error::Cooldown;
  if (spell_formula->mana_cost > caster->mana)
    return Cast_Error::Insufficient_Mana;
  if (!target && spell_formula->targets != Spell_Targets::Terrain)
    return Cast_Error::Invalid_Target;
  if (target && length(caster->physics.position - target->physics.position) > spell_formula->range &&
      spell_formula->targets != Spell_Targets::Terrain && spell_formula->targets != Spell_Targets::Self)
    return Cast_Error::Out_of_Range;
  if (caster->casting)
    return Cast_Error::Already_Casting;

  return Cast_Error::Success;
}

void Warg_Server::release_spell(UID caster_id, UID target_id, Spell_Status *spell_status)
{
  ASSERT(spell_status);
  Spell_Formula *spell_formula = spell_db.get_spell(spell_status->formula_index);
  ASSERT(spell_formula);

  Character *caster = get_character(caster_id);
  ASSERT(caster);

  Character *target = get_character(target_id);

  Cast_Error err;
  if (static_cast<int>(err = cast_viable(caster_id, target_id, spell_status, true)))
    return;

  spell_on_release_dispatch(spell_formula, &game_state, caster, &map->colliders);

  if (!spell_formula->cast_time && spell_formula->on_global_cooldown)
    caster->global_cooldown = caster->effective_stats.global_cooldown;
  spell_status->cooldown_remaining = spell_formula->cooldown;
}

void Warg_Server::update_buffs(UID character_id)
{
  ASSERT(character_id);
  Character *character = get_character(character_id);
  ASSERT(character);

  character->effective_stats = character->base_stats;
  character->silenced = false;

  auto update_buffs_ = [&](Buff *buffs, uint8 *buff_count) {
    for (size_t i = 0; i < *buff_count; i++)
    {
      Buff *buff = buffs + i;
      BuffDef *buff_formula = spell_db.get_buff(buff->formula_index);
      buff->duration -= dt;
      buff->time_since_last_tick += dt;
      character->effective_stats *= buff_formula->stats_modifiers;
      if (buff->time_since_last_tick > 1.f / buff_formula->tick_freq)
      {
        buff_on_tick_dispatch(buff_formula, buff, &game_state, character);
        buff->time_since_last_tick = 0;
      }
      if (buff->duration <= 0)
      {
        buff_on_end_dispatch(buff_formula, buff, &game_state, character);
        *buff = buffs[*buff_count - 1];
        (*buff_count)--;
      }
    }
  };

  update_buffs_(character->buffs, &character->buff_count);
  update_buffs_(character->debuffs, &character->debuff_count);
}

void Warg_Server::update_target(UID character_id)
{
  ASSERT(character_id);
  Character *character = get_character(character_id);
  ASSERT(character);

  if (!character->target_id)
    return;

  Character *target = get_character(character->target_id);
  ASSERT(target);

  if (!target->alive)
    character->target_id = 0;
}

UID Warg_Server::add_dummy()
{
  UID id = uid();

  ASSERT(game_state.character_count < MAX_CHARACTERS);

  Character *dummy = &game_state.characters[game_state.character_count++];
  dummy->id = id;
  dummy->team = 2;
  strcpy(dummy->name, "Combat Dummy");
  dummy->physics.position = {1, 0, 15};
  dummy->physics.orientation = map->spawn_orientation[0];
  dummy->hp_max = 50;
  dummy->hp = dummy->hp_max;
  dummy->mana_max = 10000;
  dummy->mana = dummy->mana_max;

  Character_Stats stats;
  stats.global_cooldown = 1.5;
  stats.speed = 0.0;
  stats.cast_speed = 1;
  stats.hp_regen = 0;
  stats.mana_regen = 1000;
  stats.damage_mod = 1;
  stats.atk_dmg = 0;
  stats.atk_dmg = 1;

  dummy->base_stats = stats;
  dummy->effective_stats = stats;

  return id;
}

UID Warg_Server::add_char(int team, const char *name)
{
  ASSERT(0 <= team && team < 2);
  ASSERT(name);
  ASSERT(game_state.character_count < MAX_CHARACTERS);

  UID id = uid();

  Character *character = &game_state.characters[game_state.character_count++];
  character->id = id;
  character->team = team;
  strncpy(character->name, name, MAX_CHARACTER_NAME_LENGTH);
  character->physics.position = map->spawn_pos[team];
  character->physics.orientation = map->spawn_orientation[team];
  character->hp_max = 100;
  character->hp = character->hp_max;
  character->mana_max = 500;
  character->mana = character->mana_max;

  Character_Stats stats;
  stats.global_cooldown = 1.5;
  stats.speed = MOVE_SPEED;
  stats.cast_speed = 1;
  stats.hp_regen = 2;
  stats.mana_regen = 10;
  stats.damage_mod = 1;
  stats.atk_dmg = 5;
  stats.atk_speed = 2;

  character->base_stats = stats;
  character->effective_stats = stats;

  auto add_spell = [&](const char *name) {
    Spell_Status *spell_status = &character->spell_set.spell_statuses[character->spell_set.spell_count];
    spell_status->cooldown_remaining = 0.f;
    spell_status->formula_index = spell_db.get_spell(name)->index;
    spell_status->index = character->spell_set.spell_count;
    character->spell_set.spell_count++;
  };

  add_spell("Frostbolt");
  add_spell("Blink");
  add_spell("Seed of Corruption");
  add_spell("Icy Veins");
  add_spell("Sprint");
  add_spell("Demonic Circle: Summon");
  add_spell("Demonic Circle: Teleport");

  return id;
}

void Warg_Server::connect(std::shared_ptr<Peer> peer)
{
  peers[uid()] = peer;
}

Character *Warg_Server::get_character(UID id)
{
  for (size_t i = 0; i < game_state.character_count; i++)
  {
    Character *character = &game_state.characters[i];
    if (character->id == id)
      return character;
  }
  return nullptr;
}