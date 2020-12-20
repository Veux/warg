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

Spell_Database::Spell_Database()
{
  UID id;

  // FROSTBOLT

  id = uid();
  buff_name[id] = "Frostbolt";
  buff_by_name["Frostbolt"] = id;
  buff_icon[id] = "frostbolt.jpg";
  buff_duration[id] = 10;
  buff_stats_modifiers[id].speed = 0.2;
  buff_is_debuff.insert(id);

  id = uid();
  sobj_name[id] = "Frostbolt";
  sobj_by_name["Frostbolt"] = id;
  sobj_speed[id] = 30;
  sobj_hit_damage[id] = 10;
  sobj_hit_buff_application[id] = buff_by_name["Frostbolt"];

  id = uid();
  name[id] = "Frostbolt";
  by_name["Frostbolt"] = id;
  mana_cost[id] = 20;
  range[id] = 30;
  targets[id] = Spell_Targets::Hostile;
  cast_time[id] = 1.5f;
  on_gcd.insert(id);
  icon[id] = "frostbolt.jpg";
  spell_release_object_creation[id] = sobj_by_name["Frostbolt"];

  // BLINK

  id = uid();
  name[id] = "Blink";
  by_name["Blink"] = id;
  icon[id] = "blink.jpg";
  mana_cost[id] = 5;
  targets[id] = Spell_Targets::Self;
  cooldown[id] = 15.f;
  spell_release_blink.insert(id);

  // SHADOW WORD: PAIN

  id = uid();
  buff_name[id] = "Shadow Word: Pain";
  buff_by_name["Shadow Word: Pain"] = id;
  buff_icon[id] = "shadow_word_pain.jpg";
  buff_duration[id] = 15;
  buff_tick_freq[id] = 1.0 / 3;
  buff_is_debuff.insert(id);
  buff_tick_damage[id] = 5;

  id = uid();
  name[id] = "Shadow Word: Pain";
  by_name["Shadow Word: Pain"] = id;
  icon[id] = "shadow_word_pain.jpg";
  mana_cost[id] = 50;
  range[id] = 30;
  on_gcd.insert(id);
  targets[id] = Spell_Targets::Hostile;
  spell_release_buff_application[id].push_back(buff_by_name["Shadow Word: Pain"]);

  // ICY VEINS

  id = uid();
  buff_name[id] = "Icy Veins";
  buff_by_name["Icy Veins"] = id;
  buff_icon[id] = "icy_veins.jpg";
  buff_duration[id] = 20;
  buff_stats_modifiers[id].cast_speed = 2.f;

  id = uid();
  name[id] = "Icy Veins";
  by_name["Icy Veins"] = id;
  icon[id] = "icy_veins.jpg";
  mana_cost[id] = 20;
  targets[id] = Spell_Targets::Self;
  spell_release_buff_application[id].push_back(buff_by_name["Icy Veins"]);

  // SPRINT

  id = uid();
  buff_name[id] = "Sprint";
  buff_by_name["Sprint"] = id;
  buff_icon[id] = "sprint.jpg";
  buff_duration[id] = 15;
  buff_stats_modifiers[id].speed = 2.f;

  id = uid();
  name[id] = "Sprint";
  by_name["Sprint"] = id;
  cooldown[id] = 10.f;
  icon[id] = "sprint.jpg";
  mana_cost[id] = 5;
  targets[id] = Spell_Targets::Self;
  spell_release_buff_application[id].push_back(buff_by_name["Sprint"]);

  // DEMONIC CIRCLE: SUMMON

  id = uid();
  buff_name[id] = "Demonic Circle";
  buff_by_name["Demonic Circle"] = id;
  buff_icon[id] = "demonic_circle_summon.jpg";
  buff_duration[id] = 3600.f;
  buff_end_demonic_circle_destroy.insert(id);

  id = uid();
  name[id] = "Demonic Circle: Summon";
  by_name["Demonic Circle: Summon"] = id;
  cooldown[id] = 30.f;
  icon[id] = "demonic_circle_summon.jpg";
  mana_cost[id] = 5;
  on_gcd.insert(id);
  targets[id] = Spell_Targets::Self;
  spell_release_demonic_circle_summon.insert(id);

  // DEMONIC CIRCLE: TELEPORT

  id = uid();
  name[id] = "Demonic Circle: Teleport";
  by_name["Demonic Circle: Teleport"] = id;
  cooldown[id] = 5.f;
  icon[id] = "demonic_circle_teleport.jpg";
  mana_cost[id] = 5;
  targets[id] = Spell_Targets::Self;
  spell_release_demonic_circle_teleport.insert(id);

  // CORRUPTION

  id = uid();
  buff_name[id] = "Corruption";
  buff_by_name["Corruption"] = id;
  buff_icon[id] = "corruption.jpg";
  buff_duration[id] = 15;
  buff_tick_freq[id] = 1.0 / 3;
  buff_tick_damage[id] = 5;
  buff_is_debuff.insert(id);

  id = uid();
  name[id] = "Corruption";
  by_name["Corruption"] = id;
  icon[id] = "corruption.jpg";
  mana_cost[id] = 50;
  range[id] = 30;
  on_gcd.insert(id);
  targets[id] = Spell_Targets::Hostile;
  spell_release_buff_application[id].push_back(buff_by_name["Corruption"]);

  // SEED OF CORRUPTION

  id = uid();
  buff_name[id] = "Seed of Corruption";
  buff_by_name["Seed of Corruption"] = id;
  buff_icon[id] = "seed_of_corruption.jpg";
  buff_duration[id] = 20.f;
  buff_end_seed_of_corruption.insert(id);
  buff_damage_seed_of_corruption.insert(id);
  buff_is_debuff.insert(id);

  id = uid();
  sobj_name[id] = "Seed of Corruption";
  sobj_by_name["Seed of Corruption"] = id;
  sobj_speed[id] = 30;
  sobj_hit_seed_of_corruption.insert(id);

  id = uid();
  name[id] = "Seed of Corruption";
  by_name["Seed of Corruption"] = id;
  cooldown[id] = 5.f;
  icon[id] = "seed_of_corruption.jpg";
  mana_cost[id] = 5;
  range[id] = 30;
  cast_time[id] = 1.f;
  on_gcd.insert(id);
  targets[id] = Spell_Targets::Hostile;
  spell_release_object_creation[id] = sobj_by_name["Seed of Corruption"];
}
