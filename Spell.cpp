#include "Spell.h"

std::unique_ptr<Spell_Database> make_spell_db()
{
  auto db = std::make_unique<Spell_Database>();

  // DIVINE SHIELD

  // CharMod *ds_mod = &char_mods[nchar_mods++];
  // ds_mod->type = CharModType::DamageTaken;
  // ds_mod->damage_taken = DamageTakenMod{0};

  // BuffDef *ds_buff = &buffs[nbuffs++];
  // ds_buff->name = "DivineShieldBuff";
  // ds_buff->duration = 10;
  // ds_buff->char_mods.push_back(ds_mod);

  // SpellEffect *ds_effect = &spell_effects[nspell_effects++];
  // ds_effect->name = "DivineShieldEffect";
  // ds_effect->type = SpellEffectType::ApplyBuff;
  // ds_effect->applybuff.buff = ds_buff;

  // SpellDef *ds = &spells[nspells++];
  // ds->name = "Divine Shield";
  // ds->mana_cost = 50;
  // ds->range = -1;
  // ds->targets = SpellTargets::Self;
  // ds->cooldown = 5 * 60;
  // ds->cast_time = 0;
  // ds->effects.push_back(ds_effect);

  // // SHADOW WORD: PAIN

  // SpellEffect *swp_tick = &spell_effects[nspell_effects++];
  // swp_tick->name = "ShadowWordPainTickEffect";
  // swp_tick->type = SpellEffectType::Damage;
  // swp_tick->damage.n = 5;
  // swp_tick->damage.pierce_absorb = false;
  // swp_tick->damage.pierce_mod = false;

  // BuffDef *swp_buff = &buffs[nbuffs++];
  // swp_buff->name = "ShadowWordPainBuff";
  // swp_buff->duration = 5;
  // swp_buff->tick_freq = 1.0 / 3;
  // swp_buff->tick_effects.push_back(swp_tick);

  // SpellEffect *swp_effect = &spell_effects[nspell_effects++];
  // swp_effect->name = "ShadowWordPainEffect";
  // swp_effect->type = SpellEffectType::ApplyDebuff;
  // swp_effect->applydebuff.debuff = swp_buff;

  // SpellDef *swp = &spells[nspells++];
  // swp->name = "Shadow Word: Pain";
  // swp->mana_cost = 50;
  // swp->range = 30;
  // swp->cooldown = 0;
  // swp->cast_time = 0;
  // swp->targets = SpellTargets::Hostile;
  // swp->effects.push_back(swp_effect);

  // // ARCANE EXPLOSION

  // SpellEffect *ae_damage = &spell_effects[nspell_effects++];
  // ae_damage->name = "ArcaneExplosionDamageEffect";
  // ae_damage->type = SpellEffectType::Damage;
  // ae_damage->damage.n = 5;
  // ae_damage->damage.pierce_absorb = false;
  // ae_damage->damage.pierce_mod = false;

  // SpellEffect *ae_aoe = &spell_effects[nspell_effects++];
  // ae_aoe->name = "ArcaneExplosionAoEEffect";
  // ae_aoe->type = SpellEffectType::AoE;
  // ae_aoe->aoe.targets = SpellTargets::Hostile;
  // ae_aoe->aoe.radius = 8;
  // ae_aoe->aoe.effect = ae_damage;

  // SpellDef *ae = &spells[nspells++];
  // ae->name = "Arcane Explosion";
  // ae->mana_cost = 50;
  // ae->range = 0;
  // ae->cooldown = 0;
  // ae->cast_time = 0;
  // ae->targets = SpellTargets::Terrain;
  // ae->effects.push_back(ae_aoe);

  // // HOLY NOVA

  // SpellEffect *hn_heal = &spell_effects[nspell_effects++];
  // hn_heal->name = "HolyNovaHealEffect";
  // hn_heal->type = SpellEffectType::Heal;
  // hn_heal->heal.n = 10;

  // SpellEffect *hn_aoe = &spell_effects[nspell_effects++];
  // hn_aoe->name = "HolyNovaAoEEffect";
  // hn_aoe->type = SpellEffectType::AoE;
  // hn_aoe->aoe.targets = SpellTargets::Ally;
  // hn_aoe->aoe.radius = 8;
  // hn_aoe->aoe.effect = hn_heal;

  // SpellDef *hn = &spells[nspells++];
  // hn->name = "Holy Nova";
  // hn->mana_cost = 50;
  // hn->range = 0;
  // hn->targets = SpellTargets::Terrain;
  // hn->cooldown = 0;
  // hn->cast_time = 0;
  // hn->effects.push_back(hn_aoe);

  // // SHADOW NOVA

  // SpellEffect *shadow_nova_aoe = &spell_effects[nspell_effects++];
  // shadow_nova_aoe->name = "ShadowNovaAoEEffect";
  // shadow_nova_aoe->type = SpellEffectType::AoE;
  // shadow_nova_aoe->aoe.targets = SpellTargets::Hostile;
  // shadow_nova_aoe->aoe.radius = 8;
  // shadow_nova_aoe->aoe.effect = swp_effect;

  // SpellDef *shadow_nova = &spells[nspells++];
  // shadow_nova->name = "Shadow Nova";
  // shadow_nova->mana_cost = 50;
  // shadow_nova->range = 0;
  // shadow_nova->targets = SpellTargets::Terrain;
  // shadow_nova->cooldown = 0;
  // shadow_nova->cast_time = 0;
  // shadow_nova->effects.push_back(shadow_nova_aoe);

  // // MIND BLAST

  // SpellEffect *mb_damage = &spell_effects[nspell_effects++];
  // mb_damage->name = "MindBlastDamageEffect";
  // mb_damage->type = SpellEffectType::Damage;
  // mb_damage->damage.n = 20;
  // mb_damage->damage.pierce_absorb = false;
  // mb_damage->damage.pierce_mod = false;

  // SpellDef *mb = &spells[nspells++];
  // mb->name = "Mind Blast";
  // mb->mana_cost = 100;
  // mb->range = 15;
  // mb->targets = SpellTargets::Hostile;
  // mb->cooldown = 6;
  // mb->cast_time = 1.5;
  // mb->effects.push_back(mb_damage);

  // // LESSER HEAL

  // CharMod *lh_slow_mod = &char_mods[nchar_mods++];
  // lh_slow_mod->type = CharModType::Speed;
  // lh_slow_mod->speed.m = 0.5;

  // BuffDef *lh_slow_debuff = &buffs[nbuffs++];
  // lh_slow_debuff->name = "LesserHealSlowDebuff";
  // lh_slow_debuff->duration = 10;
  // lh_slow_debuff->tick_freq = 1.0 / 3;
  // lh_slow_debuff->char_mods.push_back(lh_slow_mod);
  // lh_slow_debuff->tick_effects.push_back(mb_damage);

  // SpellEffect *lh_slow_buff_appl = &spell_effects[nspell_effects++];
  // lh_slow_buff_appl->name = "LesserHealSlowEffect";
  // lh_slow_buff_appl->type = SpellEffectType::ApplyDebuff;
  // lh_slow_buff_appl->applydebuff.debuff = lh_slow_debuff;

  // SpellEffect *lh_heal = &spell_effects[nspell_effects++];
  // lh_heal->name = "LesserHealHealEffect";
  // lh_heal->type = SpellEffectType::Heal;
  // lh_heal->heal.n = 20;

  // SpellDef *lh = &spells[nspells++];
  // lh->name = "Lesser Heal";
  // lh->mana_cost = 50;
  // lh->range = 30;
  // lh->targets = SpellTargets::Ally;
  // lh->cooldown = 0;
  // lh->cast_time = 2;
  // lh->effects.push_back(lh_heal);
  // lh->effects.push_back(lh_slow_buff_appl);

  // // ICE BLOCK

  // SpellEffect *ib_clear_debuffs_effect = &spell_effects[nspell_effects++];
  // ib_clear_debuffs_effect->name = "IceBlockClearDebuffsEffect";
  // ib_clear_debuffs_effect->type = SpellEffectType::ClearDebuffs;

  // SpellDef *ib = &spells[nspells++];
  // ib->name = "Ice Block";
  // ib->mana_cost = 20;
  // ib->range = 0;
  // ib->targets = SpellTargets::Self;
  // ib->cooldown = 120;
  // ib->cast_time = 0;
  // ib->effects.push_back(ib_clear_debuffs_effect);

  // // FROSTBOLT

  // CharMod *fb_slow = &char_mods[nchar_mods++];
  // fb_slow->type = CharModType::Speed;
  // fb_slow->speed.m = 0.2;

  // BuffDef *fb_debuff = &buffs[nbuffs++];
  // fb_debuff->name = "FrostboltSlowDebuff";
  // fb_debuff->duration = 10;
  // fb_debuff->char_mods.push_back(fb_slow);

  // SpellEffect *fb_debuff_appl = &spell_effects[nspell_effects++];
  // fb_debuff_appl->name = "FrostboltDebuffApplyEffect";
  // fb_debuff_appl->type = SpellEffectType::ApplyDebuff;
  // fb_debuff_appl->applydebuff.debuff = fb_debuff;

  // SpellEffect *fb_damage = &spell_effects[nspell_effects++];
  // fb_damage->name = "FrostboltDamageEffect";
  // fb_damage->type = SpellEffectType::Damage;
  // fb_damage->damage.n = 15;
  // fb_damage->damage.pierce_absorb = false;
  // fb_damage->damage.pierce_mod = false;

  // int fb_object = nspell_objects;
  // SpellObjectDef *fb_object_ = &spell_objects[nspell_objects++];
  // fb_object_->name = "Frostbolt";
  // fb_object_->speed = 30;
  // fb_object_->effects.push_back(fb_debuff_appl);
  // fb_object_->effects.push_back(fb_damage);

  // SpellEffect *fb_object_launch = &spell_effects[nspell_effects++];
  // fb_object_launch->name = "FrostboltObjectLaunchEffect";
  // fb_object_launch->type = SpellEffectType::ObjectLaunch;
  // fb_object_launch->objectlaunch.object = fb_object;

  // SpellDef *fb = &spells[nspells++];
  // fb->name = "Frostbolt";
  // fb->mana_cost = 20;
  // fb->range = 30;
  // fb->targets = SpellTargets::Hostile;
  // fb->cooldown = 0;
  // fb->cast_time = 2;
  // fb->effects.push_back(fb_object_launch);

   // COUNTERSPELL

   //SpellEffect *cs_effect = &spell_effects[nspell_effects++];
   //cs_effect->name = "Counterspell";
   //cs_effect->type = SpellEffectType::Interrupt;
   //cs_effect->interrupt.lockout = 6;

   //SpellDef *cs = &spells[nspells++];
   //cs->name = "Counterspell";
   //cs->mana_cost = 10;
   //cs->range = 30;
   //cs->targets = SpellTargets::Hostile;
   //cs->cooldown = 24;
   //cs->cast_time = 0;
   //cs->effects.push_back(cs_effect);

  // FROSTBOLT

  CharMod fb_slow_;
  fb_slow_.type = Character_Modifier_Type::Speed;
  fb_slow_.speed.factor = 0.2;
  db->char_mods.push_back(fb_slow_);
  size_t fb_slow = db->char_mods.size() - 1;

  BuffDef fb_debuff_;
  fb_debuff_.name = "FrostboltSlowDebuff";
  fb_debuff_.icon = Texture("../Assets/Icons/frostbolt.jpg");
  fb_debuff_.duration = 10;
  fb_debuff_.char_mods.push_back(fb_slow_);
  db->buffs.push_back(fb_debuff_);
  size_t fb_debuff = db->buffs.size() - 1;

  Spell_Effect_Formula fb_debuff_appl_;
  fb_debuff_appl_.name = "FrostboltDebuffApplyEffect";
  fb_debuff_appl_.type = Spell_Effect_Type::Apply_Debuff;
  fb_debuff_appl_.apply_debuff.debuff_formula = fb_debuff;
  db->effects.push_back(fb_debuff_appl_);
  size_t fb_debuff_appl = db->effects.size() - 1;

  Spell_Effect_Formula fb_damage_;
  fb_damage_.name = "FrostboltDamageEffect";
  fb_damage_.type = Spell_Effect_Type::Damage;
  fb_damage_.damage.amount = 15;
  fb_damage_.damage.pierce_absorb = false;
  fb_damage_.damage.pierce_mod = false;
  db->effects.push_back(fb_damage_);
  size_t fb_damage = db->effects.size() - 1;

  Spell_Object_Formula fb_object_;
  fb_object_.name = "Frostbolt";
  fb_object_.speed = 30;
  fb_object_.effects.push_back(fb_debuff_appl);
  fb_object_.effects.push_back(fb_damage);
  db->objects.push_back(fb_object_);
  size_t fb_object = db->objects.size() - 1;

  Spell_Effect_Formula fb_object_launch_;
  fb_object_launch_.name = "FrostboltObjectLaunchEffect";
  fb_object_launch_.type = Spell_Effect_Type::Object_Launch;
  fb_object_launch_.object_launch.object_formula = fb_object;
  db->effects.push_back(fb_object_launch_);
  size_t fb_object_launch = db->effects.size() - 1;

  Spell_Formula frostbolt_;
  frostbolt_.name = "Frostbolt";
  frostbolt_.icon = Texture("../Assets/Icons/frostbolt.jpg");
  frostbolt_.mana_cost = 20;
  frostbolt_.range = 30;
  frostbolt_.targets = Spell_Targets::Hostile;
  frostbolt_.cooldown = 0;
  frostbolt_.cast_time = 1.5f;
  frostbolt_.on_global_cooldown = true;
  frostbolt_.effects.push_back(fb_object_launch);
  db->spells.push_back(frostbolt_);

  // BLINK

  Spell_Effect_Formula blink_effect_;
  blink_effect_.blink.distance = 15.f;
  blink_effect_.type = Spell_Effect_Type::Blink;
  db->effects.push_back(blink_effect_);
  size_t blink_effect = db->effects.size() - 1;
  
  Spell_Formula blink;
  blink.name = "Blink";
  blink.icon = Texture("../Assets/Icons/blink.jpg");
  blink.mana_cost = 5;
  blink.range = 0.f;
  blink.targets = Spell_Targets::Self;
  blink.cooldown = 15.f;
  blink.cast_time = 0.f;
  blink.on_global_cooldown = false;
  blink.effects.push_back(blink_effect);
  db->spells.push_back(blink);

   // SHADOW WORD: PAIN

  Spell_Effect_Formula swp_tick_;
  swp_tick_.type = Spell_Effect_Type::Damage;
  swp_tick_.damage.amount = 5;
  swp_tick_.damage.pierce_absorb = false;
  swp_tick_.damage.pierce_mod = false;
  db->effects.push_back(swp_tick_);
  size_t swp_tick = db->effects.size() - 1;

  BuffDef swp_buff_;
  swp_buff_.name = "ShadowWordPainBuff";
  swp_buff_.icon = Texture("../Assets/Icons/shadow_word_pain.jpg");
  swp_buff_.duration = 15;
  swp_buff_.tick_freq = 1.0 / 3;
  swp_buff_.tick_effects.push_back(swp_tick);
  db->buffs.push_back(swp_buff_);
  size_t swp_buff = db->buffs.size() - 1;

  Spell_Effect_Formula swp_effect_;
  swp_effect_.name = "ShadowWordPainEffect";
  swp_effect_.type = Spell_Effect_Type::Apply_Debuff;
  swp_effect_.apply_debuff.debuff_formula = swp_buff;
  db->effects.push_back(swp_effect_);
  size_t swp_effect = db->effects.size() - 1;

  Spell_Formula swp;
  swp.name = "Shadow Word: Pain";
  swp.icon = Texture("../Assets/Icons/shadow_word_pain.jpg");
  swp.mana_cost = 50;
  swp.range = 30;
  swp.cooldown = 0;
  swp.cast_time = 0;
  swp.on_global_cooldown = true;
  swp.targets = Spell_Targets::Self;
  swp.effects.push_back(swp_effect);
  db->spells.push_back(swp);
   
   // ICY VEINS

  CharMod icy_veins_mod_;
  icy_veins_mod_.type = Character_Modifier_Type::CastSpeed;
  icy_veins_mod_.cast_speed.factor = 2.f;
  db->char_mods.push_back(icy_veins_mod_);
  size_t icy_veins_mod = db->char_mods.size() - 1;

  BuffDef icy_veins_buff_;
  icy_veins_buff_.name = "IcyVeinsBuff";
  icy_veins_buff_.icon = Texture("../Assets/Icons/icy_veins.jpg");
  icy_veins_buff_.duration = 20;
  icy_veins_buff_.char_mods.push_back(icy_veins_mod_);
  db->buffs.push_back(icy_veins_buff_);
  size_t icy_veins_buff = db->buffs.size() - 1;

  Spell_Effect_Formula icy_veins_buff_appl_;
  icy_veins_buff_appl_.name = "IcyVeinsBuffEffect";
  icy_veins_buff_appl_.type = Spell_Effect_Type::Apply_Buff;
  icy_veins_buff_appl_.apply_buff.buff_formula = icy_veins_buff;
  db->effects.push_back(icy_veins_buff_appl_);
  size_t icy_veins_buff_appl = db->effects.size() - 1;

  Spell_Formula icy_veins;
  icy_veins.name = "Icy Veins";
  icy_veins.icon = Texture("../Assets/Icons/icy_veins.jpg");
  icy_veins.mana_cost = 20;
  icy_veins.range = 0;
  icy_veins.targets = Spell_Targets::Self;
  icy_veins.cooldown = 0.f;
  icy_veins.cast_time = 0;
  icy_veins.on_global_cooldown = false;
  icy_veins.effects.push_back(icy_veins_buff_appl);
  db->spells.push_back(icy_veins);

  // SPRINT

  CharMod sprint_modifier_;
  sprint_modifier_.type = Character_Modifier_Type::Speed;
  sprint_modifier_.speed.factor = 2.f;
  db->char_mods.push_back(sprint_modifier_);
  size_t sprint_modifier = db->char_mods.size() - 1;

  BuffDef sprint_buff_;
  sprint_buff_.name = "Sprint";
  sprint_buff_.duration = 15.f;
  sprint_buff_.icon = Texture("../Assets/Icons/sprint.jpg");
  sprint_buff_.char_mods.push_back(sprint_modifier_);
  db->buffs.push_back(sprint_buff_);
  size_t sprint_buff = db->buffs.size() - 1;

  Spell_Effect_Formula sprint_effect_;
  sprint_effect_.name = "Sprint";
  sprint_effect_.type = Spell_Effect_Type::Apply_Buff;
  sprint_effect_.apply_buff.buff_formula = sprint_buff;
  db->effects.push_back(sprint_effect_);
  size_t sprint_effect = db->effects.size() - 1;

  Spell_Formula sprint;
  sprint.name = "Sprint";
  sprint.cooldown = 10.f;
  sprint.icon = Texture("../Assets/Icons/sprint.jpg");
  sprint.mana_cost = 5;
  sprint.cast_time = 0.f;
  sprint.on_global_cooldown = false;
  sprint.targets = Spell_Targets::Self;
  sprint.effects.push_back(sprint_effect);
  db->spells.push_back(sprint);

  return db;
}
