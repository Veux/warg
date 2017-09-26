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
  add_wall({0, 8, 0}, {12, 8}, 20);
  add_wall({12, 0, 0}, {12, 8}, 20);
  add_wall({12, 0, 0}, {20, 0}, 20);
  add_wall({20, 0, 0}, {20, 8}, 20);
  add_wall({20, 8, 0}, {32, 8}, 20);
  add_wall({28, 8, 0}, {32, 12}, 20);
  add_wall({32, 8, 0}, {32, 36}, 20);
  add_wall({32, 36, 0}, {20, 36}, 20);
  add_wall({20, 36, 0}, {20, 44}, 20);
  add_wall({20, 44, 0}, {12, 44}, 20);
  add_wall({12, 44, 0}, {12, 36}, 20);
  add_wall({12, 36, 0}, {0, 36}, 20);
  add_wall({0, 36, 0}, {0, 8}, 20);
  add_wall({4, 28, 0}, {4, 32}, 20);
  add_wall({4, 32, 0}, {8, 32}, 20);
  add_wall({8, 32, 0}, {8, 28}, 20);
  add_wall({8, 28, 0}, {4, 28}, 20);
  add_wall({24, 28, 0}, {24, 32}, 20);
  add_wall({24, 32, 0}, {28, 32}, 20);
  add_wall({28, 32, 0}, {28, 28}, 20);
  add_wall({28, 28, 0}, {24, 28}, 20);
  add_wall({24, 16, 0}, {28, 16}, 20);
  add_wall({28, 16, 0}, {28, 12}, 20);
  add_wall({28, 12, 0}, {24, 12}, 20);
  add_wall({24, 12, 0}, {24, 16}, 20);
  add_wall({8, 12, 0}, {8, 16}, 20);
  add_wall({8, 16, 0}, {4, 16}, 20);
  add_wall({4, 16, 0}, {4, 12}, 20);
  add_wall({4, 12, 0}, {8, 12}, 20);

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
  lh_slow_debuff->tick_freq = 1.0 / 3;
  lh_slow_debuff->char_mods.push_back(lh_slow_mod);
  lh_slow_debuff->tick_effects.push_back(mb_damage);

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

  int fb_object = nspell_objects;
  SpellObjectDef *fb_object_ = &spell_objects[nspell_objects++];
  fb_object_->name = "Frostbolt";
  fb_object_->speed = 30;
  fb_object_->effects.push_back(fb_debuff_appl);
  fb_object_->effects.push_back(fb_damage);

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

  server.spell_objects = &spell_objects;
  server.nspell_objects = &nspell_objects;
  server.out = &client.in;

  server.in.push(char_spawn_request_event("Eirich", 0));
  server.in.push(char_spawn_request_event("Veuxia", 0));
  server.in.push(char_spawn_request_event("Selion", 1));
  server.in.push(char_spawn_request_event("Veuxe", 1));

  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();
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

  server.walls.push_back({p1, p2, h});
}

void Warg_State::add_char(int team, const char *name)
{
  vec3 pos = spawnpos[team];
  vec3 dir = spawndir[team];

  Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "test_emissive.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "world_origin_distance.frag";

  Character c;
  c.team = team;
  c.name = std::string(name);
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

  client.chars.push_back(c);
  client.nchars++;
  server.chars.push_back(c);

  if (pc < 0)
    pc = 0;
}

void Warg_State::handle_input(
    State **current_state, std::vector<State *> available_states)
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
        else if (pc >= client.nchars - 1)
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
        for (int i = client.nchars - 1; i >= 0; i--)
        {
          if (client.chars[i].alive &&
              client.chars[i].team != client.chars[pc].team)
          {
            if (i < client.chars[pc].target)
            {
              first_lower = i;
            }
            else if (i > client.chars[pc].target)
            {
              first_higher = i;
            }
          }
        }

        if (first_higher != -1)
        {
          client.chars[pc].target = first_higher;
        }
        else if (first_lower != -1)
        {
          client.chars[pc].target = first_lower;
        }
        set_message("Target", s(client.chars[pc].name, " targeted ",
                                  client.chars[client.chars[pc].target].name),
            3.0f);
      }
      if (_e.key.keysym.sym == SDLK_1 && !free_cam)
      {
        server.in.push(cast_event(pc, client.chars[pc].target, "Frostbolt"));
      }
      if (_e.key.keysym.sym == SDLK_2 && !free_cam)
      {
        int target = (client.chars[pc].target < 0 ||
                         client.chars[pc].team !=
                             client.chars[client.chars[pc].target].team)
                         ? pc
                         : client.chars[pc].target;
        server.in.push(cast_event(pc, target, "Lesser Heal"));
      }
      if (_e.key.keysym.sym == SDLK_3 && !free_cam)
      {
        server.in.push(cast_event(pc, client.chars[pc].target, "Counterspell"));
      }
      if (_e.key.keysym.sym == SDLK_4 && !free_cam)
      {
        server.in.push(cast_event(pc, pc, "Ice Block"));
      }
      if (_e.key.keysym.sym == SDLK_5 && !free_cam)
      {
        server.in.push(cast_event(pc, -1, "Arcane Explosion"));
      }
      if (_e.key.keysym.sym == SDLK_6 && !free_cam)
      {
        server.in.push(cast_event(pc, -1, "Holy Nova"));
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

  if (pc < 0)
    return;
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

    vec3 old_pos = client.chars[pc].pos;

    if (right_button_down)
    {
      client.chars[pc].dir = normalize(-vec3(cam_rel.x, cam_rel.y, 0));
    }

    if (is_pressed(SDL_SCANCODE_W))
    {
      vec3 v = vec3(client.chars[pc].dir.x, client.chars[pc].dir.y, 0.0f);
      server.in.push(move_event(pc, dt * v));
    }
    if (is_pressed(SDL_SCANCODE_S))
    {
      vec3 v = vec3(client.chars[pc].dir.x, client.chars[pc].dir.y, 0.0f);
      server.in.push(move_event(pc, dt * -v));
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(client.chars[pc].dir.x, client.chars[pc].dir.y, 0, 0);
      server.in.push(move_event(pc, dt * vec3(r * v)));
    }
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(client.chars[pc].dir.x, client.chars[pc].dir.y, 0, 0);
      server.in.push(move_event(pc, dt * vec3(r * v)));
    }

    cam.pos =
        client.chars[pc].pos + vec3(cam_rel.x, cam_rel.y, cam_rel.z) * cam.zoom;
    cam.dir = -vec3(cam_rel);
  }

  previous_mouse_state = mouse_state;
}

void Warg_State::process_client_events()
{
  while (!client.in.empty())
  {
    Warg_Event ev = client.in.front();
    switch (ev.type)
    {
      case Warg_Event_Type::CharSpawn:
      {
        CharSpawn_Event *spawn = (CharSpawn_Event *)ev.event;
        char *name = spawn->name;
        int team = spawn->team;

        add_char(team, name);
        break;
      }
      case Warg_Event_Type::CharPos:
      {
        CharPos_Event *pos = (CharPos_Event *)ev.event;
        int chr = pos->character;
        vec3 p = pos->pos;

        client.chars[chr].pos = p;
        break;
      }
      case Warg_Event_Type::CastError:
      {
        CastError_Event *errev = (CastError_Event *)ev.event;

        std::string msg;
        msg += client.chars[errev->caster].name;
        msg += " failed to cast ";
        msg += errev->spell;
        msg += ": ";

        switch (errev->err)
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
        break;
      }
      case Warg_Event_Type::CastBegin:
      {
        CastBegin_Event *begin = (CastBegin_Event *)ev.event;
        std::string msg;
        msg += client.chars[begin->caster].name;
        msg += " begins casting ";
        msg += begin->spell;
        set_message("CastBegin:", msg, 10);
        break;
      }
      case Warg_Event_Type::CastInterrupt:
      {
        CastInterrupt_Event *interrupt = (CastInterrupt_Event *)ev.event;
        std::string msg;
        msg += client.chars[interrupt->caster].name;
        msg += "'s casting was interrupted";
        set_message("CastInterrupt:", msg, 10);
        break;
      }
      case Warg_Event_Type::CharHP:
      {
        CharHP_Event *chhp = (CharHP_Event *)ev.event;
        client.chars[chhp->character].alive = chhp->hp > 0;

        std::string msg;
        msg += client.chars[chhp->character].name;
        msg += "'s HP is ";
        msg += std::to_string(chhp->hp);
        set_message("", msg, 10);
        break;
      }
      case Warg_Event_Type::BuffAppl:
      {
        BuffAppl_Event *appl = (BuffAppl_Event *)ev.event;
        std::string msg;
        msg += appl->buff;
        msg += " applied to ";
        msg += client.chars[appl->character].name;
        set_message("ApplyBuff:", msg, 10);
        break;
      }
      case Warg_Event_Type::ObjectLaunch:
      {
        ObjectLaunch_Event *launch = (ObjectLaunch_Event *)ev.event;

        Material_Descriptor material;
        material.albedo = "crate_diffuse.png";
        material.emissive = "test_emissive.png";
        material.normal = "test_normal.png";
        material.roughness = "crate_roughness.png";
        material.vertex_shader = "vertex_shader.vert";
        material.frag_shader = "world_origin_distance.frag";

        SpellObjectInst obji;
        obji.def = spell_objects[launch->object];
        obji.caster = launch->caster;
        obji.target = launch->target;
        obji.pos = launch->pos;
        obji.mesh =
            scene.add_primitive_mesh(cube, "spell_object_cube", material);
        obji.mesh->scale = vec3(0.4f);
        spell_objs.push_back(obji);

        std::string msg;
        msg += (0 <= launch->caster && launch->caster < client.chars.size())
                   ? client.chars[launch->caster].name
                   : "unknown character";
        msg += " launched ";
        msg += spell_objects[launch->object].name;
        msg += " at ";
        msg += (0 <= launch->target && launch->target < client.chars.size())
                   ? client.chars[launch->target].name
                   : "unknown character";
        set_message("ObjectLaunch:", msg, 10);
        break;
      }
      default:
        ASSERT(false);
        break;
    }
    free_warg_event(ev);
    client.in.pop();
  }
}

void Warg_State::update()
{
  server.process_events();
  process_client_events();

  for (int i = 0; i < client.nchars; i++)
  {
    Character *c = &client.chars[i];

    if (!c->alive)
      c->pos = {-1000, -1000, 0};

    c->mesh->position = c->pos;
    c->mesh->scale = vec3(1.0f);
    c->mesh->orientation =
        angleAxis((float32)atan2(c->dir.y, c->dir.x), vec3(0.f, 0.f, 1.f));
  }

  for (auto i = spell_objs.begin(); i != spell_objs.end();)
  {
    auto &o = *i;
    float32 d = length(client.chars[o.target].pos - o.pos);
    if (d < 0.5)
    {
      set_message("object hit",
          client.chars[o.caster].name + "'s " + o.def.name + " hit " +
              client.chars[o.target].name,
          3.0f);

      i = spell_objs.erase(i);
    }
    else
    {
      vec3 a = normalize(client.chars[o.target].pos - o.pos);
      a.x *= o.def.speed * dt;
      a.y *= o.def.speed * dt;
      a.z *= o.def.speed * dt;
      o.pos += a;

      o.mesh->position = o.pos;

      ++i;
    }
  }
}
