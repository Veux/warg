#include "stdafx.h"
#include "Spell.h"

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

Spell_Effect_Formula *Spell_Database::get_spell_effect(Spell_Index index)
{
  ASSERT(index < effects.size());
  return &effects[index];
}

Spell_Object_Formula *Spell_Database::get_spell_object(Spell_Index index)
{
  ASSERT(index < objects.size());
  return &objects[index];
}

BuffDef *Spell_Database::get_buff(Spell_Index index)
{
  ASSERT(index < buffs.size());
  return &buffs[index];
}

CharMod *Spell_Database::get_character_modifier(Spell_Index index)
{
  ASSERT(index < char_mods.size());
  return &char_mods[index];
}

Spell_Formula *Spell_Database::add_spell()
{
  Spell_Formula spell;
  spell.index = spells.size();
  spells.push_back(spell);
  return &spells.back();
}

Spell_Effect_Formula *Spell_Database::add_spell_effect()
{
  Spell_Effect_Formula effect;
  effect.index = effects.size();
  effects.push_back(effect);
  return &effects.back();
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

CharMod *Spell_Database::add_character_modifier()
{
  CharMod character_modifier;
  character_modifier.index = char_mods.size();
  char_mods.push_back(character_modifier);
  return &char_mods.back();
}

Spell_Database::Spell_Database()
{
  auto icon = [](const char *filename)
  {
    return Texture(s("../Assets/Icons/", filename));
  };

  // FROSTBOLT

  CharMod *fb_slow = add_character_modifier();
  fb_slow->type = Character_Modifier_Type::Speed;
  fb_slow->speed.factor = 0.2;

  BuffDef *fb_debuff = add_buff();
  fb_debuff->name = "FrostboltSlowDebuff";
  fb_debuff->icon = icon("frostbolt.jpg");
  fb_debuff->duration = 10;
  fb_debuff->character_modifiers.push_back(fb_slow->index);

  Spell_Effect_Formula *fb_debuff_appl = add_spell_effect();
  fb_debuff_appl->name = "FrostboltDebuffApplyEffect";
  fb_debuff_appl->type = Spell_Effect_Type::Apply_Debuff;
  fb_debuff_appl->apply_debuff.debuff_formula = fb_debuff->index;

  Spell_Effect_Formula *fb_damage = add_spell_effect();
  fb_damage->name = "FrostboltDamageEffect";
  fb_damage->type = Spell_Effect_Type::Damage;
  fb_damage->damage.amount = 15;
  fb_damage->damage.pierce_absorb = false;
  fb_damage->damage.pierce_mod = false;

  Spell_Object_Formula *fb_object = add_spell_object();
  fb_object->name = "Frostbolt";
  fb_object->speed = 30;
  fb_object->effects.push_back(fb_debuff_appl->index);
  fb_object->effects.push_back(fb_damage->index);

  Spell_Effect_Formula *fb_object_launch = add_spell_effect();
  fb_object_launch->name = "FrostboltObjectLaunchEffect";
  fb_object_launch->type = Spell_Effect_Type::Object_Launch;
  fb_object_launch->object_launch.object_formula = fb_object->index;

  Spell_Formula *frostbolt = add_spell();
  frostbolt->name = "Frostbolt";
  frostbolt->icon = icon("frostbolt.jpg");
  frostbolt->mana_cost = 20;
  frostbolt->range = 30;
  frostbolt->targets = Spell_Targets::Hostile;
  frostbolt->cooldown = 0;
  frostbolt->cast_time = 1.5f;
  frostbolt->on_global_cooldown = true;
  frostbolt->effects.push_back(fb_object_launch->index);

  // BLINK

  Spell_Effect_Formula *blink_effect = add_spell_effect();
  blink_effect->blink.distance = 15.f;
  blink_effect->type = Spell_Effect_Type::Blink;

  Spell_Formula *blink = add_spell();
  blink->name = "Blink";
  blink->icon = icon("blink.jpg");
  blink->mana_cost = 5;
  blink->range = 0.f;
  blink->targets = Spell_Targets::Self;
  blink->cooldown = 15.f;
  blink->cast_time = 0.f;
  blink->on_global_cooldown = false;
  blink->effects.push_back(blink_effect->index);

  // SHADOW WORD: PAIN

  Spell_Effect_Formula *swp_tick = add_spell_effect();
  swp_tick->type = Spell_Effect_Type::Damage;
  swp_tick->damage.amount = 5;
  swp_tick->damage.pierce_absorb = false;
  swp_tick->damage.pierce_mod = false;

  BuffDef *swp_buff = add_buff();
  swp_buff->name = "ShadowWordPainBuff";
  swp_buff->icon = icon("shadow_word_pain.jpg");
  swp_buff->duration = 15;
  swp_buff->tick_freq = 1.0 / 3;
  swp_buff->tick_effects.push_back(swp_tick->index);

  Spell_Effect_Formula *swp_effect = add_spell_effect();
  swp_effect->name = "ShadowWordPainEffect";
  swp_effect->type = Spell_Effect_Type::Apply_Debuff;
  swp_effect->apply_debuff.debuff_formula = swp_buff->index;

  Spell_Formula *swp = add_spell();
  swp->name = "Shadow Word: Pain";
  swp->icon = icon("shadow_word_pain.jpg");
  swp->mana_cost = 50;
  swp->range = 30;
  swp->cooldown = 0;
  swp->cast_time = 0;
  swp->on_global_cooldown = true;
  swp->targets = Spell_Targets::Self;
  swp->effects.push_back(swp_effect->index);

  // ICY VEINS

  CharMod *icy_veins_mod = add_character_modifier();
  icy_veins_mod->type = Character_Modifier_Type::CastSpeed;
  icy_veins_mod->cast_speed.factor = 2.f;

  BuffDef *icy_veins_buff = add_buff();
  icy_veins_buff->name = "IcyVeinsBuff";
  icy_veins_buff->icon = Texture("../Assets/Icons/icy_veins.jpg");
  icy_veins_buff->duration = 20;
  icy_veins_buff->character_modifiers.push_back(icy_veins_mod->index);

  Spell_Effect_Formula *icy_veins_buff_appl = add_spell_effect();
  icy_veins_buff_appl->name = "IcyVeinsBuffEffect";
  icy_veins_buff_appl->type = Spell_Effect_Type::Apply_Buff;
  icy_veins_buff_appl->apply_buff.buff_formula = icy_veins_buff->index;

  Spell_Formula *icy_veins = add_spell();
  icy_veins->name = "Icy Veins";
  icy_veins->icon = icon("icy_veins.jpg");
  icy_veins->mana_cost = 20;
  icy_veins->range = 0;
  icy_veins->targets = Spell_Targets::Self;
  icy_veins->cooldown = 0.f;
  icy_veins->cast_time = 0;
  icy_veins->on_global_cooldown = false;
  icy_veins->effects.push_back(icy_veins_buff_appl->index);

  // SPRINT

  CharMod *sprint_modifier = add_character_modifier();
  sprint_modifier->type = Character_Modifier_Type::Speed;
  sprint_modifier->speed.factor = 2.f;

  BuffDef *sprint_buff = add_buff();
  sprint_buff->name = "Sprint";
  sprint_buff->duration = 15.f;
  sprint_buff->icon = icon("sprint.jpg");
  sprint_buff->character_modifiers.push_back(sprint_modifier->index);

  Spell_Effect_Formula *sprint_effect = add_spell_effect();
  sprint_effect->name = "Sprint";
  sprint_effect->type = Spell_Effect_Type::Apply_Buff;
  sprint_effect->apply_buff.buff_formula = sprint_buff->index;

  Spell_Formula *sprint = add_spell();
  sprint->name = "Sprint";
  sprint->cooldown = 10.f;
  sprint->icon = icon("sprint.jpg");
  sprint->mana_cost = 5;
  sprint->cast_time = 0.f;
  sprint->on_global_cooldown = false;
  sprint->targets = Spell_Targets::Self;
  sprint->effects.push_back(sprint_effect->index);
}
