#include "stdafx.h"
#include "Warg_Server.h"

Warg_Server::Warg_Server() : scene(&resource_manager)
{
  resource_manager.init();
  map = make_blades_edge();
  map.node = scene.add_aiscene("Blades Edge", "Blades_Edge/blades_edge.fbx", &map.material);
  collider_cache = collect_colliders(scene);
  // collider_cache.push_back({ {1000, 1000, 0}, {-1000,1000,0}, {-1000,-1000,0} });
  // collider_cache.push_back({ {-1000, -1000, 0}, {1000, -1000, 0}, {1000, 1000, 0} });
  spell_db = Spell_Database();
  add_dummy();
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
    move_char(ch, last_input, collider_cache);
    if (_isnan(ch.physics.position.x) || _isnan(ch.physics.position.y) || _isnan(ch.physics.position.z))
      ch.physics.position = map.spawn_pos[ch.team];
    if (ch.alive)
    {
      if (ch.physics.position != old_pos)
        interrupt_cast(cid);
      update_buffs(cid);
      apply_character_modifiers(&ch);
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
    for (Spell_Index i : object_formula->effects)
    {
      Spell_Effect_Formula *spell_effect_formula = spell_db.get_spell_effect(i);
      Spell_Effect effect;
      effect.formula_index = spell_effect_formula->index;
      effect.caster = spell_object->caster;
      effect.position = spell_object->pos;
      effect.target = spell_object->target;
      invoke_spell_effect(effect);
    }
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
  if (static_cast<int>(err = cast_viable(caster.id, target_id, spell_status)))
    /*    push_all(new Cast_Error_Message(caster.id, target_, spell_, (int)err))*/;
  else
    cast_spell(caster.id, target ? target->id : 0, spell_status);
}

void Warg_Server::cast_spell(UID caster_id, UID target_id, Spell_Status *spell_status)
{
  ASSERT(spell_status);

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

Cast_Error Warg_Server::cast_viable(UID caster_id, UID target_id, Spell_Status *spell_status)
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
  if (caster->global_cooldown > 0 && spell_formula->on_global_cooldown)
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
  if (static_cast<int>(err = cast_viable(caster_id, target_id, spell_status)))
    return;

  for (Spell_Index effect_formula_index : spell_formula->effects)
  {
    Spell_Effect effect;
    effect.formula_index = effect_formula_index;
    effect.caster = caster_id;
    effect.position = caster->physics.position;
    effect.target = target_id;

    invoke_spell_effect(effect);
  }

  if (!spell_formula->cast_time && spell_formula->on_global_cooldown)
    caster->global_cooldown = caster->effective_stats.global_cooldown;
  spell_status->cooldown_remaining = spell_formula->cooldown;
}

void Warg_Server::invoke_spell_effect(Spell_Effect &effect)
{
  Spell_Effect_Formula *effect_formula = spell_db.get_spell_effect(effect.formula_index);
  switch (effect_formula->type)
  {
    case Spell_Effect_Type::Area:
      invoke_spell_effect_aoe(effect);
      break;
    case Spell_Effect_Type::Apply_Buff:
    case Spell_Effect_Type::Apply_Debuff:
      invoke_spell_effect_apply_buff(effect);
      break;
    case Spell_Effect_Type::Clear_Debuffs:
      invoke_spell_effect_clear_debuffs(effect);
      break;
    case Spell_Effect_Type::Damage:
      invoke_spell_effect_damage(effect);
      break;
    case Spell_Effect_Type::Heal:
      invoke_spell_effect_heal(effect);
      break;
    case Spell_Effect_Type::Interrupt:
      invoke_spell_effect_interrupt(effect);
      break;
    case Spell_Effect_Type::Object_Launch:
      invoke_spell_effect_object_launch(effect);
      break;
    case Spell_Effect_Type::Blink:
      invoke_spell_effect_blink(effect);
      break;
    default:
      ASSERT(false);
  }
}

void Warg_Server::invoke_spell_effect_aoe(Spell_Effect &effect)
{
  ASSERT(effect.caster);
  Character *caster = get_character(effect.caster);
  ASSERT(caster);

  Spell_Effect_Formula *formula = spell_db.get_spell_effect(effect.formula_index);

  for (size_t i = 0; i < game_state.character_count; i++)
  {
    Character *character = &game_state.characters[i];

    bool in_range = length(character->physics.position - effect.position) <= formula->area.radius;
    if (!in_range)
      continue;
    bool at_ally = formula->area.targets == Spell_Targets::Ally && character->team == caster->team;
    bool at_hostile = formula->area.targets == Spell_Targets::Hostile && character->team != caster->team;
    if (!(at_ally || at_hostile))
      continue;

    Spell_Effect effect;
    effect.formula_index = formula->area.effect_formula;
    effect.caster = effect.caster;
    effect.position = {0, 0, 0};
    effect.target = i;

    invoke_spell_effect(effect);
  }
}

void Warg_Server::invoke_spell_effect_apply_buff(Spell_Effect &effect)
{
  ASSERT(effect.target);
  Character *target = get_character(effect.target);
  ASSERT(target);

  Spell_Effect_Formula *formula = spell_db.get_spell_effect(effect.formula_index);

  bool is_buff = formula->type == Spell_Effect_Type::Apply_Buff;

  Buff buff;
  buff.formula_index = is_buff ? formula->apply_buff.buff_formula : formula->apply_debuff.debuff_formula;
  BuffDef *buff_formula = spell_db.get_buff(buff.formula_index);
  buff.duration = buff_formula->duration;
  buff.time_since_last_tick = 0.f;

  if (is_buff && target->buff_count < MAX_BUFFS)
    target->buffs[target->buff_count++] = buff;
  else if (!is_buff && target->debuff_count < MAX_DEBUFFS)
    target->debuffs[target->debuff_count++] = buff;
}

void Warg_Server::invoke_spell_effect_clear_debuffs(Spell_Effect &effect)
{
  ASSERT(effect.caster);
  Character *character = get_character(effect.target);
  ASSERT(character);
  character->debuff_count = 0;
}

void Warg_Server::invoke_spell_effect_damage(Spell_Effect &effect)
{
  ASSERT(effect.target);
  Character *target = get_character(effect.target);
  ASSERT(target);

  if (!target->alive)
    return;

  Spell_Effect_Formula *formula = spell_db.get_spell_effect(effect.formula_index);
  Damage_Effect *damage_effect = &formula->damage;

  int effective = damage_effect->amount;
  int overkill = 0;

  if (!damage_effect->pierce_mod)
    effective = (int)round(effective * target->effective_stats.damage_mod);

  target->hp -= effective;

  if (target->hp <= 0)
  {
    effective += target->hp;
    overkill = -target->hp;
    target->hp = 0;
    target->alive = false;

    float32 new_height = target->radius.y;
    target->physics.grounded = false;
    target->physics.position.z -= target->radius.z - target->radius.y;
  }
}

void Warg_Server::invoke_spell_effect_heal(Spell_Effect &effect)
{
  ASSERT(effect.target);
  Character *target = get_character(effect.target);
  ASSERT(target);

  Spell_Effect_Formula *formula = spell_db.get_spell_effect(effect.formula_index);
  Heal_Effect *heal_effect = &formula->heal;

  int effective = heal_effect->amount;
  int overheal = 0;

  target->hp += effective;

  if (target->hp > target->hp_max)
  {
    overheal = target->hp - target->hp_max;
    effective -= overheal;
    target->hp = target->hp_max;
  }
}

void Warg_Server::invoke_spell_effect_interrupt(Spell_Effect &effect)
{
  ASSERT(effect.target);
  Character *target = get_character(effect.target);
  ASSERT(target);

  if (!target->casting)
    return;

  target->casting = false;
  target->cast_progress = 0;
  target->cast_target = 0;
}

void Warg_Server::invoke_spell_effect_object_launch(Spell_Effect &effect)
{
  Spell_Effect_Formula *effect_formula = spell_db.get_spell_effect(effect.formula_index);
  Spell_Object object;
  object.formula_index = effect_formula->object_launch.object_formula;
  object.caster = effect.caster;
  object.target = effect.target;
  Character *caster = get_character(effect.caster);
  Character *target = get_character(effect.target);

  vec3 launch_pos = caster->physics.position +
                    normalize(target->physics.position - caster->physics.position) * caster->radius.y * 1.5f;
  object.pos = effect.position;
  object.id = uid();
  game_state.spell_objects[game_state.spell_object_count++] = object;
}

void Warg_Server::invoke_spell_effect_blink(Spell_Effect &effect)
{
  Spell_Effect_Formula *formula = spell_db.get_spell_effect(effect.formula_index);

  ASSERT(effect.caster);
  Character *caster = get_character(effect.caster);
  ASSERT(caster);

  vec3 dir = caster->physics.orientation * vec3(0, 1, 0);
  vec3 delta = normalize(dir) * formula->blink.distance;
  collide_and_slide_char(caster->physics, caster->radius, delta, vec3(0, 0, -100), collider_cache);
}

void Warg_Server::update_buffs(UID character_id)
{
  ASSERT(character_id);
  Character *character = get_character(character_id);
  ASSERT(character);

  auto update_buffs_ = [&](Buff *buffs, uint8 *buff_count) {
    for (size_t i = 0; i < *buff_count; i++)
    {
      Buff *buff = buffs + i;
      BuffDef *buff_formula = spell_db.get_buff(buff->formula_index);
      buff->duration -= dt;
      buff->time_since_last_tick += dt;
      if (buff->time_since_last_tick > 1.f / buff_formula->tick_freq)
      {
        for (Spell_Index effect_formula_index : buff_formula->tick_effects)
        {
          Spell_Effect effect;
          effect.formula_index = effect_formula_index;
          effect.caster = 0;
          effect.target = character_id;
          effect.position = {0, 0, 0};

          invoke_spell_effect(effect);
        }
        buff->time_since_last_tick = 0;
      }
      if (buff->duration <= 0)
      {
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
  dummy->physics.orientation = map.spawn_orientation[0];
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
  character->physics.position = map.spawn_pos[team];
  character->physics.orientation = map.spawn_orientation[team];
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
  add_spell("Shadow Word: Pain");
  add_spell("Icy Veins");
  add_spell("Sprint");

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

void Warg_Server::apply_character_modifiers(Character *character)
{
  character->effective_stats = character->base_stats;
  character->silenced = false;

  for (size_t i = 0; i < character->buff_count; i++)
  {
    Buff *buff = &character->buffs[i];
    BuffDef *buff_formula = spell_db.get_buff(buff->formula_index);
    for (size_t j = 0; j < buff_formula->character_modifiers.size(); j++)
    {
      CharMod *character_modifier = spell_db.get_character_modifier(buff_formula->character_modifiers[j]);
      character->apply_modifier(*character_modifier);
    }
  }
  for (size_t i = 0; i < character->debuff_count; i++)
  {
    Buff *debuff = &character->debuffs[i];
    BuffDef *debuff_formula = spell_db.get_buff(debuff->formula_index);
    for (size_t j = 0; j < debuff_formula->character_modifiers.size(); j++)
    {
      CharMod *character_modifier = spell_db.get_character_modifier(debuff_formula->character_modifiers[j]);
      character->apply_modifier(*character_modifier);
    }
  }
}