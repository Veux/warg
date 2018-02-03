#include "Spell.h"

std::unique_ptr<SpellDB> make_spell_db()
{
  auto db = std::make_unique<SpellDB>();;
  db->spells.resize(100);
  db->effects.resize(100);
  db->objects.resize(100);
  db->buffs.resize(100);
  db->char_mods.resize(100);

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

  // // ICY VEINS

  // CharMod *icy_veins_mod = &char_mods[nchar_mods++];
  // icy_veins_mod->type = CharModType::CastSpeed;
  // icy_veins_mod->cast_speed.m = 3;

  // BuffDef *icy_veins_buff = &buffs[nbuffs++];
  // icy_veins_buff->name = "IcyVeinsBuff";
  // icy_veins_buff->duration = 20;
  // icy_veins_buff->char_mods.push_back(icy_veins_mod);

  // SpellEffect *icy_veins_buff_appl = &spell_effects[nspell_effects++];
  // icy_veins_buff_appl->name = "IcyVeinsBuffEffect";
  // icy_veins_buff_appl->type = SpellEffectType::ApplyBuff;
  // icy_veins_buff_appl->applybuff.buff = icy_veins_buff;

  // SpellDef *icy_veins = &spells[nspells++];
  // icy_veins->name = "Icy Veins";
  // icy_veins->mana_cost = 20;
  // icy_veins->range = 0;
  // icy_veins->targets = SpellTargets::Self;
  // icy_veins->cooldown = 120;
  // icy_veins->cast_time = 0;
  // icy_veins->effects.push_back(icy_veins_buff_appl);

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

  // // COUNTERSPELL

  // SpellEffect *cs_effect = &spell_effects[nspell_effects++];
  // cs_effect->name = "Counterspell";
  // cs_effect->type = SpellEffectType::Interrupt;
  // cs_effect->interrupt.lockout = 6;

  // SpellDef *cs = &spells[nspells++];
  // cs->name = "Counterspell";
  // cs->mana_cost = 10;
  // cs->range = 30;
  // cs->targets = SpellTargets::Hostile;
  // cs->cooldown = 24;
  // cs->cast_time = 0;
  // cs->effects.push_back(cs_effect);

  CharMod fb_slow_;
  fb_slow_.type = CharModType::Speed;
  fb_slow_.speed.m = 0.2;
  db->char_mods.push_back(fb_slow_);
  CharMod *fb_slow = &db->char_mods.back();

  BuffDef fb_debuff_;
  fb_debuff_.name = "FrostboltSlowDebuff";
  fb_debuff_.duration = 10;
  fb_debuff_.char_mods.push_back(fb_slow);
  db->buffs.push_back(fb_debuff_);
  BuffDef *fb_debuff = &db->buffs.back();

  SpellEffect fb_debuff_appl_;
  fb_debuff_appl_.name = "FrostboltDebuffApplyEffect";
  fb_debuff_appl_.type = SpellEffectType::ApplyDebuff;
  fb_debuff_appl_.applydebuff.debuff = fb_debuff;
  db->effects.push_back(fb_debuff_appl_);
  SpellEffect *fb_debuff_appl = &db->effects.back();

  SpellEffect fb_damage_;
  fb_damage_.name = "FrostboltDamageEffect";
  fb_damage_.type = SpellEffectType::Damage;
  fb_damage_.damage.n = 15;
  fb_damage_.damage.pierce_absorb = false;
  fb_damage_.damage.pierce_mod = false;
  db->effects.push_back(fb_damage_);
  SpellEffect *fb_damage = &db->effects.back();

  int fb_object = db->objects.size();
  SpellObjectDef fb_object_;
  fb_object_.name = "Frostbolt";
  fb_object_.speed = 30;
  fb_object_.effects.push_back(fb_debuff_appl);
  fb_object_.effects.push_back(fb_damage);
  db->objects.push_back(fb_object_);

  SpellEffect fb_object_launch_;
  fb_object_launch_.name = "FrostboltObjectLaunchEffect";
  fb_object_launch_.type = SpellEffectType::ObjectLaunch;
  fb_object_launch_.objectlaunch.object = fb_object;
  db->effects.push_back(fb_object_launch_);
  SpellEffect *fb_object_launch = &db->effects.back();

  SpellDef frostbolt_;
  frostbolt_.name = "Frostbolt";
  frostbolt_.mana_cost = 20;
  frostbolt_.range = 30;
  frostbolt_.targets = SpellTargets::Hostile;
  frostbolt_.cooldown = 0;
  frostbolt_.cast_time = 0;
  frostbolt_.effects.push_back(fb_object_launch);
  db->spells.push_back(frostbolt_);

  return db;
}
