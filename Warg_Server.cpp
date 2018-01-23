#include "Warg_Server.h"
#include "Warg_State.h"
#include <cstring>
#include <memory>

Warg_Server::Warg_Server()
{
  map = make_blades_edge();
  sdb = make_spell_db();
}

void Warg_Server::update(float32 dt)
{
  process_events();

  for (int ch = 0; ch < chars.size(); ch++)
  {
    move_char(ch, dt);
    update_buffs(ch, dt);
    apply_char_mods(ch);
    update_gcd(ch, dt);
    update_cds(ch, dt);
    update_cast(ch, dt);
    update_target(ch);
    update_hp(ch, dt);
    update_mana(ch, dt);

    push(char_pos_event(ch, chars[ch].dir, chars[ch].pos));
  }

  for (auto i = spell_objs.begin(); i != spell_objs.end();)
  {
    if (update_spell_object(&*i))
      i = spell_objs.erase(i);
    else
      i++;
  }
}

void Warg_Server::process_events()
{
  for (auto &conn : connections)
  {
    while (!conn.in->empty())
    {
      eq.push(conn.in->front());
      conn.in->pop();
    }
  }

  while (!eq.empty())
  {
    Warg_Event ev = eq.front();

    if (get_real_time() - ev.t < SIM_LATENCY / 2000.0f)
      return;

    ASSERT(ev.event);
    switch (ev.type)
    {
      case Warg_Event_Type::CharSpawnRequest:
        process_event((CharSpawnRequest_Event *)ev.event);
        break;
      case Warg_Event_Type::Dir:
        process_event((Dir_Event *)ev.event);
        break;
      case Warg_Event_Type::Move:
        process_event((Move_Event *)ev.event);
        break;
      case Warg_Event_Type::Jump:
        process_event((Jump_Event *)ev.event);
        break;
      case Warg_Event_Type::Cast:
        process_event((Cast_Event *)ev.event);
        break;
      default:
        break;
    }
    free_warg_event(ev);
    eq.pop();
  }
}

void Warg_Server::update_gcd(int ch, float32 dt)
{
  ASSERT(0 <= ch && ch < chars.size());
  Character *c = &chars[ch];

  c->gcd -= dt;
  if (c->gcd < 0)
    c->gcd = 0;
}

void Warg_Server::update_cds(int ch, float32 dt)
{
  ASSERT(0 <= ch && ch < chars.size());
  Character *c = &chars[ch];

  for (auto &s_ : c->spellbook)
  {
    auto &s = std::get<1>(s_);
    if (s.cd_remaining > 0)
      s.cd_remaining -= dt;
    if (s.cd_remaining < 0)
      s.cd_remaining = 0;
  }
}

bool Warg_Server::update_spell_object(SpellObjectInst *so)
{
  ASSERT(0 <= so->target && so->target < chars.size());
  Character *target = &chars[so->target];

  float d = length(target->pos - so->pos);

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

  vec3 a = normalize(target->pos - so->pos);
  a.x *= so->def.speed * dt;
  a.y *= so->def.speed * dt;
  a.z *= so->def.speed * dt;
  so->pos += a;
  return false;
}

void Warg_Server::process_event(CharSpawnRequest_Event *req)
{
  ASSERT(req);
  char *name = req->name;
  int team = req->team;
  ASSERT(name);

  add_char(team, name);
  push(char_spawn_event(name, team));
}

void Warg_Server::process_event(Dir_Event *dir)
{
  ASSERT(dir);

  chars[dir->character].dir = dir->dir;
}

void Warg_Server::process_event(Move_Event *mv)
{
  ASSERT(mv);
  ASSERT(0 <= mv->character && mv->character < chars.size());

  chars[mv->character].move_status = mv->m;
}

void Warg_Server::process_event(Jump_Event *jump)
{
  ASSERT(jump);
  ASSERT(0 <= jump->character && jump->character < chars.size());
  Character *c = &chars[jump->character];

  if (c->grounded)
  {
    c->vel.z += 4;
    c->grounded = false;
  }
}

void Warg_Server::process_event(Cast_Event *cast)
{
  ASSERT(cast);

  int ch = cast->character;
  ASSERT(0 <= ch && ch < chars.size());

  int target = cast->target;
  ASSERT(target < (int)chars.size());

  char *spell = cast->spell;
  ASSERT(spell);

  try_cast_spell(ch, target, spell);
}

void Warg_Server::collide_and_slide_char(
    int ci, const vec3 &vel, const vec3 &gravity)
{
  ASSERT(0 <= ci < chars.size());
  Character *c = &chars[ci];
  ASSERT(c);

  c->colpkt.e_radius = vec3(0.3, 0.3, 0.5);
  c->colpkt.pos_r3 = c->pos;
  c->colpkt.vel_r3 = vel;

  vec3 e_space_pos = c->colpkt.pos_r3 / c->colpkt.e_radius;
  vec3 e_space_vel = c->colpkt.vel_r3 / c->colpkt.e_radius;

  c->collision_recursion_depth = 0;

  vec3 final_pos = collide_char_with_world(ci, e_space_pos, e_space_vel);

  if (c->grounded) {
	  c->colpkt.pos_r3 = final_pos * c->colpkt.e_radius;
	  c->colpkt.vel_r3 = vec3(0, 0, -0.5);
	  c->colpkt.vel = vec3(0, 0, -0.5) / c->colpkt.e_radius;
	  c->colpkt.vel_normalized = normalize(c->colpkt.vel);
	  c->colpkt.base_point = final_pos;
	  c->colpkt.found_collision = false;

	  for (auto &surface : map.surfaces)
	  {
		  Triangle t;
		  t.a = surface.a / c->colpkt.e_radius;
		  t.b = surface.b / c->colpkt.e_radius;
		  t.c = surface.c / c->colpkt.e_radius;
		  check_triangle(&c->colpkt, t);
	  }

	  if (c->colpkt.found_collision && c->colpkt.nearest_distance > 0.05)
		  final_pos.z -= c->colpkt.nearest_distance - 0.005;
  }

  c->colpkt.pos_r3 = final_pos * c->colpkt.e_radius;
  c->colpkt.vel_r3 = gravity;

  e_space_vel = gravity / c->colpkt.e_radius;
  c->collision_recursion_depth = 0;

  final_pos = collide_char_with_world(ci, final_pos, e_space_vel);

  c->colpkt.pos_r3 = final_pos * c->colpkt.e_radius;
  c->colpkt.vel_r3 = vec3(0, 0, -0.05);
  c->colpkt.vel = vec3(0, 0, -0.05) / c->colpkt.e_radius;
  c->colpkt.vel_normalized = normalize(c->colpkt.vel);
  c->colpkt.base_point = final_pos;
  c->colpkt.found_collision = false;

  for (auto &surface : map.surfaces)
  {
	  Triangle t;
	  t.a = surface.a / c->colpkt.e_radius;
	  t.b = surface.b / c->colpkt.e_radius;
	  t.c = surface.c / c->colpkt.e_radius;
	  check_triangle(&c->colpkt, t);
  }

  c->grounded = c->colpkt.found_collision && gravity.z <= 0;

  final_pos *= c->colpkt.e_radius;
  c->pos = final_pos;
}

vec3 Warg_Server::collide_char_with_world(
    int ci, const vec3 &pos, const vec3 &vel)
{
  ASSERT(0 <= ci < chars.size());
  Character *c = &chars[ci];
  ASSERT(c);

  float epsilon = 0.005f;

  if (c->collision_recursion_depth > 5)
    return pos;

  c->colpkt.vel = vel;
  c->colpkt.vel_normalized = normalize(c->colpkt.vel);
  c->colpkt.base_point = pos;
  c->colpkt.found_collision = false;

  for (auto &surface : map.surfaces)
  {
	  bool nocol = c->colpkt.found_collision;
    Triangle t;
    t.a = surface.a / c->colpkt.e_radius;
    t.b = surface.b / c->colpkt.e_radius;
    t.c = surface.c / c->colpkt.e_radius;
    check_triangle(&c->colpkt, t);
  }

  if (!c->colpkt.found_collision)
  {
    return pos + vel;
  }

  vec3 destination_point = pos + vel;
  vec3 new_base_point = pos;

  if (c->colpkt.nearest_distance >= epsilon)
  {
    vec3 v = vel;
    v = normalize(v) * (c->colpkt.nearest_distance - epsilon);
    new_base_point = c->colpkt.base_point + v;

    v = normalize(v);
    c->colpkt.intersection_point -= epsilon * v;
  }

  vec3 slide_plane_origin = c->colpkt.intersection_point;
  vec3 slide_plane_normal = new_base_point - c->colpkt.intersection_point;
  slide_plane_normal = normalize(slide_plane_normal);
  Plane sliding_plane = Plane(slide_plane_origin, slide_plane_normal);

  vec3 new_destination_point =
      destination_point -
      sliding_plane.signed_distance_to(destination_point) * slide_plane_normal;

  vec3 new_vel_vec = new_destination_point - c->colpkt.intersection_point;

  if (length(new_vel_vec) < epsilon)
  {
    return new_base_point;
  }

  c->collision_recursion_depth++;
  return collide_char_with_world(ci, new_base_point, new_vel_vec);
}

void Warg_Server::move_char(int ci, float dt)
{
  ASSERT(0 <= ci < chars.size());
  Character *c = &chars[ci];
  ASSERT(c);

  vec3 v = vec3(0);
  if (c->move_status & Move_Status::Forwards)
    v += vec3(c->dir.x, c->dir.y, 0);
  else if (c->move_status & Move_Status::Backwards)
    v += -vec3(c->dir.x, c->dir.y, 0);
  if (c->move_status & Move_Status::Left)
  {
    mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
    vec4 v_ = vec4(c->dir.x, c->dir.y, 0, 0);
    v += vec3(r * v_);
  }
  else if (c->move_status & Move_Status::Right)
  {
    mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
    vec4 v_ = vec4(c->dir.x, c->dir.y, 0, 0);
    v += vec3(r * v_);
  }
  if (c->move_status)
  {
    v = normalize(v);
    v *= c->e_stats.speed;
  }

  c->vel.z -= 9.81 * dt;
  if (c->vel.z < -53)
    c->vel.z = -53;
  if (c->grounded)
    c->vel.z = 0;

  collide_and_slide_char(ci, v * dt, vec3(0, 0, c->vel.z) * dt);
}

void Warg_Server::try_cast_spell(int caster_, int target_, const char *spell_)
{
  ASSERT(0 <= caster_ && caster_ < chars.size());
  ASSERT(spell_);

  Character *caster = &chars[caster_];
  Character *target =
      (0 <= target_ && target_ < chars.size()) ? &chars[target_] : nullptr;
  ASSERT(caster->spellbook.count(spell_));
  Spell *spell = &caster->spellbook[spell_];

  CastErrorType err;
  if (static_cast<int>(err = cast_viable(caster_, target_, spell)))
    push(cast_error_event(caster_, target_, spell_, static_cast<int>(err)));
  else
    cast_spell(caster_, target_, spell);
}

void Warg_Server::cast_spell(int caster_, int target_, Spell *spell)
{
  ASSERT(spell);

  if (spell->def->cast_time > 0)
    begin_cast(caster_, target_, spell);
  else
    release_spell(caster_, target_, spell);
}

void Warg_Server::begin_cast(int caster_, int target_, Spell *spell)
{
  ASSERT(0 <= caster_ && caster_ < chars.size());
  ASSERT(spell);
  ASSERT(spell->def->cast_time > 0);

  Character *caster = &chars[caster_];
  Character *target =
      (0 <= target_ && target_ < chars.size()) ? &chars[target_] : nullptr;

  caster->casting = true;
  caster->casting_spell = spell;
  caster->cast_progress = 0;
  caster->cast_target = target_;
  caster->gcd = caster->e_stats.gcd;

  push(cast_begin_event(caster_, target_, spell->def->name.c_str()));
}

void Warg_Server::interrupt_cast(int ci)
{
  ASSERT(0 <= ci && ci < chars.size());
  Character *c = &chars[ci];

  c->casting = false;
  c->cast_progress = 0;
  c->casting_spell = nullptr;
  c->gcd = 0;
  push(cast_interrupt_event(ci));
}

void Warg_Server::update_cast(int caster_, float32 dt)
{
  ASSERT(0 <= caster_ && caster_ < chars.size());
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
    caster->cast_target = -1;
    caster->casting_spell = nullptr;
  }
}

CastErrorType Warg_Server::cast_viable(int caster_, int target_, Spell *spell)
{
  ASSERT(0 <= caster_ && caster_ < chars.size());
  ASSERT(target_ < (int)chars.size());
  ASSERT(spell);
  ASSERT(spell->def);

  Character *caster = &chars[caster_];
  Character *target =
      (0 <= target_ && target_ < chars.size()) ? &chars[target_] : nullptr;

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
  if (target && length(caster->pos - target->pos) > spell->def->range &&
      spell->def->targets != SpellTargets::Terrain)
    return CastErrorType::OutOfRange;
  if (caster->casting)
    return CastErrorType::AlreadyCasting;
}

void Warg_Server::release_spell(int caster_, int target_, Spell *spell)
{
  ASSERT(0 <= caster_ && caster_ < chars.size());
  ASSERT(spell);
  ASSERT(spell->def);

  Character *caster = &chars[caster_];
  Character *target =
      (0 <= target_ && target_ < chars.size()) ? &chars[target_] : nullptr;

  CastErrorType err;
  if (static_cast<int>(err = cast_viable(caster_, target_, spell)))
  {
    push(cast_error_event(
        caster_, target_, spell->def->name.c_str(), static_cast<int>(err)));
    return;
  }

  for (auto &e : spell->def->effects)
  {
    SpellEffectInst i;
    i.def = *e;
    i.caster = caster_;
    i.pos = caster->pos;
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
  ASSERT(0 <= effect.caster && effect.caster < chars.size());

  for (int ch = 0; ch < chars.size(); ch++)
  {
    Character *c = &chars[ch];

    bool in_range = length(c->pos - effect.pos) <= effect.def.aoe.radius;
    bool at_ally = effect.def.aoe.targets == SpellTargets::Ally &&
                   c->team == chars[effect.caster].team;
    bool at_hostile = effect.def.aoe.targets == SpellTargets::Hostile &&
                      c->team != chars[effect.caster].team;

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
  ASSERT(0 <= effect.target && effect.target <= chars.size());
  Character *target = &chars[effect.target];

  bool is_buff = effect.def.type == SpellEffectType::ApplyBuff;

  Buff buff;
  buff.def =
      is_buff ? *effect.def.applybuff.buff : *effect.def.applydebuff.debuff;
  buff.duration = buff.def.duration;
  buff.ticks_left = (int)glm::floor(buff.def.duration * buff.def.tick_freq);
  if (is_buff)
    target->buffs.push_back(buff);
  else
    target->debuffs.push_back(buff);

  push(buff_appl_event(effect.target, buff.def.name.c_str()));
}

void Warg_Server::invoke_spell_effect_clear_debuffs(SpellEffectInst &effect)
{
  ASSERT(0 <= effect.caster && effect.caster < chars.size());
  Character *c = &chars[effect.target];
  c->debuffs.clear();
}

void Warg_Server::invoke_spell_effect_damage(SpellEffectInst &effect)
{
  ASSERT(0 <= effect.target && effect.target < chars.size());
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

  push(char_hp_event(effect.target, target->hp));
}

void Warg_Server::invoke_spell_effect_heal(SpellEffectInst &effect)
{
  ASSERT(0 <= effect.target && effect.target < chars.size());
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

  push(char_hp_event(effect.target, target->hp));
}

void Warg_Server::invoke_spell_effect_interrupt(SpellEffectInst &effect)
{
  ASSERT(0 <= effect.target && effect.target < chars.size());
  Character *target = &chars[effect.target];

  if (!target->casting)
    return;

  target->casting = false;
  target->casting_spell = nullptr;
  target->cast_progress = 0;
  target->cast_target = -1;

  push(cast_interrupt_event(effect.target));
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

  push(object_launch_event(
      effect.def.objectlaunch.object, obji.caster, obji.target, obji.pos));
}

void Warg_Server::update_buffs(int character, float32 dt)
{
  ASSERT(0 <= character && character < chars.size());
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
          i.caster = -1;
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

void Warg_Server::update_target(int ch)
{
  ASSERT(0 <= ch && ch < chars.size());
  Character *c = &chars[ch];

  if (c->target < 0)
    return;

  ASSERT(c->target < chars.size());
  Character *target = &chars[c->target];

  if (!target->alive)
    c->target = -1;
}

void Warg_Server::update_mana(int ch, float32 dt)
{
  ASSERT(0 <= ch && ch < chars.size());
  Character *c = &chars[ch];

  c->mana += c->e_stats.mana_regen * dt;

  if (c->mana > c->mana_max)
    c->mana_max = c->mana_max;
}

void Warg_Server::update_hp(int ch, float32 dt)
{
  ASSERT(0 <= ch && ch < chars.size());
  Character *c = &chars[ch];

  c->hp += c->e_stats.hp_regen * dt;

  if (c->hp > c->hp_max)
    c->hp_max = c->hp_max;
}

void Warg_Server::apply_char_mods(int ch)
{
  ASSERT(0 <= ch && ch < chars.size());
  Character *c = &chars[ch];

  c->e_stats = c->b_stats;
  c->silenced = false;

  auto apply_char_mod = [](Character *c, CharMod &m) {
    switch (m.type)
    {
      case CharModType::DamageTaken:
        c->e_stats.damage_mod *= m.damage_taken.n;
        break;
      case CharModType::Speed:
        c->e_stats.speed *= m.speed.m;
        break;
      case CharModType::CastSpeed:
        c->e_stats.cast_speed *= m.cast_speed.m;
      case CharModType::Silence:
        c->silenced = true;
      default:
        break;
    }
  };

  for (auto &b : c->buffs)
    for (auto &m : b.def.char_mods)
      apply_char_mod(c, *m);
  for (auto &b : c->debuffs)
    for (auto &m : b.def.char_mods)
      apply_char_mod(c, *m);
}

void Warg_Server::add_char(int team, const char *name)
{
  ASSERT(0 <= team && team < 2);
  ASSERT(name);

  Character c;
  c.team = team;
  c.name = std::string(name);
  c.pos = map.spawn_pos[team];
  c.dir = map.spawn_dir[team];
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

  chars.push_back(c);
}

void Warg_Server::connect(
    std::queue<Warg_Event> *in, std::queue<Warg_Event> *out)
{
  connections.push_back({in, out});
}

void Warg_Server::push(Warg_Event ev)
{
  for (auto &conn : connections)
  {
    conn.out->push(ev);
  }
}
