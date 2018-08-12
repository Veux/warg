#include "stdafx.h"
#include "Spell.h"
#include "Warg_Common.h"

Spell_Database SPELL_DB;

void Character_Stats::operator*=(const Character_Stats &b)
{
  global_cooldown *= b.global_cooldown;
  speed *= b.speed;
  cast_speed *= b.cast_speed;
  mana_regen *= b.mana_regen;
  hp_regen *= b.hp_regen;
  damage_mod *= b.damage_mod;
  atk_dmg *= b.atk_dmg;
  atk_speed *= b.atk_dmg;
}

Character_Stats operator*(const Character_Stats &a, const Character_Stats &b)
{
  Character_Stats c;
  c.global_cooldown = a.global_cooldown * b.global_cooldown;
  c.speed = a.speed * b.speed;
  c.cast_speed = a.cast_speed * b.cast_speed;
  c.mana_regen = a.mana_regen * b.mana_regen;
  c.hp_regen = a.hp_regen * b.hp_regen;
  c.damage_mod = a.damage_mod * b.damage_mod;
  c.atk_dmg = a.atk_dmg * b.atk_dmg;
  c.atk_speed = a.atk_speed * b.atk_speed;

  return c;
}

Spell_Formula *Spell_Database::get_spell(Spell_Index index)
{
  ASSERT(index < spells.size());
  return &spells[index];
}

Spell_Formula *Spell_Database::get_spell(const char *name)
{
  for (Spell_Formula &formula : spells)
  {
    if (formula.name == name)
      return &formula;
  }
  return nullptr;
}

Spell_Object_Formula *Spell_Database::get_spell_object(Spell_Index index)
{
  ASSERT(index < objects.size());
  return &objects[index];
}

Spell_Object_Formula *Spell_Database::get_spell_object(Spell_ID id)
{
  for (Spell_Object_Formula &formula : objects)
    if (formula._id == id)
      return &formula;
  return nullptr;
}

BuffDef *Spell_Database::get_buff(Spell_Index index)
{
  ASSERT(index < buffs.size());
  return &buffs[index];
}

BuffDef *Spell_Database::get_buff(Spell_ID id)
{
  for (BuffDef &buff : buffs)
    if (buff._id == id)
      return &buff;
  return nullptr;
}

Spell_Formula *Spell_Database::add_spell()
{
  Spell_Formula spell;
  spell.index = spells.size();
  spells.push_back(spell);
  return &spells.back();
}

Spell_Object_Formula *Spell_Database::add_spell_object()
{
  Spell_Object_Formula object;
  object.index = objects.size();
  objects.push_back(object);
  return &objects.back();
}

BuffDef *Spell_Database::add_buff()
{
  BuffDef buff;
  buff.index = buffs.size();
  buffs.push_back(buff);
  return &buffs.back();
}

void shadow_word_pain_debuff_tick(BuffDef *formula, Buff *buff, Game_State *game_state, Character *character)
{
  ASSERT(character);
  game_state->damage_character(nullptr, character, 5);
}

void shadow_word_pain_release(Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  ASSERT(caster);
  Character *target = game_state->get_character(caster->cast_target);
  ASSERT(target);

  target->remove_debuff(Spell_ID::Shadow_Word_Pain);
  BuffDef *buff_formula = SPELL_DB.get_buff(Spell_ID::Shadow_Word_Pain);
  Buff buff;
  buff._id = Spell_ID::Shadow_Word_Pain;
  buff.formula_index = buff_formula->index;
  buff.duration = buff_formula->duration;
  buff.time_since_last_tick = 0.f;
  target->apply_debuff(&buff);
}

void corruption_debuff_tick(BuffDef *formula, Buff *buff, Game_State *game_state, Character *character)
{
  ASSERT(character);
  game_state->damage_character(nullptr, character, 5);
}

void corruption_release(Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  ASSERT(caster);
  Character *target = game_state->get_character(caster->cast_target);
  ASSERT(target);

  target->remove_debuff(Spell_ID::Corruption);
  BuffDef *buff_formula = SPELL_DB.get_buff(Spell_ID::Corruption);
  Buff buff;
  buff._id = Spell_ID::Corruption;
  buff.formula_index = buff_formula->index;
  buff.duration = buff_formula->duration;
  buff.time_since_last_tick = 0.f;
  target->apply_debuff(&buff);
}

void frostbolt_release(Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  Spell_Object_Formula *object_formula = SPELL_DB.get_spell_object(Spell_ID::Frostbolt);
  Spell_Object object;
  object.formula_index = object_formula->index;
  object.caster = caster->id;
  object.target = caster->target_id;
  object.pos = caster->physics.position;
  object.id = uid();
  game_state->spell_objects[game_state->spell_object_count++] = object;
}

void blink_release(Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  ASSERT(caster);

  vec3 dir = caster->physics.orientation * vec3(0, 1, 0);
  vec3 delta = normalize(dir) * vec3(15.f);
  collide_and_slide_char(caster->physics, caster->radius, delta, vec3(0, 0, -100), *colliders);
}

void sprint_release(Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  ASSERT(caster);

  caster->remove_buff(Spell_ID::Sprint);
  BuffDef *buff_formula = SPELL_DB.get_buff(Spell_ID::Sprint);
  Buff buff;
  buff._id = Spell_ID::Sprint;
  buff.formula_index = buff_formula->index;
  buff.duration = buff_formula->duration;
  caster->apply_buff(&buff);
}

void icy_veins_release(Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  ASSERT(caster);

  caster->remove_buff(Spell_ID::Icy_Veins);
  BuffDef *buff_formula = SPELL_DB.get_buff(Spell_ID::Icy_Veins);
  Buff buff;
  buff._id = Spell_ID::Icy_Veins;
  buff.formula_index = buff_formula->index;
  buff.duration = buff_formula->duration;
  caster->apply_buff(&buff);
}

void frostbolt_object_on_hit(
    Spell_Object_Formula *formula, Spell_Object *object, Game_State *game_state, Colliders *colliders)
{
  Character *caster = game_state->get_character(object->caster);
  Character *target = game_state->get_character(object->target);
  ASSERT(target);
  ASSERT(formula);
  ASSERT(object);

  game_state->damage_character(caster, target, 10);

  target->remove_debuff(Spell_ID::Frostbolt);
  BuffDef *debuff_formula = SPELL_DB.get_buff(Spell_ID::Frostbolt);
  Buff debuff;
  debuff._id = Spell_ID::Frostbolt;
  debuff.formula_index = debuff_formula->index;
  debuff.duration = debuff_formula->duration;
  target->apply_debuff(&debuff);
}

void seed_of_corruption_debuff_on_damage(
    BuffDef *formula, Buff *buff, Game_State *game_state, Character *subject, Character *object, float32 damage)
{
  if (subject->id != buff->u.seed_of_corruption.caster)
    return;

  buff->u.seed_of_corruption.damage_taken += damage;
  if (buff->u.seed_of_corruption.damage_taken >= 20)
  {
    buff_on_end_dispatch(formula, buff, game_state, object);
    object->remove_debuff(Spell_ID::Seed_of_Corruption);
  }
}

void seed_of_corruption_debuff_on_end(BuffDef *formula, Buff *buff, Game_State *game_state, Character *character)
{
  for (size_t i = 0; i < game_state->character_count; i++)
  {
    Character *target = &game_state->characters[i];
    bool within_range = length(target->physics.position - character->physics.position) < 10.f;
    bool same_team = character->team == target->team;
    if (within_range && same_team)
    {
      BuffDef *buff_formula = SPELL_DB.get_buff(Spell_ID::Corruption);
      Buff buff;
      buff._id = Spell_ID::Corruption;
      buff.formula_index = buff_formula->index;
      buff.duration = buff_formula->duration;
      buff.time_since_last_tick = 0.f;
      target->apply_debuff(&buff);
    }
  }
}

void seed_of_corruption_release(Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  Spell_Object_Formula *object_formula = SPELL_DB.get_spell_object(Spell_ID::Seed_of_Corruption);
  Spell_Object object;
  object.formula_index = object_formula->index;
  object.caster = caster->id;
  object.target = caster->target_id;
  object.pos = caster->physics.position;
  object.id = uid();
  game_state->spell_objects[game_state->spell_object_count++] = object;
}

void seed_of_corruption_object_on_hit(
    Spell_Object_Formula *formula, Spell_Object *object, Game_State *game_state, Colliders *colliders)
{
  Character *target = game_state->get_character(object->target);
  ASSERT(target);
  ASSERT(formula);
  ASSERT(object);

  BuffDef *debuff_formula = SPELL_DB.get_buff(Spell_ID::Seed_of_Corruption);

  Buff *existing = target->find_debuff(Spell_ID::Seed_of_Corruption);
  if (existing)
  {
    buff_on_end_dispatch(debuff_formula, existing, game_state, target);
    target->remove_debuff(Spell_ID::Seed_of_Corruption);
    return;
  }

  Buff debuff;
  debuff._id = Spell_ID::Seed_of_Corruption;
  debuff.formula_index = debuff_formula->index;
  debuff.duration = debuff_formula->duration;
  debuff.u.seed_of_corruption.caster = object->caster;
  debuff.u.seed_of_corruption.damage_taken = 0;
  target->apply_debuff(&debuff);
}

void demonic_circle_summon_release(
    Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  ASSERT(caster);

  BuffDef *buff_formula = SPELL_DB.get_buff(Spell_ID::Demonic_Circle_Summon);
  ASSERT(buff_formula);

  caster->remove_buff(Spell_ID::Demonic_Circle_Summon);
  Buff buff;
  buff._id = Spell_ID::Demonic_Circle_Summon;
  buff.formula_index = buff_formula->index;
  buff.duration = buff_formula->duration;
  buff.u.demonic_circle.position = caster->physics.position;
  buff.u.demonic_circle.grounded = caster->physics.grounded;
  caster->apply_buff(&buff);
}

void demonic_circle_teleport_release(
    Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  ASSERT(caster);

  Buff *circle_buff = caster->find_buff(Spell_ID::Demonic_Circle_Summon);

  if (!circle_buff)
    return;

  caster->physics.position = circle_buff->u.demonic_circle.position;
  caster->physics.grounded = circle_buff->u.demonic_circle.grounded;
}

void buff_on_end_dispatch(BuffDef *formula, Buff *buff, Game_State *game_state, Character *character)
{
  switch (formula->_id)
  {
    case Spell_ID::Seed_of_Corruption:
      seed_of_corruption_debuff_on_end(formula, buff, game_state, character);
      break;
    default:
      break;
  }
}

void buff_on_damage_dispatch(
    BuffDef *formula, Buff *buff, Game_State *game_state, Character *subject, Character *object, float32 damage)
{
  switch (formula->_id)
  {
    case Spell_ID::Seed_of_Corruption:
      seed_of_corruption_debuff_on_damage(formula, buff, game_state, subject, object, damage);
      break;
    default:
      break;
  }
}

void buff_on_tick_dispatch(BuffDef *formula, Buff *buff, Game_State *game_state, Character *character)
{
  switch (formula->_id)
  {
    case Spell_ID::Shadow_Word_Pain:
      shadow_word_pain_debuff_tick(formula, buff, game_state, character);
      break;
    case Spell_ID::Corruption:
      corruption_debuff_tick(formula, buff, game_state, character);
    default:
      break;
  }
}

void spell_object_on_hit_dispatch(
    Spell_Object_Formula *formula, Spell_Object *object, Game_State *game_state, Colliders *colliders)
{
  switch (formula->_id)
  {
    case Spell_ID::Frostbolt:
      frostbolt_object_on_hit(formula, object, game_state, colliders);
      break;
    case Spell_ID::Seed_of_Corruption:
      seed_of_corruption_object_on_hit(formula, object, game_state, colliders);
      break;
    default:
      break;
  }
}

void spell_on_release_dispatch(Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  switch (formula->_id)
  {
  case Spell_ID::Blink:
    blink_release(formula, game_state, caster, colliders);
    break;
  case Spell_ID::Corruption:
    corruption_release(formula, game_state, caster, colliders);
    break;
  case Spell_ID::Frostbolt:
    frostbolt_release(formula, game_state, caster, colliders);
    break;
  case Spell_ID::Icy_Veins:
    icy_veins_release(formula, game_state, caster, colliders);
    break;
  case Spell_ID::Shadow_Word_Pain:
    shadow_word_pain_release(formula, game_state, caster, colliders);
    break;
  case Spell_ID::Sprint:
    sprint_release(formula, game_state, caster, colliders);
    break;
  case Spell_ID::Seed_of_Corruption:
    seed_of_corruption_release(formula, game_state, caster, colliders);
    break;
  case Spell_ID::Demonic_Circle_Summon:
    demonic_circle_summon_release(formula, game_state, caster, colliders);
    break;
  case Spell_ID::Demonic_Circle_Teleport:
    demonic_circle_teleport_release(formula, game_state, caster, colliders);
    break;
  default:
    break;
  }
}

Spell_Database::Spell_Database()
{
  // FROSTBOLT

  BuffDef *fb_debuff = add_buff();
  fb_debuff->_id = Spell_ID::Frostbolt;
  fb_debuff->name = "FrostboltSlowDebuff";
  fb_debuff->icon = "frostbolt.jpg";
  fb_debuff->duration = 10;
  fb_debuff->stats_modifiers.speed = 0.2;

  Spell_Object_Formula *fb_object = add_spell_object();
  fb_object->name = "Frostbolt";
  fb_object->_id = Spell_ID::Frostbolt;
  fb_object->speed = 30;

  Spell_Formula *frostbolt = add_spell();
  frostbolt->_id = Spell_ID::Frostbolt;
  frostbolt->name = "Frostbolt";
  frostbolt->icon = "frostbolt.jpg";
  frostbolt->mana_cost = 20;
  frostbolt->range = 30;
  frostbolt->targets = Spell_Targets::Hostile;
  frostbolt->cooldown = 0;
  frostbolt->cast_time = 1.5f;
  frostbolt->on_global_cooldown = true;

  // BLINK

  Spell_Formula *blink = add_spell();
  blink->_id = Spell_ID::Blink;
  blink->name = "Blink";
  blink->icon = "blink.jpg";
  blink->mana_cost = 5;
  blink->range = 0.f;
  blink->targets = Spell_Targets::Self;
  blink->cooldown = 15.f;
  blink->cast_time = 0.f;
  blink->on_global_cooldown = false;

  // SHADOW WORD: PAIN

  BuffDef *swp_buff = add_buff();
  swp_buff->_id = Spell_ID::Shadow_Word_Pain;
  swp_buff->name = "ShadowWordPainBuff";
  swp_buff->icon = "shadow_word_pain.jpg";
  swp_buff->duration = 15;
  swp_buff->tick_freq = 1.0 / 3;

  Spell_Formula *swp = add_spell();
  swp->_id = Spell_ID::Shadow_Word_Pain;
  swp->name = "Shadow Word: Pain";
  swp->icon = "shadow_word_pain.jpg";
  swp->mana_cost = 50;
  swp->range = 30;
  swp->cooldown = 0;
  swp->cast_time = 0;
  swp->on_global_cooldown = true;
  swp->targets = Spell_Targets::Hostile;

  // ICY VEINS

  BuffDef *icy_veins_buff = add_buff();
  icy_veins_buff->_id = Spell_ID::Icy_Veins;
  icy_veins_buff->name = "IcyVeinsBuff";
  icy_veins_buff->icon = "../Assets/Icons/icy_veins.jpg";
  icy_veins_buff->duration = 20;
  icy_veins_buff->stats_modifiers.cast_speed = 2.f;

  Spell_Formula *icy_veins = add_spell();
  icy_veins->_id = Spell_ID::Icy_Veins;
  icy_veins->name = "Icy Veins";
  icy_veins->icon = "icy_veins.jpg";
  icy_veins->mana_cost = 20;
  icy_veins->range = 0;
  icy_veins->targets = Spell_Targets::Self;
  icy_veins->cooldown = 0.f;
  icy_veins->cast_time = 0;
  icy_veins->on_global_cooldown = false;

  // SPRINT

  BuffDef *sprint_buff = add_buff();
  sprint_buff->_id = Spell_ID::Sprint;
  sprint_buff->name = "Sprint";
  sprint_buff->duration = 15.f;
  sprint_buff->icon = "sprint.jpg";
  sprint_buff->stats_modifiers.speed = 2.f;

  Spell_Formula *sprint = add_spell();
  sprint->_id = Spell_ID::Sprint;
  sprint->name = "Sprint";
  sprint->cooldown = 10.f;
  // sprint->icon = "sprint.jpg");
  sprint->mana_cost = 5;
  sprint->cast_time = 0.f;
  sprint->on_global_cooldown = false;
  sprint->targets = Spell_Targets::Self;

  // DEMONIC CIRCLE: SUMMON

  BuffDef *demonic_circle_buff = add_buff();
  demonic_circle_buff->_id = Spell_ID::Demonic_Circle_Summon;
  demonic_circle_buff->name = "Demonic Circle";
  demonic_circle_buff->duration = 3600.f;
  demonic_circle_buff->icon = "demonic_circle_summon.jpg";

  Spell_Formula *demonic_circle_summon = add_spell();
  demonic_circle_summon->_id = Spell_ID::Demonic_Circle_Summon;
  demonic_circle_summon->name = "Demonic Circle: Summon";
  demonic_circle_summon->cooldown = 30.f;
  demonic_circle_summon->icon = "demonic_circle_summon.jpg";
  demonic_circle_summon->mana_cost = 5;
  demonic_circle_summon->cast_time = 0.f;
  demonic_circle_summon->on_global_cooldown = true;
  demonic_circle_summon->targets = Spell_Targets::Self;

  // DEMONIC CIRCLE: TELEPORT

  Spell_Formula *demonic_circle_teleport = add_spell();
  demonic_circle_teleport->_id = Spell_ID::Demonic_Circle_Teleport;
  demonic_circle_teleport->name = "Demonic Circle: Teleport";
  demonic_circle_teleport->cooldown = 5.f;
  demonic_circle_teleport->icon = "demonic_circle_teleport.jpg";
  demonic_circle_teleport->mana_cost = 5;
  demonic_circle_teleport->cast_time = 0.f;
  demonic_circle_teleport->on_global_cooldown = false;
  demonic_circle_teleport->targets = Spell_Targets::Self;

  // CORRUPTION

  BuffDef *corruption_buff = add_buff();
  corruption_buff->_id = Spell_ID::Corruption;
  corruption_buff->name = "Corruption";
  corruption_buff->icon = "corruption.jpg";
  corruption_buff->duration = 15;
  corruption_buff->tick_freq = 1.0 / 3;

  Spell_Formula *corruption = add_spell();
  corruption->_id = Spell_ID::Corruption;
  corruption->name = "Corruption";
  corruption->icon = "corruption.jpg";
  corruption->mana_cost = 50;
  corruption->range = 30;
  corruption->cooldown = 0;
  corruption->cast_time = 0;
  corruption->on_global_cooldown = true;
  corruption->targets = Spell_Targets::Hostile;

  // SEED OF CORRUPTION

  BuffDef *soc_buff = add_buff();
  soc_buff->_id = Spell_ID::Seed_of_Corruption;
  soc_buff->name = "Seed of Corruption";
  soc_buff->duration = 20.f;
  soc_buff->icon = "seed_of_corruption.jpg";

  Spell_Object_Formula *soc_object = add_spell_object();
  soc_object->name = "Seed of Corruption";
  soc_object->_id = Spell_ID::Seed_of_Corruption;
  soc_object->speed = 30;

  Spell_Formula *seed_of_corruption = add_spell();
  seed_of_corruption->_id = Spell_ID::Seed_of_Corruption;
  seed_of_corruption->name = "Seed of Corruption";
  seed_of_corruption->cooldown = 5.f;
  seed_of_corruption->icon = "seed_of_corruption.jpg";
  seed_of_corruption->mana_cost = 5;
  seed_of_corruption->range = 30.f;
  seed_of_corruption->cast_time = 1.f;
  seed_of_corruption->on_global_cooldown = true;
  seed_of_corruption->targets = Spell_Targets::Hostile;

  SPELL_DB = *this;
}
