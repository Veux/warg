#include "stdafx.h"
#include "Warg_Event.h"
#include "Warg_Common.h"

void Buffer::reserve(size_t size)
{
  data.reserve(wnext + size);
}

void Buffer::insert(void *d, size_t size)
{
  data.resize(wnext + size);
  memcpy(&data[wnext], d, size);
  wnext += size;
}

void *Buffer::read(size_t size)
{
  ASSERT(rnext + size <= data.size());
  uint8_t *p = &data[rnext];
  rnext += size;
  return p;
}

void serialize_(Buffer &b, uint8_t n)
{
  b.insert(&n, sizeof(n));
}

void serialize_(Buffer &b, int32_t n)
{
  b.insert(&n, sizeof(n));
}

void serialize_(Buffer &b, uint32_t n)
{
  b.insert(&n, sizeof(n));
}

void serialize_(Buffer& b, size_t n) {
  b.insert(&n, sizeof n);
}

void serialize_(Buffer &b, const char *s)
{
  ASSERT(s);
  size_t len = strlen(s);
  b.reserve((sizeof len) + len);
  b.insert(&len, sizeof len);
  b.insert((void *)s, len);
}

void serialize_(Buffer &b, std::string &s)
{
  serialize_(b, s.c_str());
}

void serialize_(Buffer& b, std::string_view s) {
  std::string str(s);
  serialize_(b, str);
}

void serialize_(Buffer& b, bool p) {
  b.insert(&p, sizeof p);
}

void serialize_(Buffer &b, float32_t a)
{
  b.insert(&a, sizeof(a));
}

void serialize_(Buffer &b, float64 a)
{
  b.insert(&a, sizeof(a));
}

void serialize_(Buffer &b, vec3 v)
{
  serialize_(b, v.x);
  serialize_(b, v.y);
  serialize_(b, v.z);
}

void serialize_(Buffer& b, quat q)
{
  serialize_(b, q.w);
  serialize_(b, q.x);
  serialize_(b, q.y);
  serialize_(b, q.z);
}

void serialize_(Buffer& b, Input &i)
{
  serialize_(b, i.m);
  serialize_(b, i.orientation);
}

void serialize_(Buffer& b, Character_Physics& cp) {
  serialize_(b, cp.position);
  serialize_(b, cp.orientation);
  serialize_(b, cp.velocity);
  serialize_(b, cp.grounded);
}

void serialize_(Buffer& b, Character_Stats& cs) {
  serialize_(b, cs.global_cooldown);
  serialize_(b, cs.speed);
  serialize_(b, cs.cast_speed);
  serialize_(b, cs.cast_damage);
  serialize_(b, cs.mana_regen);
  serialize_(b, cs.hp_regen);
  serialize_(b, cs.damage_mod);
  serialize_(b, cs.atk_dmg);
  serialize_(b, cs.atk_speed);
}

void serialize_(Buffer& b, Character& c) {
  serialize_(b, c.id);
  serialize_(b, c.physics);
  serialize_(b, c.radius);
  serialize_(b, c.name);
  serialize_(b, c.team);
  serialize_(b, c.base_stats);
}

void serialize_(Buffer& b, Living_Character& lc) {
  serialize_(b, lc.id);
  serialize_(b, lc.effective_stats);
  serialize_(b, lc.hp);
  serialize_(b, lc.hp_max);
  serialize_(b, lc.mana);
  serialize_(b, lc.mana_max);
}

void serialize_(Buffer& b, Character_Target& ct) {
  serialize_(b, ct.character);
  serialize_(b, ct.target);
}

void serialize_(Buffer& b, Character_Cast& cc) {
  serialize_(b, cc.caster);
  serialize_(b, cc.target);
  serialize_(b, cc.spell);
  serialize_(b, cc.progress);
}

void serialize_(Buffer& b, Spell_Cooldown& sc) {
  serialize_(b, sc.character);
  serialize_(b, sc.spell);
  serialize_(b, sc.cooldown_remaining);
}

void serialize_(Buffer& b, Character_Spell& cs) {
  serialize_(b, cs.character);
  serialize_(b, cs.spell);
}

void serialize_(Buffer& b, Character_Buff& cb) {
  serialize_(b, cb.character);
  serialize_(b, cb.buff_id);
  serialize_(b, cb.caster);
  serialize_(b, cb.duration);
  serialize_(b, cb.time_since_last_tick);
}

void serialize_(Buffer& b, Character_Silencing& cs) {
  serialize_(b, cs.character);
  serialize_(b, cs.buff_id);
}

void serialize_(Buffer& b, Character_GCD& cg) {
  serialize_(b, cg.character);
  serialize_(b, cg.remaining);
}

void serialize_(Buffer& b, Spell_Object& so) {
  serialize_(b, so.id);
  serialize_(b, so.spell_id);
  serialize_(b, so.caster);
  serialize_(b, so.target);
  serialize_(b, so.pos);
}

void serialize_(Buffer& b, Demonic_Circle& dc) {
  serialize_(b, dc.owner);
  serialize_(b, dc.position);
}

void serialize_(Buffer& b, Seed_of_Corruption& soc) {
  serialize_(b, soc.character);
  serialize_(b, soc.caster);
  serialize_(b, soc.damage_taken);
}

template <typename T> void serialize_(Buffer &b, std::vector<T> vec)
{
  serialize_(b, vec.size());
  for (T &v : vec)
    serialize_(b, v);
}

void serialize_(Buffer& b, Game_State& gs) {
  serialize_(b, gs.tick);
  serialize_(b, gs.characters);
  serialize_(b, gs.living_characters);
  serialize_(b, gs.character_targets);
  serialize_(b, gs.character_spells);
  serialize_(b, gs.character_casts);
  serialize_(b, gs.spell_cooldowns);
  serialize_(b, gs.character_buffs);
  serialize_(b, gs.character_silencings);
  serialize_(b, gs.character_gcds);
  serialize_(b, gs.spell_objects);
  serialize_(b, gs.demonic_circles);
  serialize_(b, gs.seeds_of_corruption);
}

void deserialize(Buffer &b, bool &p)
{
  p = *(bool *)b.read(1);
}

void deserialize(Buffer &b, uint8 &n)
{
  n = *(uint8 *)b.read(sizeof(n));
}

void deserialize(Buffer &b, int32 &n)
{
  n = *(int32 *)b.read(sizeof(n));
}

void deserialize(Buffer &b, uint16 &n)
{
  n = *(uint32 *)b.read(sizeof(n));
}

void deserialize(Buffer &b, uint32 &n)
{
  n = *(uint32 *)b.read(sizeof(n));
}

void deserialize(Buffer& b, size_t& n) {
  n = *(size_t *)b.read(sizeof n);
}

void deserialize(Buffer &b, std::string &s)
{
  size_t len;
  deserialize(b, len);
  s = std::string((char *)b.read(len), len);
}

void deserialize(Buffer &b, float32 &x)
{
  x = *(float32 *)b.read(sizeof(float32));
}

void deserialize(Buffer &b, float64 &x)
{
  x = *(float64 *)b.read(sizeof(float64));
}

void deserialize(Buffer &b, vec3 &v)
{
  deserialize(b, v.x);
  deserialize(b, v.y);
  deserialize(b, v.z);
}

void deserialize(Buffer &b, quat &q)
{
  deserialize(b, q.w);
  deserialize(b, q.x);
  deserialize(b, q.y);
  deserialize(b, q.z);
}

void deserialize(Buffer &b, Input &i)
{
  deserialize(b, i.m);
  deserialize(b, i.orientation);
}

void deserialize(Buffer &b, Character_Physics &cp)
{
  deserialize(b, cp.position);
  deserialize(b, cp.orientation);
  deserialize(b, cp.velocity);
  deserialize(b, cp.grounded);
}

void deserialize(Buffer &b, Character_Stats &cs)
{
  deserialize(b, cs.global_cooldown);
  deserialize(b, cs.speed);
  deserialize(b, cs.cast_speed);
  deserialize(b, cs.cast_damage);
  deserialize(b, cs.mana_regen);
  deserialize(b, cs.hp_regen);
  deserialize(b, cs.damage_mod);
  deserialize(b, cs.atk_dmg);
  deserialize(b, cs.atk_speed);
}

void deserialize(Buffer &b, Character &c)
{
  deserialize(b, c.id);
  deserialize(b, c.physics);
  deserialize(b, c.radius);
  deserialize(b, c.name);
  deserialize(b, c.team);
  deserialize(b, c.base_stats);
}

void deserialize(Buffer &b, Living_Character &lc)
{
  deserialize(b, lc.id);
  deserialize(b, lc.effective_stats);
  deserialize(b, lc.hp);
  deserialize(b, lc.hp_max);
  deserialize(b, lc.mana);
  deserialize(b, lc.mana_max);
}

void deserialize(Buffer &b, Character_Target &ct)
{
  deserialize(b, ct.character);
  deserialize(b, ct.target);
}

void deserialize(Buffer &b, Character_Cast &cc)
{
  deserialize(b, cc.caster);
  deserialize(b, cc.target);
  deserialize(b, cc.spell);
  deserialize(b, cc.progress);
}

void deserialize(Buffer &b, Spell_Cooldown &sc)
{
  deserialize(b, sc.character);
  deserialize(b, sc.spell);
  deserialize(b, sc.cooldown_remaining);
}

void deserialize(Buffer &b, Character_Spell &cs)
{
  deserialize(b, cs.character);
  deserialize(b, cs.spell);
}

void deserialize(Buffer &b, Character_Buff &cb)
{
  deserialize(b, cb.character);
  deserialize(b, cb.buff_id);
  deserialize(b, cb.caster);
  deserialize(b, cb.duration);
  deserialize(b, cb.time_since_last_tick);
}

void deserialize(Buffer& b, Character_Silencing& cs) {
  deserialize(b, cs.character);
  deserialize(b, cs.buff_id);
}

void deserialize(Buffer& b, Character_GCD& cg) {
  deserialize(b, cg.character);
  deserialize(b, cg.remaining);
}

void deserialize(Buffer& b, Spell_Object& so) {
  deserialize(b, so.id);
  deserialize(b, so.spell_id);
  deserialize(b, so.caster);
  deserialize(b, so.target);
  deserialize(b, so.pos);
}

void deserialize(Buffer& b, Demonic_Circle& dc) {
  deserialize(b, dc.owner);
  deserialize(b, dc.position);
}

void deserialize(Buffer& b, Seed_of_Corruption& soc) {
  deserialize(b, soc.character);
  deserialize(b, soc.caster);
  deserialize(b, soc.damage_taken);
}

template <typename T> void deserialize(Buffer& b, std::vector<T>& vec) {
  size_t size;
  deserialize(b, size);
  vec.resize(size);
  for (auto &v : vec)
    deserialize(b, v);
}

void deserialize(Buffer &b, Game_State &gs)
{
  deserialize(b, gs.tick);
  deserialize(b, gs.characters);
  deserialize(b, gs.living_characters);
  deserialize(b, gs.character_targets);
  deserialize(b, gs.character_spells);
  deserialize(b, gs.character_casts);
  deserialize(b, gs.spell_cooldowns);
  deserialize(b, gs.character_buffs);
  deserialize(b, gs.character_silencings);
  deserialize(b, gs.character_gcds);
  deserialize(b, gs.spell_objects);
  deserialize(b, gs.demonic_circles);
  deserialize(b, gs.seeds_of_corruption);
}