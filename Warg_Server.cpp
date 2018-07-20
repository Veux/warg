#include "Third_party/imgui/imgui.h"
#include "Warg_Server.h"
#include <cstring>
#include <memory>

Warg_Server::Warg_Server() : scene(&GL_DISABLED_RESOURCE_MANAGER)
{
  map = make_blades_edge();
  map.node = scene.add_aiscene("Blades Edge", "Blades_Edge/blades_edge.fbx", &map.material);
  collider_cache = collect_colliders(scene);
  // collider_cache.push_back({ {1000, 1000, 0}, {-1000,1000,0}, {-1000,-1000,0} });
  // collider_cache.push_back({ {-1000, -1000, 0}, {1000, -1000, 0}, {1000, 1000, 0} });
  sdb = make_spell_db();
  add_dummy();
}

void Warg_Server::update(float32 dt)
{
  time += dt;
  tick += 1;

  // set_message(s("Server update(). Time:", time, " Tick:", tick, " Tick*dt:", tick * dt), "", 1.0f);

  process_messages();

  bool found_living_dummy = false;
  for (auto &character : game_state.characters)
    if (character.second.name == "Combat Dummy" && character.second.alive)
      found_living_dummy = true;
  if (!found_living_dummy)
  {
    add_dummy();
    set_message("NEEEEEEXTTTT", "", 5);
  }

  PERF_TIMER.start();
  for (auto &c : game_state.characters)
  {
    auto &cid = c.first;
    auto &ch = c.second;

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
      update_buffs(cid, dt);
      ch.apply_modifiers();
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
    peer->push_to_peer(make_unique<State_Message>(
        peer->character, game_state.characters, game_state.spell_objects, tick, peer->last_input.number));
  }

  for (auto i = game_state.spell_objects.begin(); i != game_state.spell_objects.end();)
  {
    Spell_Object *spell_object = &i->second;
    if (update_spell_object(spell_object))
      i = game_state.spell_objects.erase(i);
    else
      i++;
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

bool Warg_Server::update_spell_object(Spell_Object *so)
{
  ASSERT(so->target && game_state.characters.count(so->target));
  Character *target = &game_state.characters[so->target];

  float d = length(target->physics.position - so->pos);

  if (d < 0.3)
  {
    for (auto &e : so->formula.effects)
    {
      Spell_Effect i;
      i.formula = sdb->effects[e];
      i.caster = so->caster;
      i.position = so->pos;
      i.target = so->target;
      invoke_spell_effect(i);
    }
    return true;
  }

  vec3 a = normalize(target->physics.position - so->pos);
  a.x *= so->formula.speed * dt;
  a.y *= so->formula.speed * dt;
  a.z *= so->formula.speed * dt;
  so->pos += a;
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
  ASSERT(server.game_state.characters.count(peer_->character));
  auto &character = server.game_state.characters[peer_->character];

  Input command;
  command.number = input_number;
  command.orientation = orientation;
  command.m = move_status;
  peer_->last_input = command;
  if (server.game_state.characters.count(peer_->character))
    server.game_state.characters[peer_->character].target = target_id;
}

void Cast_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
  ASSERT(server.game_state.characters.count(peer_->character));
  auto &character = server.game_state.characters[peer_->character];

  server.try_cast_spell(character, target, spell.c_str());
}

void Warg_Server::try_cast_spell(Character &caster, UID target_, const char *spell_)
{
  ASSERT(spell_);
  ASSERT(caster.spellbook.count(spell_));
  Spell *spell = &caster.spellbook[spell_];

  Character *target = nullptr;
  if (spell->formula->targets == Spell_Targets::Self)
    target = &caster;
  else if (spell->formula->targets == Spell_Targets::Terrain)
    ASSERT(target_ == 0);
  else if (target_ && game_state.characters.count(target_))
    target = &game_state.characters[target_];

  Cast_Error err;
  if (static_cast<int>(err = cast_viable(caster.id, target_, spell)))
/*    push_all(new Cast_Error_Message(caster.id, target_, spell_, (int)err))*/;
  else
    cast_spell(caster.id, target ? target->id : target_, spell);
}

void Warg_Server::cast_spell(UID caster_, UID target_, Spell *spell)
{
  ASSERT(spell);

  if (spell->formula->cast_time > 0)
    begin_cast(caster_, target_, spell);
  else
    release_spell(caster_, target_, spell);
}

void Warg_Server::begin_cast(UID caster_id, UID target_id, Spell *spell)
{
  ASSERT(spell);
  ASSERT(spell->formula->cast_time > 0);
  ASSERT(get_character(target_id));

  Character *caster = get_character(caster_id);
  ASSERT(caster);

  caster->casting = true;
  caster->casting_spell = spell;
  caster->cast_progress = 0;
  caster->cast_target = target_id;
  if (spell->formula->on_global_cooldown)
    caster->gcd = caster->e_stats.gcd;
}

void Warg_Server::interrupt_cast(UID ci)
{
  ASSERT(ci && game_state.characters.count(ci));
  Character *c = &game_state.characters[ci];

  if (!c->casting)
    return;

  ASSERT(c->casting_spell);

  c->casting = false;
  c->cast_progress = 0;
  if (c->casting_spell->formula->on_global_cooldown)
    c->gcd = 0;
  c->casting_spell = nullptr;
  //std::shared_ptr<Cast_Interrupt_Message> message = new Cast_Interrupt_Message(ci);
  //push_all(message);
}

void Warg_Server::update_cast(UID caster_id, float32 dt)
{
  Character *caster = get_character(caster_id);
  ASSERT(caster);

  if (!caster->casting)
    return;

  ASSERT(caster->casting_spell);
  ASSERT(caster->casting_spell->formula);

  caster->cast_progress += caster->e_stats.cast_speed * dt;
  if (caster->cast_progress >= caster->casting_spell->formula->cast_time)
  {
    set_message(s("releasing cast on tick ", tick), "", 20);

    caster->cast_progress = 0;
    caster->casting = false;
    release_spell(caster_id, caster->cast_target, caster->casting_spell);
    caster->cast_target = 0;
    caster->casting_spell = nullptr;
  }
}

Cast_Error Warg_Server::cast_viable(UID caster_id, UID target_id, Spell *spell)
{
  ASSERT(spell);
  ASSERT(spell->formula);

  Character *caster = get_character(caster_id);
  ASSERT(caster);

  Character *target = get_character(target_id);
  if (spell->formula->targets == Spell_Targets::Self)
    target = caster;

  if (!target)
    return Cast_Error::Invalid_Target;
  if (!caster->alive || !target->alive)
    return Cast_Error::Invalid_Target;
  if (caster->silenced)
    return Cast_Error::Silenced;
  if (caster->gcd > 0 && spell->formula->on_global_cooldown)
    return Cast_Error::Global_Cooldown;
  if (spell->cooldown_remaining > 0)
    return Cast_Error::Cooldown;
  if (spell->formula->mana_cost > caster->mana)
    return Cast_Error::Insufficient_Mana;
  if (!target && spell->formula->targets != Spell_Targets::Terrain)
    return Cast_Error::Invalid_Target;
  if (target && length(caster->physics.position - target->physics.position) > spell->formula->range &&
      spell->formula->targets != Spell_Targets::Terrain && spell->formula->targets != Spell_Targets::Self)
    return Cast_Error::Out_of_Range;
  if (caster->casting)
    return Cast_Error::Already_Casting;

  return Cast_Error::Success;
}

void Warg_Server::release_spell(UID caster_, UID target_, Spell *spell)
{
  ASSERT(game_state.characters.count(caster_));
  ASSERT(spell);
  ASSERT(spell->formula);

  Character *caster = &game_state.characters[caster_];
  Character *target = (target_ && game_state.characters.count(target_)) ? &game_state.characters[target_] : nullptr;

  Cast_Error err;
  if (static_cast<int>(err = cast_viable(caster_, target_, spell)))
  {
    //push_all(make_unique<Cast_Error_Message>(caster_, target_, spell->formula->name.c_str(), static_cast<int>(err)));
    return;
  }

  for (auto &e : spell->formula->effects)
  {
    Spell_Effect i;
    i.formula = sdb->effects[e];
    i.caster = caster_;
    i.position = caster->physics.position;
    i.target = target_;

    invoke_spell_effect(i);
  }

  if (!spell->formula->cast_time && spell->formula->on_global_cooldown)
    caster->gcd = caster->e_stats.gcd;
  spell->cooldown_remaining = spell->formula->cooldown;
}

void Warg_Server::invoke_spell_effect(Spell_Effect &effect)
{
  switch (effect.formula.type)
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
  ASSERT(effect.caster && game_state.characters.count(effect.caster));

  for (size_t ch = 0; ch < game_state.characters.size(); ch++)
  {
    Character *c = &game_state.characters[ch];

    bool in_range = length(c->physics.position - effect.position) <= effect.formula.area.radius;
    bool at_ally =
        effect.formula.area.targets == Spell_Targets::Ally && c->team == game_state.characters[effect.caster].team;
    bool at_hostile =
        effect.formula.area.targets == Spell_Targets::Hostile && c->team != game_state.characters[effect.caster].team;

    if (in_range && (at_ally || at_hostile))
    {
      Spell_Effect i;
      i.formula = sdb->effects[effect.formula.area.effect_formula];
      i.caster = effect.caster;
      i.position = {0, 0, 0};
      i.target = ch;

      invoke_spell_effect(i);
    }
  }
}

void Warg_Server::invoke_spell_effect_apply_buff(Spell_Effect &effect)
{
  ASSERT(effect.target && game_state.characters.count(effect.target));
  Character *target = &game_state.characters[effect.target];

  bool is_buff = effect.formula.type == Spell_Effect_Type::Apply_Buff;

  Buff buff;
  buff.def = sdb->buffs[is_buff ? effect.formula.apply_buff.buff_formula : effect.formula.apply_debuff.debuff_formula];
  buff.duration = buff.def.duration;
  buff.time_since_last_tick = 0.f;

  if (is_buff)
    target->buffs.push_back(buff);
  else
    target->debuffs.push_back(buff);
}

void Warg_Server::invoke_spell_effect_clear_debuffs(Spell_Effect &effect)
{
  ASSERT(effect.caster && game_state.characters.count(effect.caster));
  Character *c = &game_state.characters[effect.target];
  c->debuffs.clear();
}

void Warg_Server::invoke_spell_effect_damage(Spell_Effect &effect)
{
  ASSERT(effect.target && game_state.characters.count(effect.target));
  Character *target = &game_state.characters[effect.target];

  if (!target->alive)
    return;

  Damage_Effect *d = &effect.formula.damage;

  int effective = d->amount;
  int overkill = 0;

  if (!d->pierce_mod)
    effective = (int)round(effective * target->e_stats.damage_mod);

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
  ASSERT(effect.target && game_state.characters.count(effect.target));
  Character *target = &game_state.characters[effect.target];
  ASSERT(target->alive);

  Heal_Effect *h = &effect.formula.heal;

  int effective = h->amount;
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
  ASSERT(effect.target && game_state.characters.count(effect.target));
  Character *target = &game_state.characters[effect.target];

  if (!target->casting)
    return;

  target->casting = false;
  target->casting_spell = nullptr;
  target->cast_progress = 0;
  target->cast_target = 0;

  //push_all(make_unique<Cast_Interrupt_Message>(effect.target));
}

void Warg_Server::invoke_spell_effect_object_launch(Spell_Effect &effect)
{
  Spell_Object obji;
  obji.formula = sdb->objects[effect.formula.object_launch.object_formula];
  obji.caster = effect.caster;
  obji.target = effect.target;
  Character *caster = &game_state.characters[effect.caster];
  Character *target = &game_state.characters[effect.target];
  vec3 launch_pos = caster->physics.position +
                    normalize(target->physics.position - caster->physics.position) * caster->radius.y * 1.5f;
  obji.pos = effect.position;
  game_state.spell_objects[uid()] = obji;

  set_message(s("spawning spell object on tick ", tick), "", 20);
}

void Warg_Server::invoke_spell_effect_blink(Spell_Effect &effect)
{
  ASSERT(game_state.characters.count(effect.caster));
  Character *caster = &game_state.characters[effect.caster];

  vec3 dir = caster->physics.orientation * vec3(0, 1, 0);
  vec3 delta = normalize(dir) * effect.formula.blink.distance;
  collide_and_slide_char(caster->physics, caster->radius, delta, vec3(0, 0, -100), collider_cache);
}

void Warg_Server::update_buffs(UID character, float32 dt)
{
  ASSERT(character && game_state.characters.count(character));
  Character *ch = &game_state.characters[character];

  auto update_buffs_ = [&](std::vector<Buff> &buffs) {
    for (auto b = buffs.begin(); b != buffs.end();)
    {
      b->duration -= dt;
      b->time_since_last_tick += dt;
      if (b->time_since_last_tick > 1.f / b->def.tick_freq)
      {
        for (auto &e : b->def.tick_effects)
        {
          Spell_Effect i;
          i.formula = sdb->effects[e];
          i.caster = 0;
          i.target = character;
          i.position = {0, 0, 0};

          invoke_spell_effect(i);
        }
        b->time_since_last_tick = 0;
      }
      if (b->duration <= 0)
        b = buffs.erase(b);
      else
        b++;
    }
  };

  update_buffs_(ch->buffs);
  update_buffs_(ch->debuffs);
}

void Warg_Server::update_target(UID ch)
{
  ASSERT(ch && game_state.characters.count(ch));
  Character *c = &game_state.characters[ch];

  if (!c->target)
    return;

  ASSERT(game_state.characters.count(c->target));
  Character *target = &game_state.characters[c->target];

  if (!target->alive)
    c->target = 0;
}

UID Warg_Server::add_dummy()
{
  UID id = uid();

  Character c;
  c.id = id;
  c.team = 2;
  c.name = "Combat Dummy";
  c.physics.position = {1, 0, 15};
  c.physics.orientation = map.spawn_orientation[0];
  c.hp_max = 50;
  c.hp = c.hp_max;
  c.mana_max = 10000;
  c.mana = c.mana_max;

  CharStats s;
  s.gcd = 1.5;
  s.speed = 0.0;
  s.cast_speed = 1;
  s.hp_regen = 0;
  s.mana_regen = 1000;
  s.damage_mod = 1;
  s.atk_dmg = 0;
  s.atk_dmg = 1;

  c.b_stats = s;
  c.e_stats = s;

  game_state.characters[id] = c;

  return id;
}

UID Warg_Server::add_char(int team, const char *name)
{
  ASSERT(0 <= team && team < 2);
  ASSERT(name);

  UID id = uid();

  Character c;
  c.id = id;
  c.team = team;
  c.name = std::string(name);
  c.physics.position = map.spawn_pos[team];
  c.physics.orientation = map.spawn_orientation[team];
  c.hp_max = 100;
  c.hp = c.hp_max;
  c.mana_max = 500;
  c.mana = c.mana_max;

  CharStats s;
  s.gcd = 1.5;
  s.speed = MOVE_SPEED;
  s.cast_speed = 1;
  s.hp_regen = 2;
  s.mana_regen = 10;
  s.damage_mod = 1;
  s.atk_dmg = 5;
  s.atk_speed = 2;

  c.b_stats = s;
  c.e_stats = s;

  for (size_t i = 0; i < sdb->spells.size(); i++)
  {
    Spell s;
    s.formula = &sdb->spells[i];
    s.cooldown_remaining = 0;
    c.spellbook[s.formula->name] = s;
  }

  game_state.characters[id] = c;

  return id;
}

void Warg_Server::connect(std::shared_ptr<Peer> peer)
{
  peers[uid()] = peer;
}

Character *Warg_Server::get_character(UID id)
{
  return game_state.characters.count(id) ? &game_state.characters[id] : nullptr;
}