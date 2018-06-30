#include "Warg_Server.h"
#include "Third_party/imgui/imgui.h"
#include <cstring>
#include <memory>

Warg_Server::Warg_Server(bool local)
{
  this->local = local;
  if (!local)
  {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 1337;

    server = enet_host_create(&address, 32, 2, 0, 0);
    ASSERT(server);
  }
  map = make_blades_edge();
  map.node = scene.add_aiscene("Blades_Edge/blades_edge.fbx", nullptr, &map.material);
  collider_cache = collect_colliders(scene);
  sdb = make_spell_db();
}

void Warg_Server::process_packets()
{
  ASSERT(server);

  ENetEvent event;
  while (enet_host_service(server, &event, 0) > 0)
  {
    switch (event.type)
    {
      case ENET_EVENT_TYPE_NONE:
        break;
      case ENET_EVENT_TYPE_CONNECT:
      {
        UID id = uid();
        peers[id] = {event.peer, nullptr, nullptr, 0};

        for (auto &c : chars)
        {
          unique_ptr<Message> char_spawn_msg =
              make_unique<Char_Spawn_Message>(c.first, c.second.name.c_str(), c.second.team);
          send_event(peers[id], char_spawn_msg);
        }

        break;
      }
      case ENET_EVENT_TYPE_RECEIVE:
      {
        Buffer b;
        b.insert((void *)event.packet->data, event.packet->dataLength);
        auto ev = deserialize_message(b);
        ev->peer = 0;
        for (auto &p : peers)
        {
          if (p.second.peer == event.peer)
            ev->peer = p.first;
        }
        ASSERT(ev->peer >= 0);
        ev->handle(*this);
        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT:
        break;
      default:
        break;
    }
  }
}

void Warg_Server::update(float32 dt)
{
  time += dt;

  if (!local)
    process_packets();
  else
    process_events();

  for (auto &c : chars)
  {
    auto &cid = c.first;
    auto &ch = c.second;

    ch.move(dt, collider_cache);
    if (_isnan(ch.physics.pos.x) || _isnan(ch.physics.pos.y) || _isnan(ch.physics.pos.z))
      ch.physics.pos = map.spawn_pos[ch.team];
    update_buffs(cid, dt);
    ch.apply_modifiers();
    update_cast(cid, dt);
    update_target(cid);
    ch.update_hp(dt);
    ch.update_mana(dt);
    ch.update_spell_cooldowns(dt);
    ch.update_global_cooldown(dt);
  }

  push(make_unique<Player_Geometry_Message>(chars));

  for (auto i = spell_objs.begin(); i != spell_objs.end();)
  {
    if (update_spell_object(&*i))
      i = spell_objs.erase(i);
    else
      i++;
  }

  send_events();
}

void Warg_Server::process_events()
{
  auto process_peer_events = [&](UID uid, Warg_Peer &p) {
    ASSERT(p.in);
    while (!p.in->empty())
    {
      auto &ev = p.in->front();
      if (get_real_time() - ev->t < SIM_LATENCY / 2000.0f)
        return;
      ev->peer = uid;
      ev->handle(*this);
      p.in->pop();
    }
  };

  for (auto &peer : peers)
    process_peer_events(peer.first, peer.second);
}

bool Warg_Server::update_spell_object(SpellObjectInst *so)
{
  ASSERT(so->target && chars.count(so->target));
  Character *target = &chars[so->target];

  float d = length(target->physics.pos - so->pos);

  if (d < 0.3)
  {
    for (auto &e : so->def.effects)
    {
      SpellEffectInst i;
      i.def = *e;
      i.caster = so->caster;
      i.pos = so->pos;
      i.target = so->target;
      invoke_spell_effect(i);
    }
    return true;
  }

  vec3 a = normalize(target->physics.pos - so->pos);
  a.x *= so->def.speed * dt;
  a.y *= so->def.speed * dt;
  a.z *= so->def.speed * dt;
  so->pos += a;
  return false;
}

void Char_Spawn_Request_Message::handle(Warg_Server &server)
{
  UID character = server.add_char(team, name.c_str());
  ASSERT(server.peers.count(peer));
  server.peers[peer].character = character;
  unique_ptr<Message> msg = make_unique<Player_Control_Message>(character);
  server.send_event(server.peers[peer], msg);
  server.push(make_unique<Char_Spawn_Message>(character, name.c_str(), team));
}

void Player_Movement_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
  ASSERT(server.chars.count(peer_.character));
  auto &character = server.chars[peer_.character];

  Movement_Command command;
  command.i = i;
  command.dir = dir;
  command.m = move_status;
  character.last_movement_command = command;
}

void Cast_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
  ASSERT(server.chars.count(peer_.character));
  auto &character = server.chars[peer_.character];

  server.try_cast_spell(character, target, spell.c_str());
}

void Ping_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
  unique_ptr<Message> msg = make_unique<Ack_Message>();
  server.send_event(peer_, msg);
}

void Ack_Message::handle(Warg_Server &server)
{
  ASSERT(server.peers.count(peer));
  auto &peer_ = server.peers[peer];
}

void Warg_Server::try_cast_spell(Character &caster, UID target_, const char *spell_)
{
  ASSERT(spell_);

  Character *target = (target_ && chars.count(target_)) ? &chars[target_] : nullptr;
  ASSERT(caster.spellbook.count(spell_));
  Spell *spell = &caster.spellbook[spell_];

  CastErrorType err;
  if (static_cast<int>(err = cast_viable(caster.id, target_, spell)))
    push(make_unique<Cast_Error_Message>(caster.id, target_, spell_, (int)err));
  else
    cast_spell(caster.id, target_, spell);
}

void Warg_Server::cast_spell(UID caster_, UID target_, Spell *spell)
{
  ASSERT(spell);

  if (spell->def->cast_time > 0)
    begin_cast(caster_, target_, spell);
  else
    release_spell(caster_, target_, spell);
}

void Warg_Server::begin_cast(UID caster_, UID target_, Spell *spell)
{
  ASSERT(0 <= caster_ && caster_ < chars.size());
  ASSERT(spell);
  ASSERT(spell->def->cast_time > 0);

  Character *caster = &chars[caster_];
  Character *target = (target_ && chars.count(target_)) ? &chars[target_] : nullptr;

  caster->casting = true;
  caster->casting_spell = spell;
  caster->cast_progress = 0;
  caster->cast_target = target_;
  caster->gcd = caster->e_stats.gcd;

  push(make_unique<Cast_Begin_Message>(caster_, target_, spell->def->name.c_str()));
}

void Warg_Server::interrupt_cast(UID ci)
{
  ASSERT(ci && chars.count(ci));
  Character *c = &chars[ci];

  c->casting = false;
  c->cast_progress = 0;
  c->casting_spell = nullptr;
  c->gcd = 0;
  push(make_unique<Cast_Interrupt_Message>(ci));
}

void Warg_Server::update_cast(UID caster_, float32 dt)
{
  ASSERT(caster_ && chars.count(caster_));
  Character *caster = &chars[caster_];

  if (!caster->casting)
    return;

  ASSERT(caster->casting_spell);
  ASSERT(caster->casting_spell->def);

  caster->cast_progress += caster->e_stats.cast_speed * dt;
  if (caster->cast_progress >= caster->casting_spell->def->cast_time)
  {
    caster->cast_progress = 0;
    caster->casting = false;
    release_spell(caster_, caster->cast_target, caster->casting_spell);
    caster->cast_target = 0;
    caster->casting_spell = nullptr;
  }
}

CastErrorType Warg_Server::cast_viable(UID caster_, UID target_, Spell *spell)
{
  ASSERT(caster_ && chars.count(caster_));
  ASSERT(spell);
  ASSERT(spell->def);

  Character *caster = &chars[caster_];
  Character *target = (target_ && chars.count(target_)) ? &chars[target_] : nullptr;

  if (caster->silenced)
    return CastErrorType::Silenced;
  if (caster->gcd > 0)
    return CastErrorType::GCD;
  if (spell->cd_remaining > 0)
    return CastErrorType::SpellCD;
  if (spell->def->mana_cost > caster->mana)
    return CastErrorType::NotEnoughMana;
  if (!target && spell->def->targets != SpellTargets::Terrain)
    return CastErrorType::InvalidTarget;
  if (target && length(caster->physics.pos - target->physics.pos) > spell->def->range &&
      spell->def->targets != SpellTargets::Terrain)
    return CastErrorType::OutOfRange;
  if (caster->casting)
    return CastErrorType::AlreadyCasting;
}

void Warg_Server::release_spell(UID caster_, UID target_, Spell *spell)
{
  ASSERT(0 <= caster_ && caster_ < chars.size());
  ASSERT(spell);
  ASSERT(spell->def);

  Character *caster = &chars[caster_];
  Character *target = (target_ && chars.count(target_)) ? &chars[target_] : nullptr;

  CastErrorType err;
  if (static_cast<int>(err = cast_viable(caster_, target_, spell)))
  {
    push(make_unique<Cast_Error_Message>(caster_, target_, spell->def->name.c_str(), static_cast<int>(err)));
    return;
  }

  for (auto &e : spell->def->effects)
  {
    SpellEffectInst i;
    i.def = *e;
    i.caster = caster_;
    i.pos = caster->physics.pos;
    i.target = target_;

    invoke_spell_effect(i);
  }

  if (!spell->def->cast_time)
    caster->gcd = caster->e_stats.gcd;
  spell->cd_remaining = spell->def->cooldown;
}

void Warg_Server::invoke_spell_effect(SpellEffectInst &effect)
{
  switch (effect.def.type)
  {
    case SpellEffectType::AoE:
      invoke_spell_effect_aoe(effect);
      break;
    case SpellEffectType::ApplyBuff:
    case SpellEffectType::ApplyDebuff:
      invoke_spell_effect_apply_buff(effect);
      break;
    case SpellEffectType::ClearDebuffs:
      invoke_spell_effect_clear_debuffs(effect);
      break;
    case SpellEffectType::Damage:
      invoke_spell_effect_damage(effect);
      break;
    case SpellEffectType::Heal:
      invoke_spell_effect_heal(effect);
      break;
    case SpellEffectType::Interrupt:
      invoke_spell_effect_interrupt(effect);
      break;
    case SpellEffectType::ObjectLaunch:
      invoke_spell_effect_object_launch(effect);
      break;
    default:
      ASSERT(false);
  }
}

void Warg_Server::invoke_spell_effect_aoe(SpellEffectInst &effect)
{
  ASSERT(effect.caster && chars.count(effect.caster));

  for (int ch = 0; ch < chars.size(); ch++)
  {
    Character *c = &chars[ch];

    bool in_range = length(c->physics.pos - effect.pos) <= effect.def.aoe.radius;
    bool at_ally = effect.def.aoe.targets == SpellTargets::Ally && c->team == chars[effect.caster].team;
    bool at_hostile = effect.def.aoe.targets == SpellTargets::Hostile && c->team != chars[effect.caster].team;

    if (in_range && (at_ally || at_hostile))
    {
      SpellEffectInst i;
      i.def = *effect.def.aoe.effect;
      i.caster = effect.caster;
      i.pos = {0, 0, 0};
      i.target = ch;

      invoke_spell_effect(i);
    }
  }
}

void Warg_Server::invoke_spell_effect_apply_buff(SpellEffectInst &effect)
{
  ASSERT(effect.target && chars.count(effect.target));
  Character *target = &chars[effect.target];

  bool is_buff = effect.def.type == SpellEffectType::ApplyBuff;

  Buff buff;
  buff.def = is_buff ? *effect.def.applybuff.buff : *effect.def.applydebuff.debuff;
  buff.duration = buff.def.duration;
  buff.ticks_left = (int)glm::floor(buff.def.duration * buff.def.tick_freq);
  if (is_buff)
    target->buffs.push_back(buff);
  else
    target->debuffs.push_back(buff);

  push(make_unique<Buff_Application_Message>(effect.target, buff.def.name.c_str()));
}

void Warg_Server::invoke_spell_effect_clear_debuffs(SpellEffectInst &effect)
{
  ASSERT(effect.caster && chars.count(effect.caster));
  Character *c = &chars[effect.target];
  c->debuffs.clear();
}

void Warg_Server::invoke_spell_effect_damage(SpellEffectInst &effect)
{
  ASSERT(effect.target && chars.count(effect.target));
  Character *target = &chars[effect.target];
  ASSERT(target->alive);

  DamageEffect *d = &effect.def.damage;

  int effective = d->n;
  int overkill = 0;

  if (!d->pierce_mod)
    effective *= target->e_stats.damage_mod;

  target->hp -= effective;

  if (target->hp < 0)
  {
    effective += target->hp;
    overkill = -target->hp;
    target->hp = 0;
    target->alive = false;
  }

  push(make_unique<Char_HP_Message>(effect.target, target->hp));
}

void Warg_Server::invoke_spell_effect_heal(SpellEffectInst &effect)
{
  ASSERT(effect.target && chars.count(effect.target));
  Character *target = &chars[effect.target];
  ASSERT(target->alive);

  HealEffect *h = &effect.def.heal;

  int effective = h->n;
  int overheal = 0;

  target->hp += effective;

  if (target->hp > target->hp_max)
  {
    overheal = target->hp - target->hp_max;
    effective -= overheal;
    target->hp = target->hp_max;
  }

  push(make_unique<Char_HP_Message>(effect.target, target->hp));
}

void Warg_Server::invoke_spell_effect_interrupt(SpellEffectInst &effect)
{
  ASSERT(effect.target && chars.count(effect.target));
  Character *target = &chars[effect.target];

  if (!target->casting)
    return;

  target->casting = false;
  target->casting_spell = nullptr;
  target->cast_progress = 0;
  target->cast_target = 0;

  push(make_unique<Cast_Interrupt_Message>(effect.target));
}

void Warg_Server::invoke_spell_effect_object_launch(SpellEffectInst &effect)
{
  ASSERT(effect.def.objectlaunch.object);

  SpellObjectInst obji;
  obji.def = sdb->objects[effect.def.objectlaunch.object];
  obji.caster = effect.caster;
  obji.target = effect.target;
  obji.pos = effect.pos;
  spell_objs.push_back(obji);

  push(make_unique<Object_Launch_Message>(effect.def.objectlaunch.object, obji.caster, obji.target, obji.pos));
}

void Warg_Server::update_buffs(UID character, float32 dt)
{
  ASSERT(character && chars.count(character));
  Character *ch = &chars[character];

  auto update_buffs_ = [&](std::vector<Buff> &buffs) {
    for (auto b = buffs.begin(); b != buffs.end();)
    {
      b->duration -= dt;
      if (b->duration * b->def.tick_freq < b->ticks_left)
      {
        for (auto &e : b->def.tick_effects)
        {
          SpellEffectInst i;
          i.def = *e;
          i.caster = 0;
          i.target = character;
          i.pos = {0, 0, 0};

          invoke_spell_effect(i);
        }
        b->ticks_left--;
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
  ASSERT(ch && chars.count(ch));
  Character *c = &chars[ch];

  if (!c->target)
    return;

  ASSERT(chars.count(c->target));
  Character *target = &chars[c->target];

  if (!target->alive)
    c->target = 0;
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
  c.physics.pos = map.spawn_pos[team];
  c.physics.dir = map.spawn_dir[team];
  c.hp_max = 100;
  c.hp = c.hp_max;
  c.mana_max = 500;
  c.mana = c.mana_max;

  CharStats s;
  s.gcd = 1.5;
  s.speed = 4.0;
  s.cast_speed = 1;
  s.hp_regen = 2;
  s.mana_regen = 10;
  s.damage_mod = 1;
  s.atk_dmg = 5;
  s.atk_speed = 2;

  c.b_stats = s;
  c.e_stats = s;

  for (int i = 0; i < sdb->spells.size(); i++)
  {
    Spell s;
    s.def = &sdb->spells[i];
    s.cd_remaining = 0;
    c.spellbook[s.def->name] = s;
  }

  chars[id] = c;

  return id;
}

void Warg_Server::connect(std::queue<unique_ptr<Message>> *in, std::queue<unique_ptr<Message>> *out)
{
  peers[uid()] = {nullptr, in, out, 0};
}

void Warg_Server::push(unique_ptr<Message> ev)
{
  ASSERT(ev != nullptr);
  tick_events.push_back(std::move(ev));
  ASSERT(tick_events.back() != nullptr);
}

void Warg_Server::send_event(Warg_Peer &p, unique_ptr<Message> &ev)
{
  ev->t = get_real_time();
  ASSERT(ev != nullptr);
  if (local)
  {
    ASSERT(p.out);
    p.out->push(std::move(ev));
  }
  else
  {
    ASSERT(p.peer);
    Buffer b;
    ev->serialize(b);
    ENetPacket *packet = enet_packet_create(&b.data[0], b.data.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(p.peer, 0, packet);
    enet_host_flush(server);
  }
}

void Warg_Server::send_events()
{
  for (auto &p : peers)
  {
    for (auto &ev : tick_events)
    {
      ASSERT(ev != nullptr);
      send_event(p.second, ev);
    }
  }
  tick_events.clear();
}
