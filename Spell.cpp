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
  character->take_damage(5);
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
  Character *target = game_state->get_character(object->target);
  ASSERT(target);
  ASSERT(formula);
  ASSERT(object);

  target->take_damage(10);

  target->remove_debuff(Spell_ID::Frostbolt);
  BuffDef *debuff_formula = SPELL_DB.get_buff(Spell_ID::Frostbolt);
  Buff debuff;
  debuff._id = Spell_ID::Frostbolt;
  debuff.formula_index = debuff_formula->index;
  debuff.duration = debuff_formula->duration;
  target->apply_debuff(&debuff);
}

struct Demonic_Circle_Buff_Data
{
  vec3 position = vec3(0.f);
  bool grounded = false;
};

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
  Demonic_Circle_Buff_Data data_struct;
  Demonic_Circle_Buff_Data *data_ptr = (Demonic_Circle_Buff_Data *)malloc(sizeof *data_ptr);
  *data_ptr = data_struct;
  data_ptr->position = caster->physics.position;
  data_ptr->grounded = caster->physics.grounded;
  buff._data = data_ptr;
  caster->apply_buff(&buff);
}

void demonic_circle_teleport_release(
    Spell_Formula *formula, Game_State *game_state, Character *caster, Colliders *colliders)
{
  ASSERT(caster);

  Buff *circle_buff = caster->find_buff(Spell_ID::Demonic_Circle_Summon);

  if (!circle_buff)
    return;

  Demonic_Circle_Buff_Data *data_ptr = (Demonic_Circle_Buff_Data *)circle_buff->_data;

  caster->physics.position = data_ptr->position;
  caster->physics.grounded = data_ptr->grounded;
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
  fb_object->_on_hit = frostbolt_object_on_hit;

  Spell_Formula *frostbolt = add_spell();
  frostbolt->name = "Frostbolt";
  frostbolt->icon = "frostbolt.jpg";
  frostbolt->mana_cost = 20;
  frostbolt->range = 30;
  frostbolt->targets = Spell_Targets::Hostile;
  frostbolt->cooldown = 0;
  frostbolt->cast_time = 1.5f;
  frostbolt->on_global_cooldown = true;
  frostbolt->_on_release = frostbolt_release;

  // BLINK

  Spell_Formula *blink = add_spell();
  blink->name = "Blink";
  blink->icon = "blink.jpg";
  blink->mana_cost = 5;
  blink->range = 0.f;
  blink->targets = Spell_Targets::Self;
  blink->cooldown = 15.f;
  blink->cast_time = 0.f;
  blink->on_global_cooldown = false;
  blink->_on_release = blink_release;

  // SHADOW WORD: PAIN

  BuffDef *swp_buff = add_buff();
  swp_buff->_id = Spell_ID::Shadow_Word_Pain;
  swp_buff->name = "ShadowWordPainBuff";
  swp_buff->icon = "shadow_word_pain.jpg";
  swp_buff->duration = 15;
  swp_buff->tick_freq = 1.0 / 3;
  swp_buff->_on_tick = shadow_word_pain_debuff_tick;

  Spell_Formula *swp = add_spell();
  swp->name = "Shadow Word: Pain";
  swp->icon = "shadow_word_pain.jpg";
  swp->mana_cost = 50;
  swp->range = 30;
  swp->cooldown = 0;
  swp->cast_time = 0;
  swp->on_global_cooldown = true;
  swp->targets = Spell_Targets::Hostile;
  swp->_on_release = shadow_word_pain_release;

  // ICY VEINS

  BuffDef *icy_veins_buff = add_buff();
  icy_veins_buff->_id = Spell_ID::Icy_Veins;
  icy_veins_buff->name = "IcyVeinsBuff";
  icy_veins_buff->icon = "../Assets/Icons/icy_veins.jpg";
  icy_veins_buff->duration = 20;
  icy_veins_buff->stats_modifiers.cast_speed = 2.f;

  Spell_Formula *icy_veins = add_spell();
  icy_veins->name = "Icy Veins";
  icy_veins->icon = "icy_veins.jpg";
  icy_veins->mana_cost = 20;
  icy_veins->range = 0;
  icy_veins->targets = Spell_Targets::Self;
  icy_veins->cooldown = 0.f;
  icy_veins->cast_time = 0;
  icy_veins->on_global_cooldown = false;
  icy_veins->_on_release = icy_veins_release;

  // SPRINT

  BuffDef *sprint_buff = add_buff();
  sprint_buff->_id = Spell_ID::Sprint;
  sprint_buff->name = "Sprint";
  sprint_buff->duration = 15.f;
  sprint_buff->icon = "sprint.jpg";
  sprint_buff->stats_modifiers.speed = 2.f;

  Spell_Formula *sprint = add_spell();
  sprint->name = "Sprint";
  sprint->cooldown = 10.f;
  // sprint->icon = "sprint.jpg");
  sprint->mana_cost = 5;
  sprint->cast_time = 0.f;
  sprint->on_global_cooldown = false;
  sprint->targets = Spell_Targets::Self;
  sprint->_on_release = sprint_release;

  // DEMONIC CIRCLE: SUMMON

  BuffDef *demonic_circle_buff = add_buff();
  demonic_circle_buff->_id = Spell_ID::Demonic_Circle_Summon;
  demonic_circle_buff->name = "Demonic Circle";
  demonic_circle_buff->duration = 3600.f;
  demonic_circle_buff->icon = "demonic_circle_summon.jpg";

  Spell_Formula *demonic_circle_summon = add_spell();
  demonic_circle_summon->name = "Demonic Circle: Summon";
  demonic_circle_summon->cooldown = 30.f;
  demonic_circle_summon->icon = "demonic_circle_summon.jpg";
  demonic_circle_summon->mana_cost = 5;
  demonic_circle_summon->cast_time = 0.f;
  demonic_circle_summon->on_global_cooldown = true;
  demonic_circle_summon->targets = Spell_Targets::Self;
  demonic_circle_summon->_on_release = demonic_circle_summon_release;

  // DEMONIC CIRCLE: TELEPORT

  Spell_Formula *demonic_circle_teleport = add_spell();
  demonic_circle_teleport->name = "Demonic Circle: Teleport";
  demonic_circle_teleport->cooldown = 5.f;
  demonic_circle_teleport->icon = "demonic_circle_teleport.jpg";
  demonic_circle_teleport->mana_cost = 5;
  demonic_circle_teleport->cast_time = 0.f;
  demonic_circle_teleport->on_global_cooldown = false;
  demonic_circle_teleport->targets = Spell_Targets::Self;
  demonic_circle_teleport->_on_release = demonic_circle_teleport_release;

  SPELL_DB = *this;
}
