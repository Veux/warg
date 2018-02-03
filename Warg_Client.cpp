#include "Warg_Client.h"
#include "Warg_State.h"
#include <cstring>
#include <memory>

Warg_Client::Warg_Client(Scene_Graph *scene_, queue<Warg_Event> *in_)
{
  Map blades_edge = make_blades_edge();

  map = make_blades_edge();
  sdb = make_spell_db();
  scene = scene_;
  in = in_;
}

void Warg_Client::update(float32 dt)
{
  process_events();

  for (auto &c_ : chars)
  {
    auto &c = c_.second;
    if (!c.alive)
      c.pos = {-1000, -1000, 0};
    c.mesh->position = c.pos;
    c.mesh->scale = vec3(1.0f);
    c.mesh->orientation =
        angleAxis((float32)atan2(c.dir.y, c.dir.x), vec3(0.f, 0.f, 1.f));
  }

  for (auto i = spell_objs.begin(); i != spell_objs.end();)
  {
    auto &o = *i;
    float32 d = length(chars[o.target].pos - o.pos);
    if (d < 0.5)
    {
      set_message("object hit",
          chars[o.caster].name + "'s " + o.def.name + " hit " +
              chars[o.target].name,
          3.0f);

      i = spell_objs.erase(i);
    }
    else
    {
      vec3 a = normalize(chars[o.target].pos - o.pos);
      a.x *= o.def.speed * dt;
      a.y *= o.def.speed * dt;
      a.z *= o.def.speed * dt;
      o.pos += a;

      o.mesh->position = o.pos;

      ++i;
    }
  }
}

void Warg_Client::process_events()
{
  while (!in->empty())
  {
    Warg_Event ev = in->front();

    if (get_real_time() - ev.t < SIM_LATENCY / 2000.0f)
      return;

    ASSERT(ev.event);
    switch (ev.type)
    {
      case Warg_Event_Type::CharSpawn:
        process_event((CharSpawn_Event *)ev.event);
        break;
      case Warg_Event_Type::PlayerControl:
        process_event((PlayerControl_Event *)ev.event);
        break;
      case Warg_Event_Type::CharPos:
        process_event((CharPos_Event *)ev.event);
        break;
      case Warg_Event_Type::CastError:
        process_event((CastError_Event *)ev.event);
        break;
      case Warg_Event_Type::CastBegin:
        process_event((CastBegin_Event *)ev.event);
        break;
      case Warg_Event_Type::CastInterrupt:
        process_event((CastInterrupt_Event *)ev.event);
        break;
      case Warg_Event_Type::CharHP:
        process_event((CharHP_Event *)ev.event);
        break;
      case Warg_Event_Type::BuffAppl:
        process_event((BuffAppl_Event *)ev.event);
        break;
      case Warg_Event_Type::ObjectLaunch:
        process_event((ObjectLaunch_Event *)ev.event);
        break;
      default:
        break;
    }
    free_warg_event(ev);
    in->pop();
  }
}

void Warg_Client::process_event(CharSpawn_Event *spawn)
{
  ASSERT(spawn);

  add_char(spawn->id, spawn->team, spawn->name);
}

void Warg_Client::process_event(PlayerControl_Event *ev)
{
  ASSERT(pc);

  pc = ev->character;
}

void Warg_Client::process_event(CharPos_Event *pos)
{
  ASSERT(pos);
  ASSERT(pos->character >= 0 && chars.count(pos->character));
  chars[pos->character].dir = pos->dir;
  chars[pos->character].pos = pos->pos;
}

void Warg_Client::process_event(CastError_Event *err)
{
  ASSERT(err);
  ASSERT(err->caster >= 0 && chars.count(err->caster));

  std::string msg;
  msg += chars[err->caster].name;
  msg += " failed to cast ";
  msg += err->spell;
  msg += ": ";

  switch (err->err)
  {
    case (int)CastErrorType::Silenced:
      msg += "silenced";
      break;
    case (int)CastErrorType::GCD:
      msg += "GCD";
      break;
    case (int)CastErrorType::SpellCD:
      msg += "spell on cooldown";
      break;
    case (int)CastErrorType::NotEnoughMana:
      msg += "not enough mana";
      break;
    case (int)CastErrorType::InvalidTarget:
      msg += "invalid target";
      break;
    case (int)CastErrorType::OutOfRange:
      msg += "out of range";
      break;
    case (int)CastErrorType::AlreadyCasting:
      msg += "already casting";
      break;
    case (int)CastErrorType::Success:
      ASSERT(false);
      break;
    default:
      ASSERT(false);
  }

  set_message("SpellError:", msg, 10);
}

void Warg_Client::process_event(CastBegin_Event *cast)
{
  ASSERT(cast);
  ASSERT(cast->caster >= 0 && chars.count(cast->caster));

  std::string msg;
  msg += chars[cast->caster].name;
  msg += " begins casting ";
  msg += cast->spell;
  set_message("CastBegin:", msg, 10);
}

void Warg_Client::process_event(CastInterrupt_Event *interrupt)
{
  ASSERT(interrupt);
  ASSERT(interrupt->caster >= 0 && chars.count(interrupt->caster));

  std::string msg;
  msg += chars[interrupt->caster].name;
  msg += "'s casting was interrupted";
  set_message("CastInterrupt:", msg, 10);
}

void Warg_Client::process_event(CharHP_Event *hpev)
{
  ASSERT(hpev);
  ASSERT(hpev->character >= 0 && chars.count(hpev->character));

  chars[hpev->character].alive = hpev->hp > 0;

  std::string msg;
  msg += chars[hpev->character].name;
  msg += "'s HP is ";
  msg += std::to_string(hpev->hp);
  set_message("", msg, 10);
}

void Warg_Client::process_event(BuffAppl_Event *appl)
{
  ASSERT(appl);
  ASSERT(appl->character >= 0 && chars.count(appl->character));

  std::string msg;
  msg += appl->buff;
  msg += " applied to ";
  msg += chars[appl->character].name;
  set_message("ApplyBuff:", msg, 10);
}

void Warg_Client::process_event(ObjectLaunch_Event *launch)
{
  ASSERT(launch);
  ASSERT(launch->caster >= 0 && chars.count(launch->caster));
  ASSERT(launch->target >= 0 && chars.count(launch->target));

  Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "test_emissive.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "world_origin_distance.frag";

  SpellObjectInst obji;
  obji.def = sdb->objects[launch->object];
  obji.caster = launch->caster;
  obji.target = launch->target;
  obji.pos = launch->pos;
  obji.mesh = scene->add_primitive_mesh(cube, "spell_object_cube", material);
  obji.mesh->scale = vec3(0.4f);
  spell_objs.push_back(obji);

  std::string msg;
  msg += (0 <= launch->caster && launch->caster < chars.size())
             ? chars[launch->caster].name
             : "unknown character";
  msg += " launched ";
  msg += sdb->objects[launch->object].name;
  msg += " at ";
  msg += (0 <= launch->target && launch->target < chars.size())
             ? chars[launch->target].name
             : "unknown character";
  set_message("ObjectLaunch:", msg, 10);
}

void Warg_Client::add_char(UID id, int team, const char *name)
{
  ASSERT(name);

  vec3 pos = map.spawn_pos[team];
  vec3 dir = map.spawn_dir[team];

  Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";

  Character c;
  c.team = team;
  c.name = std::string(name);
  c.pos = pos;
  c.dir = dir;
  c.mesh = scene->add_primitive_mesh(cube, "player_cube", material);
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
}
