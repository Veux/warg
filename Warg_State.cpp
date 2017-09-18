#include "Warg_State.h"
#include "Functions.h"
#include "Globals.h"
#include "Render.h"
#include "State.h"
#include <atomic>
#include <memory>
#include <sstream>
#include <thread>

using namespace glm;

static void invoke_spell_effect(SpellEffectInst *i,
                                std::vector<Character> &chars, uint32 nchars,
                                std::vector<SpellObjectInst> &objs);
static void cast_spell(int caster, int target, Spell *spell,
                       std::vector<Character> &chars, uint32 nchars,
                       std::vector<SpellObjectInst> &objs);
static void apply_char_mods(int c, std::vector<Character> &chars);
static void release_spell(int caster, int target, Spell *spell,
                          std::vector<Character> &chars, uint32 nchars,
                          std::vector<SpellObjectInst> &objs);
static void move_char(int c, vec3 v, std::vector<Character> &chars);
static void apply_spell_effects(int caster, int target,
                                std::vector<SpellEffect *> &effects,
                                std::vector<Character> &chars, uint32 nchars,
                                std::vector<SpellObjectInst> &objs);

Warg_State::Warg_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{
  Material_Descriptor material;
  material.albedo = "pebbles_diffuse.png";
  material.emissive = "";
  material.normal = "pebbles_normal.png";
  material.roughness = "pebbles_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "world_origin_distance.frag";

  ground_mesh = scene.add_primitive_mesh(plane, "ground_plane", material);
  ground_mesh->position = ground_pos;
  ground_mesh->scale = ground_dim;
  add_wall({0, 4, 0}, {6, 4}, 10);
  add_wall({6, 0, 0}, {6, 4}, 10);
  add_wall({6, 0, 0}, {10, 0}, 10);
  add_wall({10, 0, 0}, {10, 4}, 10);
  add_wall({10, 4, 0}, {16, 4}, 10);

  add_wall({14, 4, 0}, {16, 6}, 10);

  add_wall({16, 4, 0}, {16, 18}, 10);
  add_wall({16, 18, 0}, {10, 18}, 10);
  add_wall({10, 18, 0}, {10, 22}, 10);
  add_wall({10, 22, 0}, {6, 22}, 10);
  add_wall({6, 22, 0}, {6, 18}, 10);
  add_wall({6, 18, 0}, {0, 18}, 10);
  add_wall({0, 18, 0}, {0, 4}, 10);
  add_wall({2, 14, 0}, {2, 16}, 10);
  add_wall({2, 16, 0}, {4, 16}, 10);
  add_wall({4, 16, 0}, {4, 14}, 10);
  add_wall({4, 14, 0}, {2, 14}, 10);
  add_wall({12, 14, 0}, {12, 16}, 10);
  add_wall({12, 16, 0}, {14, 16}, 10);
  add_wall({14, 16, 0}, {14, 14}, 10);
  add_wall({14, 14, 0}, {12, 14}, 10);
  add_wall({12, 8, 0}, {14, 8}, 10);
  add_wall({14, 8, 0}, {14, 6}, 10);
  add_wall({14, 6, 0}, {12, 6}, 10);
  add_wall({12, 6, 0}, {12, 8}, 10);
  add_wall({4, 6, 0}, {4, 8}, 10);
  add_wall({4, 8, 0}, {2, 8}, 10);
  add_wall({2, 8, 0}, {2, 6}, 10);
  add_wall({2, 6, 0}, {4, 6}, 10);

  scene.lights.light_count = 1;
  Light *light = &scene.lights.lights[0];
  light->position = vec3{0, 0, 10};
  light->color = 30.0f * vec3(1.0f, 0.93f, 0.92f);
  light->attenuation = vec3(1.0f, .045f, .0075f);
  light->ambient = 0.02f;
  light->type = omnidirectional;

  material.albedo = "crate_diffuse.png";
  material.emissive = "test_emissive.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";

  // DIVINE SHIELD

  CharMod *ds_mod = &char_mods[nchar_mods++];
  ds_mod->type = CharModType::DamageTaken;
  ds_mod->damage_taken = DamageTakenMod{0};

  BuffDef *ds_buff = &buffs[nbuffs++];
  ds_buff->name = "DivineShieldBuff";
  ds_buff->duration = 10;
  ds_buff->char_mods.push_back(ds_mod);

  SpellEffect *ds_effect = &spell_effects[nspell_effects++];
  ds_effect->name = "DivineShieldEffect";
  ds_effect->type = SpellEffectType::ApplyBuff;
  ds_effect->applybuff.buff = ds_buff;

  SpellDef *ds = &spells[nspells++];
  ds->name = "Divine Shield";
  ds->mana_cost = 50;
  ds->range = -1;
  ds->targets = SpellTargets::Self;
  ds->cooldown = 5 * 60;
  ds->cast_time = 0;
  ds->effects.push_back(ds_effect);

  // SHADOW WORD: PAIN

  SpellEffect *swp_tick = &spell_effects[nspell_effects++];
  swp_tick->name = "ShadowWordPainTickEffect";
  swp_tick->type = SpellEffectType::Damage;
  swp_tick->damage.n = 5;
  swp_tick->damage.pierce_absorb = false;
  swp_tick->damage.pierce_mod = false;

  BuffDef *swp_buff = &buffs[nbuffs++];
  swp_buff->name = "ShadowWordPainBuff";
  swp_buff->duration = 5;
  swp_buff->tick_freq = 1.0 / 3;
  swp_buff->tick_effects.push_back(swp_tick);

  SpellEffect *swp_effect = &spell_effects[nspell_effects++];
  swp_effect->name = "ShadowWordPainEffect";
  swp_effect->type = SpellEffectType::ApplyDebuff;
  swp_effect->applydebuff.debuff = swp_buff;

  SpellDef *swp = &spells[nspells++];
  swp->name = "Shadow Word: Pain";
  swp->mana_cost = 50;
  swp->range = 30;
  swp->cooldown = 0;
  swp->cast_time = 0;
  swp->targets = SpellTargets::Hostile;
  swp->effects.push_back(swp_effect);

  // ARCANE EXPLOSION

  SpellEffect *ae_damage = &spell_effects[nspell_effects++];
  ae_damage->name = "ArcaneExplosionDamageEffect";
  ae_damage->type = SpellEffectType::Damage;
  ae_damage->damage.n = 5;
  ae_damage->damage.pierce_absorb = false;
  ae_damage->damage.pierce_mod = false;

  SpellEffect *ae_aoe = &spell_effects[nspell_effects++];
  ae_aoe->name = "ArcaneExplosionAoEEffect";
  ae_aoe->type = SpellEffectType::AoE;
  ae_aoe->aoe.targets = SpellTargets::Hostile;
  ae_aoe->aoe.radius = 8;
  ae_aoe->aoe.effect = ae_damage;

  SpellDef *ae = &spells[nspells++];
  ae->name = "Arcane Explosion";
  ae->mana_cost = 50;
  ae->range = 0;
  ae->cooldown = 0;
  ae->cast_time = 0;
  ae->targets = SpellTargets::Terrain;
  ae->effects.push_back(ae_aoe);

  // HOLY NOVA

  SpellEffect *hn_heal = &spell_effects[nspell_effects++];
  hn_heal->name = "HolyNovaHealEffect";
  hn_heal->type = SpellEffectType::Heal;
  hn_heal->heal.n = 10;

  SpellEffect *hn_aoe = &spell_effects[nspell_effects++];
  hn_aoe->name = "HolyNovaAoEEffect";
  hn_aoe->type = SpellEffectType::AoE;
  hn_aoe->aoe.targets = SpellTargets::Ally;
  hn_aoe->aoe.radius = 8;
  hn_aoe->aoe.effect = hn_heal;

  SpellDef *hn = &spells[nspells++];
  hn->name = "Holy Nova";
  hn->mana_cost = 50;
  hn->range = 0;
  hn->targets = SpellTargets::Terrain;
  hn->cooldown = 0;
  hn->cast_time = 0;
  hn->effects.push_back(hn_aoe);

  // SHADOW NOVA

  SpellEffect *shadow_nova_aoe = &spell_effects[nspell_effects++];
  shadow_nova_aoe->name = "ShadowNovaAoEEffect";
  shadow_nova_aoe->type = SpellEffectType::AoE;
  shadow_nova_aoe->aoe.targets = SpellTargets::Hostile;
  shadow_nova_aoe->aoe.radius = 8;
  shadow_nova_aoe->aoe.effect = swp_effect;

  SpellDef *shadow_nova = &spells[nspells++];
  shadow_nova->name = "Shadow Nova";
  shadow_nova->mana_cost = 50;
  shadow_nova->range = 0;
  shadow_nova->targets = SpellTargets::Terrain;
  shadow_nova->cooldown = 0;
  shadow_nova->cast_time = 0;
  shadow_nova->effects.push_back(shadow_nova_aoe);

  // MIND BLAST

  SpellEffect *mb_damage = &spell_effects[nspell_effects++];
  mb_damage->name = "MindBlastDamageEffect";
  mb_damage->type = SpellEffectType::Damage;
  mb_damage->damage.n = 20;
  mb_damage->damage.pierce_absorb = false;
  mb_damage->damage.pierce_mod = false;

  SpellDef *mb = &spells[nspells++];
  mb->name = "Mind Blast";
  mb->mana_cost = 100;
  mb->range = 15;
  mb->targets = SpellTargets::Hostile;
  mb->cooldown = 6;
  mb->cast_time = 1.5;
  mb->effects.push_back(mb_damage);

  // LESSER HEAL

  CharMod *lh_slow_mod = &char_mods[nchar_mods++];
  lh_slow_mod->type = CharModType::Speed;
  lh_slow_mod->speed.m = 0.5;

  BuffDef *lh_slow_debuff = &buffs[nbuffs++];
  lh_slow_debuff->name = "LesserHealSlowDebuff";
  lh_slow_debuff->duration = 10;
  lh_slow_debuff->char_mods.push_back(lh_slow_mod);

  SpellEffect *lh_slow_buff_appl = &spell_effects[nspell_effects++];
  lh_slow_buff_appl->name = "LesserHealSlowEffect";
  lh_slow_buff_appl->type = SpellEffectType::ApplyDebuff;
  lh_slow_buff_appl->applydebuff.debuff = lh_slow_debuff;

  SpellEffect *lh_heal = &spell_effects[nspell_effects++];
  lh_heal->name = "LesserHealHealEffect";
  lh_heal->type = SpellEffectType::Heal;
  lh_heal->heal.n = 20;

  SpellDef *lh = &spells[nspells++];
  lh->name = "Lesser Heal";
  lh->mana_cost = 50;
  lh->range = 30;
  lh->targets = SpellTargets::Ally;
  lh->cooldown = 0;
  lh->cast_time = 2;
  lh->effects.push_back(lh_heal);
  lh->effects.push_back(lh_slow_buff_appl);

  // ICY VEINS

  CharMod *icy_veins_mod = &char_mods[nchar_mods++];
  icy_veins_mod->type = CharModType::CastSpeed;
  icy_veins_mod->cast_speed.m = 3;

  BuffDef *icy_veins_buff = &buffs[nbuffs++];
  icy_veins_buff->name = "IcyVeinsBuff";
  icy_veins_buff->duration = 20;
  icy_veins_buff->char_mods.push_back(icy_veins_mod);

  SpellEffect *icy_veins_buff_appl = &spell_effects[nspell_effects++];
  icy_veins_buff_appl->name = "IcyVeinsBuffEffect";
  icy_veins_buff_appl->type = SpellEffectType::ApplyBuff;
  icy_veins_buff_appl->applybuff.buff = icy_veins_buff;

  SpellDef *icy_veins = &spells[nspells++];
  icy_veins->name = "Icy Veins";
  icy_veins->mana_cost = 20;
  icy_veins->range = 0;
  icy_veins->targets = SpellTargets::Self;
  icy_veins->cooldown = 120;
  icy_veins->cast_time = 0;
  icy_veins->effects.push_back(icy_veins_buff_appl);

  // ICE BLOCK

  SpellEffect *ib_clear_debuffs_effect = &spell_effects[nspell_effects++];
  ib_clear_debuffs_effect->name = "IceBlockClearDebuffsEffect";
  ib_clear_debuffs_effect->type = SpellEffectType::ClearDebuffs;

  SpellDef *ib = &spells[nspells++];
  ib->name = "Ice Block";
  ib->mana_cost = 20;
  ib->range = 0;
  ib->targets = SpellTargets::Self;
  ib->cooldown = 120;
  ib->cast_time = 0;
  ib->effects.push_back(ib_clear_debuffs_effect);

  // FROSTBOLT

  CharMod *fb_slow = &char_mods[nchar_mods++];
  fb_slow->type = CharModType::Speed;
  fb_slow->speed.m = 0.2;

  BuffDef *fb_debuff = &buffs[nbuffs++];
  fb_debuff->name = "FrostboltSlowDebuff";
  fb_debuff->duration = 10;
  fb_debuff->char_mods.push_back(fb_slow);

  SpellEffect *fb_debuff_appl = &spell_effects[nspell_effects++];
  fb_debuff_appl->name = "FrostboltDebuffApplyEffect";
  fb_debuff_appl->type = SpellEffectType::ApplyDebuff;
  fb_debuff_appl->applydebuff.debuff = fb_debuff;

  SpellEffect *fb_damage = &spell_effects[nspell_effects++];
  fb_damage->name = "FrostboltDamageEffect";
  fb_damage->type = SpellEffectType::Damage;
  fb_damage->damage.n = 15;
  fb_damage->damage.pierce_absorb = false;
  fb_damage->damage.pierce_mod = false;

  SpellObjectDef *fb_object = &spell_objects[nspell_objects++];
  fb_object->name = "Frostbolt";
  fb_object->speed = 30;
  fb_object->effects.push_back(fb_debuff_appl);
  fb_object->effects.push_back(fb_damage);

  SpellEffect *fb_object_launch = &spell_effects[nspell_effects++];
  fb_object_launch->name = "FrostboltObjectLaunchEffect";
  fb_object_launch->type = SpellEffectType::ObjectLaunch;
  fb_object_launch->objectlaunch.object = fb_object;

  SpellDef *fb = &spells[nspells++];
  fb->name = "Frostbolt";
  fb->mana_cost = 20;
  fb->range = 30;
  fb->targets = SpellTargets::Hostile;
  fb->cooldown = 0;
  fb->cast_time = 2;
  fb->effects.push_back(fb_object_launch);

  // COUNTERSPELL

  SpellEffect *cs_effect = &spell_effects[nspell_effects++];
  cs_effect->name = "Counterspell";
  cs_effect->type = SpellEffectType::Interrupt;
  cs_effect->interrupt.lockout = 6;

  SpellDef *cs = &spells[nspells++];
  cs->name = "Counterspell";
  cs->mana_cost = 10;
  cs->range = 30;
  cs->targets = SpellTargets::Hostile;
  cs->cooldown = 24;
  cs->cast_time = 0;
  cs->effects.push_back(cs_effect);

  add_char(0, "Eirich");
  add_char(0, "Veuxia");
  add_char(1, "Selion");
  add_char(1, "Veuxe");

  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();
}
void Warg_State::handle_input(State **current_state,
                              std::vector<State *> available_states)
{
  auto is_pressed = [](int key) {
    const static Uint8 *keys = SDL_GetKeyboardState(NULL);
    SDL_PumpEvents();
    return keys[key];
  };
  SDL_Event _e;
  while (SDL_PollEvent(&_e))
  {
    if (_e.type == SDL_QUIT)
    {
      running = false;
      return;
    }
    else if (_e.type == SDL_KEYDOWN)
    {
      if (_e.key.keysym.sym == SDLK_ESCAPE)
      {
        running = false;
        return;
      }
    }
    else if (_e.type == SDL_KEYUP)
    {

      if (_e.key.keysym.sym == SDLK_F1)
      {
        *current_state = &*available_states[0];
        return;
      }
      if (_e.key.keysym.sym == SDLK_F2)
      {
        *current_state = &*available_states[1];
        return;
      }
      if (_e.key.keysym.sym == SDLK_F5)
      {
        if (free_cam)
        {
          free_cam = false;
          pc = 0;
        }
        else if (pc >= nchars - 1)
        {
          free_cam = true;
        }
        else
        {
          pc += 1;
        }
      }
      if (_e.key.keysym.sym == SDLK_TAB && !free_cam)
      {
        int first_lower = -1, first_higher = -1;
        for (int i = nchars - 1; i >= 0; i--)
        {
          if (chars[i].alive && chars[i].team != chars[pc].team)
          {
            if (i < chars[pc].target)
            {
              first_lower = i;
            }
            else if (i > chars[pc].target)
            {
              first_higher = i;
            }
          }
        }

        if (first_higher != -1)
        {
          chars[pc].target = first_higher;
        }
        else if (first_lower != -1)
        {
          chars[pc].target = first_lower;
        }
        set_message("Target", s(chars[pc].name, " targeted ",
                                chars[chars[pc].target].name),
                    3.0f);
      }
      if (_e.key.keysym.sym == SDLK_1 && !free_cam)
      {
        cast_spell(pc, chars[pc].target, &chars[pc].spellbook["Frostbolt"],
                   chars, nchars, spell_objs);
      }
      if (_e.key.keysym.sym == SDLK_2 && !free_cam)
      {
        int target = -1;
        if (!chars[pc].target || chars[pc].team != chars[chars[pc].target].team)
        {
          target = pc;
        }
        else
        {
          target = chars[pc].target;
        }
        cast_spell(pc, target, &chars[pc].spellbook["Lesser Heal"], chars,
                   nchars, spell_objs);
      }
      if (_e.key.keysym.sym == SDLK_3 && !free_cam)
      {
        cast_spell(pc, chars[pc].target, &chars[pc].spellbook["Counterspell"],
                   chars, nchars, spell_objs);
      }
      if (_e.key.keysym.sym == SDLK_4 && !free_cam)
      {
        cast_spell(pc, pc, &chars[pc].spellbook["Ice Block"], chars, nchars,
                   spell_objs);
      }
      if (_e.key.keysym.sym == SDLK_5 && !free_cam)
      {
        cast_spell(pc, -1, &chars[pc].spellbook["Arcane Explosion"], chars,
                   nchars, spell_objs);
      }
      if (_e.key.keysym.sym == SDLK_6 && !free_cam)
      {
        cast_spell(pc, -1, &chars[pc].spellbook["Holy Nova"], chars, nchars,
                   spell_objs);
      }
    }
    else if (_e.type == SDL_MOUSEWHEEL)
    {
      if (_e.wheel.y < 0)
      {
        cam.zoom += ZOOM_STEP;
      }
      else if (_e.wheel.y > 0)
      {
        cam.zoom -= ZOOM_STEP;
      }
    }
    else if (_e.type == SDL_WINDOWEVENT)
    {
      if (_e.window.event == SDL_WINDOWEVENT_RESIZED)
      {
        // resize
      }
      else if (_e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
               _e.window.event == SDL_WINDOWEVENT_ENTER)
      { // dumping mouse delta prevents camera teleport on focus gain

        // required, else clicking the title bar itself to gain focus
        // makes SDL ignore the mouse entirely for some reason...
        SDL_SetRelativeMouseMode(SDL_bool(false));
        SDL_SetRelativeMouseMode(SDL_bool(true));
        reset_mouse_delta();
      }
    }
  }

  checkSDLError(__LINE__);
  // first person style camera control:
  const uint32 mouse_state =
      SDL_GetMouseState(&mouse_position.x, &mouse_position.y);

  // note: SDL_SetRelativeMouseMode is being set by the constructor
  ivec2 mouse_delta;
  SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
  bool last_seen_lmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool last_seen_rmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

  if (free_cam)
  {
    cam.theta += mouse_delta.x * MOUSE_X_SENS;
    cam.phi += mouse_delta.y * MOUSE_Y_SENS;
    // wrap x
    if (cam.theta > two_pi<float32>())
    {
      cam.theta = cam.theta - two_pi<float32>();
    }
    if (cam.theta < 0)
    {
      cam.theta = two_pi<float32>() + cam.theta;
    }
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (cam.phi > upper)
    {
      cam.phi = upper;
    }
    const float32 lower = -half_pi<float32>() + 100 * epsilon<float32>();
    if (cam.phi < lower)
    {
      cam.phi = lower;
    }

    mat4 rx = rotate(-cam.theta, vec3(0, 0, 1));
    vec4 vr = rx * vec4(0, 1, 0, 0);
    vec3 perp = vec3(-vr.y, vr.x, 0);
    mat4 ry = rotate(cam.phi, perp);
    cam.dir = normalize(vec3(ry * vr));

    if (is_pressed(SDL_SCANCODE_W))
    {
      cam.pos += MOVE_SPEED * cam.dir;
    }
    if (is_pressed(SDL_SCANCODE_S))
    {
      cam.pos -= MOVE_SPEED * cam.dir;
    }
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.x, vr.y, 0, 0);
      cam.pos += vec3(MOVE_SPEED * r * v);
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.x, vr.y, 0, 0);
      cam.pos += vec3(MOVE_SPEED * r * v);
    }
  }
  else
  { // wow style camera
    vec4 cam_rel;
    if ((left_button_down || right_button_down) &&
        (last_seen_lmb || last_seen_rmb))
    { // won't track mouse delta that happened when mouse button was not
      // pressed
      cam.theta += mouse_delta.x * MOUSE_X_SENS;
      cam.phi += mouse_delta.y * MOUSE_Y_SENS;
    }
    // wrap x
    if (cam.theta > two_pi<float32>())
    {
      cam.theta = cam.theta - two_pi<float32>();
    }
    if (cam.theta < 0)
    {
      cam.theta = two_pi<float32>() + cam.theta;
    }
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (cam.phi > upper)
    {
      cam.phi = upper;
    }
    const float32 lower = 100 * epsilon<float32>();
    if (cam.phi < lower)
    {
      cam.phi = lower;
    }

    //+x right, +y forward, +z up

    // construct a matrix that represents a rotation around the z axis by
    // theta(x) radians
    mat4 rx = rotate(-cam.theta, vec3(0, 0, 1));

    //'default' position of camera is behind the Character

    // rotate that vector by our rx matrix
    cam_rel = rx * normalize(vec4(0, -1, 0.0, 0));

    // perp is the camera-relative 'x' axis that moves with the camera
    // as the camera rotates around the Character-centered y axis
    // should always point towards the right of the screen
    vec3 perp = vec3(-cam_rel.y, cam_rel.x, 0);

    // construct a matrix that represents a rotation around perp by -phi
    mat4 ry = rotate(-cam.phi, perp);

    // rotate the camera vector around perp
    cam_rel = normalize(ry * cam_rel);

    vec3 old_pos = chars[pc].pos;

    if (right_button_down)
    {
      chars[pc].dir = normalize(-vec3(cam_rel.x, cam_rel.y, 0));
    }
    if (is_pressed(SDL_SCANCODE_W))
    {
      vec3 v = vec3(chars[pc].dir.x, chars[pc].dir.y, 0.0f);
      move_char(pc, dt * v, chars);
    }
    if (is_pressed(SDL_SCANCODE_S))
    {
      vec3 v = vec3(chars[pc].dir.x, chars[pc].dir.y, 0.0f);
      move_char(pc, dt * -v, chars);
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(chars[pc].dir.x, chars[pc].dir.y, 0, 0);
      move_char(pc, dt * vec3(r * v), chars);
    }
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(chars[pc].dir.x, chars[pc].dir.y, 0, 0);
      move_char(pc, dt * vec3(r * v), chars);
    }

    cam.pos = chars[pc].pos + vec3(cam_rel.x, cam_rel.y, cam_rel.z) * cam.zoom;
    cam.dir = -vec3(cam_rel);

    // if (intersects(player_pos, player_body, obstacle_pos, obstacle_body))
    //   if (!intersects(vec3(player_pos.x, old_pos.y, player_pos.z),
    //   player_body,
    //                   obstacle_pos, obstacle_body))
    //     player_pos.y = old_pos.y;
    //   else if (!intersects(vec3(old_pos.x, player_pos.y, old_pos.z),
    //                        player_body, obstacle_pos, obstacle_body))
    //     player_pos.x = old_pos.x;
    //   else
    //     player_pos = old_pos;
    // else
    for (auto &wall : walls)
    {
      if (intersects(chars[pc].pos, chars[pc].body, wall))
      {
        if (!intersects(vec3(chars[pc].pos.x, old_pos.y, chars[pc].pos.z),
                        chars[pc].body, wall))
        {
          chars[pc].pos.y = old_pos.y;
        }
        else if (!intersects(vec3(old_pos.x, chars[pc].pos.y, old_pos.z),
                             chars[pc].body, wall))
        {
          chars[pc].pos.x = old_pos.x;
        }
        else
        {
          chars[pc].pos = old_pos;
        }
      }
    }
  }

  vec3 falling_pos = chars[pc].pos;
  falling_pos.z -= 5.0 / 60;
  // if (!intersects(player_pos, player_dim, ground_pos, ground_dim))
  if (chars[pc].pos.z > chars[pc].body.h / 2)
    chars[pc].pos = falling_pos;

  previous_mouse_state = mouse_state;
}

void apply_spell_effects(int caster, int target,
                         std::vector<SpellEffect *> &effects,
                         std::vector<Character> &chars, uint32 nchars,
                         std::vector<SpellObjectInst> &objs)
{
  SpellEffectInst i;
  for (auto &e : effects)
  {
    i.def = *e;
    i.caster = caster;
    i.pos = {0, 0, 0};
    i.target = target;

    invoke_spell_effect(&i, chars, nchars, objs);
  }
}
void Warg_State::update()
{
  for (int i = 0; i < nchars; i++)
  {
    Character *c = &chars[i];

    c->mesh->position = c->pos;
    c->mesh->scale = vec3(1.0f);
    c->mesh->orientation =
        angleAxis((float32)atan2(c->dir.y, c->dir.x), vec3(0.f, 0.f, 1.f));

    if (c->target >= 0 && !chars[c->target].alive)
    {
      c->target = -1;
    }

    c->atk_cd -= dt;
    if (c->target >= 0 && chars[c->target].alive &&
        c->team != chars[c->target].team &&
        length(c->pos - chars[c->target].pos) <= ATK_RANGE && c->atk_cd <= 0)
    {
      // int edamage = chars[c.target].take_damage({c.atk_dmg, false, false});
      // c.atk_cd = c.atk_speed;
      // std::cout << c.name << " whacks " << chars[c.target].name << " for "
      //           << edamage << " damage" << std::endl;
    }

    for (auto j = c->buffs.begin(); j != c->buffs.end();)
    {
      auto &b = *j;
      BuffDef bdef = b.def;
      b.duration -= dt;
      if (!bdef.tick_effects.empty() &&
          b.duration * bdef.tick_freq < b.ticks_left)
      {
        apply_spell_effects(-1, i, bdef.tick_effects, chars, nchars,
                            spell_objs);
        b.ticks_left--;
      }
      if (b.duration <= 0)
      {
        set_message("", s(b.def.name, " falls off ", c->name), 3.0f);
        if (b.dynamic)
        {
          for (auto &te : b.def.tick_effects)
          {
            free(te);
          }
          for (auto &cm : b.def.char_mods)
          {
            free(cm);
          }
        }
        j = c->debuffs.erase(j);
      }
      else
      {
        j++;
      }
    }

    for (auto j = c->debuffs.begin(); j != c->debuffs.end();)
    {
      auto &d = *j;
      BuffDef ddef = d.def;
      d.duration -= dt;
      if (!ddef.tick_effects.empty() &&
          d.duration * ddef.tick_freq < d.ticks_left)
      {
        apply_spell_effects(-1, i, ddef.tick_effects, chars, nchars,
                            spell_objs);
        d.ticks_left--;
      }
      if (d.duration <= 0)
      {
        set_message("", d.def.name + " falls off " + c->name, 3.0f);
        if (d.dynamic)
        {
          for (auto &te : d.def.tick_effects)
          {
            free(te);
          }
          for (auto &cm : d.def.char_mods)
          {
            free(cm);
          }
        }
        j = c->debuffs.erase(j);
      }
      else
      {
        j++;
      }
    }

    apply_char_mods(i, chars);

    c->mana += c->e_stats.mana_regen * dt;
    if (c->mana > c->mana_max)
    {
      c->mana = c->mana_max;
    }

    if (c->casting)
    {
      c->cast_progress += c->e_stats.cast_speed * dt;
      if (c->cast_progress > c->casting_spell->def->cast_time)
      {
        release_spell(i, c->cast_target, c->casting_spell, chars, nchars,
                      spell_objs);
        c->cast_progress = 0;
        c->casting = false;
      }
    }

    if (c->gcd > 0)
    {
      c->gcd -= dt * c->e_stats.cast_speed;
      if (c->gcd < 0)
      {
        c->gcd = 0;
      }
    }

    for (auto &sp : c->spellbook)
    {
      auto &s = std::get<1>(sp);
      if (s.cd_remaining > 0)
      {
        s.cd_remaining -= dt;
      }
      if (s.cd_remaining < 0)
      {
        s.cd_remaining = 0;
      }
    }
  }

  for (auto i = spell_objs.begin(); i != spell_objs.end();)
  {
    auto &o = *i;
    float32 d = length(chars[o.target].pos - o.pos);
    if (d < 0.5)
    {
      set_message("",
                  chars[o.caster].name + "'s " + o.def.name + " hit " +
                      chars[o.target].name,
                  3.0f);
      for (auto &e : o.def.effects)
      {
        SpellEffectInst i;
        i.def = *e;
        i.caster = o.caster;
        i.pos = o.pos;
        i.target = o.target;
        invoke_spell_effect(&i, chars, nchars, spell_objs);
      }
      i = spell_objs.erase(i);
    }
    else
    {
      vec3 a = normalize(chars[o.target].pos - o.pos);
      a.x *= o.def.speed * dt;
      a.y *= o.def.speed * dt;
      a.z *= o.def.speed * dt;
      o.pos += a;
      ++i;
    }
  }
}

void Warg_State::add_wall(vec3 p1, vec2 p2, float32 h)
{
  Material_Descriptor material;
  material.albedo = "pebbles_diffuse.png";
  material.emissive = "";
  material.normal = "pebbles_normal.png";
  material.roughness = "pebbles_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "world_origin_distance.frag";
  material.backface_culling = false;
  material.uv_scale = vec2(2);

  Mesh_Data data;
  vec3 a, b, c, d;
  a = p1;
  b = vec3(p2.x, p2.y, 0);
  c = vec3(p2.x, p2.y, h);
  d = vec3(p1.x, p1.y, h);

  add_quad(a, b, c, d, data);
  auto mesh = scene.add_mesh(data, material, "some wall");

  walls.push_back(Wall{p1, p2, h});
  wall_meshes.push_back(mesh);
}

void Warg_State::add_char(int team, std::string name)
{
  vec3 pos, dir;
  switch (team)
  {
    case 0:
      pos = spawnloc0;
      dir = spawndir0;
      break;
    case 1:
      pos = spawnloc1;
      dir = spawndir1;
      break;
    default:
      return;
  };

  Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "test_emissive.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "world_origin_distance.frag";

  Character c;
  c.team = team;
  c.name = name;
  c.pos = pos;
  c.dir = dir;
  c.body = {1.f, 0.3f};
  c.mesh = scene.add_primitive_mesh(cube, "player_cube", material);
  c.hp_max = 100;
  c.hp = c.hp_max;
  c.mana_max = 500;
  c.mana = c.mana_max;

  CharStats s;
  s.gcd = 1.5;
  s.speed = 3.0;
  s.cast_speed = 1;
  s.hp_regen = 2;
  s.mana_regen = 10;
  s.damage_mod = 1;
  s.atk_dmg = 5;
  s.atk_speed = 2;

  c.b_stats = s;
  c.e_stats = s;

  for (int i = 0; i < nspells; i++)
  {
    Spell s;
    s.def = &spells[i];
    s.cd_remaining = 0;
    c.spellbook[s.def->name] = s;
  }

  chars.push_back(c);
  nchars++;
}

void invoke_spell_effect(SpellEffectInst *i, std::vector<Character> &chars,
                         uint32 nchars, std::vector<SpellObjectInst> &objs)
{
  ASSERT(i);
  SpellEffect *e = &i->def;
  switch (e->type)
  {
    case SpellEffectType::Heal:
    {
      if (!i->target)
      {
        set_message("Invalid Target", " heal failed: no target", 3.0f);
        return;
      }

      Character *c = &chars[i->target];
      HealEffect *h = &e->heal;

      if (!c->alive)
      {
        break;
      }

      uint32 effective = h->n;
      uint32 overheal = 0;

      c->hp += effective;

      if (c->hp > c->hp_max)
      {
        overheal = c->hp - c->hp_max;
        effective -= overheal;
        c->hp = c->hp_max;
      }

      if (overheal > 0)
      {
        set_message("", s(e->name, " healed ", c->name, " for ", effective,
                          " (", overheal, " overheal)"),
                    3.0f);
      }
      else
      {
        set_message("", s(e->name, " healed ", c->name, " for ", effective),
                    3.0f);
      }
      break;
    }
    case SpellEffectType::Damage:
    {
      Character *c = &chars[i->target];
      if (!c)
      {
        set_message("Invalid Target", " damage failed: no target", 3.0f);
        return;
      }

      if (!c->alive)
      {
        break;
      }

      int effective = e->damage.n;
      int overkill = 0;

      if (!e->damage.pierce_mod)
      {
        effective *= c->e_stats.damage_mod;
      }

      c->hp -= effective;

      if (c->hp < 0)
      {
        effective -= 0 - c->hp;
        overkill = -c->hp;
        c->hp = 0;
        c->alive = false;
      }

      if (overkill > 0)
      {
        set_message("", s(e->name, " hit ", c->name, " for ", effective,
                          " damage (", overkill, " overkill)"),
                    3.0f);
      }
      else
      {
        set_message("",
                    s(e->name, " hit ", c->name, " for ", effective, " damage"),
                    3.0f);
      }

      if (!c->alive)
      {
        set_message("", c->name + " has died", 3.0f);
      }
      break;
    }
    case SpellEffectType::ApplyBuff:
    {
      Character *c = &chars[i->target];
      if (!c)
      {
        set_message("Invalid Target", "buff application failed: no target",
                    3.0f);
        return;
      }

      auto buffdef = e->applybuff.buff;
      Buff buff;
      buff.def = *buffdef;
      buff.duration = buffdef->duration;
      buff.ticks_left =
          static_cast<int>(glm::floor(buffdef->duration * buffdef->tick_freq));
      c->buffs.push_back(buff);
      set_message("", s(e->name, " spell effect buff applied ", buffdef->name,
                        " to ", c->name),
                  3.0f);
      apply_char_mods(i->target, chars);
      break;
    }
    case SpellEffectType::ApplyDebuff:
    {
      Character *c = &chars[i->target];
      if (!c)
      {
        set_message("Invalid Target", "debuff application failed: no target",
                    3.0f);
        return;
      }
      auto buffdef = e->applydebuff.debuff;
      Buff debuff;
      debuff.def = *buffdef;
      debuff.duration = buffdef->duration;
      debuff.ticks_left =
          (uint32)(glm::floor(buffdef->duration * buffdef->tick_freq));
      c->debuffs.push_back(debuff);
      set_message("", s(e->name, " spell effect debuff applied ", buffdef->name,
                        " to ", c->name),
                  3.0f);
      apply_char_mods(i->target, chars);
      break;
    }
    case SpellEffectType::AoE:
    {
      for (int j = 0; j < nchars; j++)
      {
        Character *c = &chars[j];
        if (length(c->pos - i->pos) <= e->aoe.radius)
        {
          // bug: doesnt seem to be tracking already-hit characters
          SpellEffectInst k;
          k.def = *e->aoe.effect;
          k.caster = i->caster;
          k.pos = {0, 0, 0};
          k.target = j;

          if (c->team == chars[k.caster].team &&
              e->aoe.targets == SpellTargets::Ally)
          {
            invoke_spell_effect(&k, chars, nchars, objs);
          }
          else if (c->team != chars[k.caster].team &&
                   e->aoe.targets == SpellTargets::Hostile)
          {
            invoke_spell_effect(&k, chars, nchars, objs);
          }
        }
      }
      break;
    }
    case SpellEffectType::ClearDebuffs:
    {
      Character *c = &chars[i->target];
      set_message("", s(i->def.name, " cleared all debuffs from ", c->name),
                  3.0f);
      for (auto &d : c->debuffs)
      {
        if (d.dynamic)
        {
          for (auto &te : d.def.tick_effects)
          {
            free(te);
          }
          for (auto &cm : d.def.char_mods)
          {
            free(cm);
          }
        }
      }
      c->debuffs.clear();
      apply_char_mods(i->target, chars);
      break;
    }
    case SpellEffectType::ObjectLaunch:
    {
      SpellObjectInst obji;
      obji.def = *i->def.objectlaunch.object;
      obji.caster = i->caster;
      obji.target = i->target;
      obji.pos = i->pos;
      objs.push_back(obji);
      break;
    }
    case SpellEffectType::Interrupt:
    {
      Character *c = &chars[i->target];
      c->casting = false;
      c->casting_spell = nullptr;
      c->cast_progress = 0;
      c->cast_target = -1;

      CharMod *silence_charmod = (CharMod *)malloc(sizeof(*silence_charmod));
      silence_charmod->type = CharModType::Silence;

      BuffDef silence_buffdef;
      silence_buffdef.name = i->def.name;
      silence_buffdef.duration = i->def.interrupt.lockout;
      silence_buffdef.char_mods.push_back(silence_charmod);

      Buff silence;
      silence.def = silence_buffdef;
      silence.duration = silence.def.duration;
      silence.dynamic = true;

      c->debuffs.push_back(silence);

      set_message("", s(chars[i->caster].name, "'s ", i->def.name,
                        " interrupted ", c->name, "'s casting"),
                  3.0f);
      break;
    }
    default:
      break;
  };
}

void apply_char_mod(Character *c, CharMod &m)
{
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
}

void apply_char_mods(int ci, std::vector<Character> &chars)
{
  Character *c = &chars[ci];
  c->e_stats = c->b_stats;
  c->silenced = false;

  for (auto &b : c->buffs)
  {
    for (auto &m : b.def.char_mods)
    {
      apply_char_mod(c, *m);
    }
  }
  for (auto &b : c->debuffs)
  {
    for (auto &m : b.def.char_mods)
    {
      apply_char_mod(c, *m);
    }
  }
}

void cast_spell(int casteri, int targeti, Spell *spell,
                std::vector<Character> &chars, uint32 nchars,
                std::vector<SpellObjectInst> &objs)
{
  Character *caster = nullptr, *target = nullptr;
  if (casteri >= 0)
    caster = &chars[casteri];
  if (targeti >= 0)
    target = &chars[targeti];
  if (caster->silenced)
  {
    set_message("Cast failed:", "silenced", 3.0f);
    return;
  }
  if (spell->cd_remaining > 0)
  {
    set_message("Cast failed:", s(caster->name, " failed to cast ",
                                  spell->def->name, ": on cooldown (",
                                  spell->cd_remaining, "s remaining)"),
                3.0f);
    return;
  }
  if (caster->gcd > 0)
  {
    set_message("Cast failed:", s(caster->name, " failed to cast ",
                                  spell->def->name, ": on gcd (", caster->gcd,
                                  "s remaining)"),
                3.0f);
    return;
  }
  if (spell->def->mana_cost > caster->mana)
  {
    set_message("Cast failed:", s(caster->name, " failed to cast ",
                                  spell->def->name, ": not enough mana (costs ",
                                  spell->def->mana_cost, ", have ",
                                  caster->mana, ")"),
                3.0f);

    return;
  }
  if (!target && spell->def->targets != SpellTargets::Terrain)
  {
    set_message("Cast failed:", s(caster->name, " failed to cast ",
                                  spell->def->name, ": no target"),
                3.0f);
    return;
  }
  if (spell->def->targets != SpellTargets::Terrain &&
      length(caster->pos - target->pos) > spell->def->range)
  {
    set_message("Cast failed:",
                s(caster->name, " failed to cast ", spell->def->name,
                  ": target out of range (spell range is ", spell->def->range,
                  ", target is ", length(caster->pos - target->pos), "m away)"),
                3.0f);
    return;
  }
  if (caster->casting)
  {
    set_message("Cast failed:", s(caster->name, " failed to cast ",
                                  spell->def->name, ": already casting"),
                3.0f);
    return;
  }

  if (spell->def->cast_time > 0)
  {
    caster->casting = true;
    caster->casting_spell = spell;
    caster->cast_progress = 0;
    if (target)
    {
      caster->cast_target = targeti;
      set_message("Begin cast:", s(caster->name, " begins casting ",
                                   spell->def->name, " (",
                                   spell->def->cast_time, "s remaining)"),
                  3.0f);
    }
    else
    {
      set_message("Begin cast:", s(caster->name, " begins casting ",
                                   spell->def->name, " at ", target->name, " (",
                                   spell->def->cast_time, "s remaining)"),
                  3.0f);
    }
  }
  else
  {
    release_spell(casteri, targeti, spell, chars, nchars, objs);
  }
}

void release_spell(int casteri, int targeti, Spell *spell,
                   std::vector<Character> &chars, uint32 nchars,
                   std::vector<SpellObjectInst> &objs)
{
  ASSERT(spell);
  ASSERT(spell->def);

  Character *caster = nullptr, *target = nullptr;
  if (casteri >= 0)
    caster = &chars[casteri];
  if (targeti >= 0)
    target = &chars[targeti];

  if (caster->mana < spell->def->mana_cost)
  {
    set_message("Cast failed:", s(caster->name, " failed to cast ",
                                  spell->def->name, ": not enough mana (costs ",
                                  spell->def->mana_cost, ", have ",
                                  caster->mana, ")"),
                3.0f);
    return;
  }

  if (spell->def->targets != SpellTargets::Terrain &&
      glm::length(caster->pos - target->pos) > spell->def->range)
  {
    set_message("Cast failed:",
                s(caster->name, " failed to cast ", spell->def->name,
                  ": target out of range (spell range is ", spell->def->range,
                  ", target is ", length(caster->pos - target->pos), "m away)"),
                3.0f);
    return;
  }

  if (spell->def->targets == SpellTargets::Self)
  {
    if (caster != target)
    {
      set_message("Cast failed:", s(caster->name, " failed to cast ",
                                    spell->def->name,
                                    ": can only cast at self"),
                  3.0f);
      return;
    }
    set_message("Cast success:", s(caster->name, " casts ", spell->def->name),
                3.0f);
  }

  if (spell->def->targets == SpellTargets::Ally)
  {
    if (caster->team != target->team)
    {
      set_message("Cast failed:", s(caster->name, " failed to cast ",
                                    spell->def->name,
                                    ": can only cast at allies"),
                  3.0f);
      return;
    }

    set_message("Cast success:", s(caster->name, " casts ", spell->def->name,
                                   " at ", target->name),
                3.0f);
    apply_spell_effects(casteri, targeti, spell->def->effects, chars, nchars,
                        objs);
  }

  if (spell->def->targets == SpellTargets::Hostile)
  {
    if (caster->team == target->team)
    {
      set_message("Cast failed:", s(caster->name, " failed to cast ",
                                    spell->def->name,
                                    ": can only cast at hostile targets"),
                  3.0f);
      return;
    }
    set_message("Cast success:", s(caster->name, " casts ", spell->def->name,
                                   " at ", target->name),
                3.0f);
    apply_spell_effects(casteri, targeti, spell->def->effects, chars, nchars,
                        objs);
  }

  if (spell->def->targets == SpellTargets::Terrain)
  {
    set_message("Cast success:", s(caster->name, " casts ", spell->def->name),
                3.0f);
    apply_spell_effects(casteri, targeti, spell->def->effects, chars, nchars,
                        objs);
  }

  caster->mana -= spell->def->mana_cost;
  if (caster->mana < 0)
    caster->mana = 0;

  caster->gcd = spell->def->cast_time < caster->b_stats.gcd
                    ? caster->b_stats.gcd - spell->def->cast_time
                    : 0;

  spell->cd_remaining = spell->def->cooldown;
}

void move_char(int ci, vec3 v, std::vector<Character> &chars)
{
  Character *c = &chars[ci];
  ASSERT(c);
  if (c->casting)
  {
    c->casting = false;
    c->cast_progress = 0;
    set_message("Interrupt:", s(c->name, "'s casting interrupted: moved"),
                3.0f);
  }
  c->pos += c->e_stats.speed * v;
}
