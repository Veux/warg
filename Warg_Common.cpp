#include "Warg_Common.h"

Map make_blades_edge()
{
  Map blades_edge;

  // spawns
  blades_edge.spawn_pos[0] = {5, 5, 5};
  blades_edge.spawn_pos[1] = {45, 45, 5};
  blades_edge.spawn_dir[0] = {0, 1, 0};
  blades_edge.spawn_dir[1] = {0, -1, 0};

  blades_edge.mesh.unique_identifier = "blades_edge_map";
  blades_edge.material.backface_culling = false;
  blades_edge.material.albedo = "crate_diffuse.png";
  blades_edge.material.emissive = "";
  blades_edge.material.normal = "crate_normal.png";
  blades_edge.material.roughness = "crate_roughness.png";
  blades_edge.material.vertex_shader = "vertex_shader.vert";
  blades_edge.material.frag_shader = "fragment_shader.frag";
  blades_edge.material.casts_shadows = true;
  blades_edge.material.uv_scale = vec2(16);

  return blades_edge;
}

void Character::update_hp(float32 dt)
{
  hp += e_stats.hp_regen * dt;
  if (hp > hp_max)
    hp = hp_max;
}

void Character::update_mana(float32 dt)
{
  mana += e_stats.mana_regen * dt;

  if (mana > mana_max)
    mana = mana_max;
}

void Character::update_spell_cooldowns(float32 dt)
{
  for (auto &spell_ : spellbook)
  {
    auto &spell = spell_.second;
    if (spell.cd_remaining > 0)
      spell.cd_remaining -= dt;
    if (spell.cd_remaining < 0)
      spell.cd_remaining = 0;
  }
}

void Character::update_global_cooldown(float32 dt)
{
  gcd -= dt;
  if (gcd < 0)
    gcd = 0;
}

void Character::apply_modifier(CharMod &modifier)
{
  switch (modifier.type)
  {
    case CharModType::DamageTaken:
      e_stats.damage_mod *= modifier.damage_taken.n;
      break;
    case CharModType::Speed:
      e_stats.speed *= modifier.speed.m;
      break;
    case CharModType::CastSpeed:
      e_stats.cast_speed *= modifier.cast_speed.m;
    case CharModType::Silence:
      silenced = true;
    default:
      break;
  }
}

void Character::apply_modifiers()
{
  e_stats = b_stats;
  silenced = false;

  for (auto &buff : buffs)
    for (auto &modifier : buff.def.char_mods)
      apply_modifier(*modifier);
  for (auto &debuff : debuffs)
    for (auto &modifier : debuff.def.char_mods)
      apply_modifier(*modifier);
}
