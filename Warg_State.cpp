#include "Warg_State.h"
#include "Globals.h"
#include "Render.h"
#include "State.h"
#include <atomic>
#include <memory>
#include <sstream>
#include <thread>

using namespace glm;

Warg_State::Warg_State(std::string name, SDL_Window *window, ivec2 window_size,
    const char *address_, const char *char_name, uint8_t team)
    : State(name, window, window_size)
{
  local = false;

  ASSERT(!enet_initialize());
  clientp = enet_host_create(NULL, 1, 2, 0, 0);
  ASSERT(clientp);

  ENetAddress address;
  ENetEvent event;

  enet_address_set_host(&address, address_);
  address.port = 1337;

  serverp = enet_host_connect(clientp, &address, 2, 0);
  ASSERT(serverp);

  ASSERT(enet_host_service(clientp, &event, 5000) > 0 &&
         event.type == ENET_EVENT_TYPE_CONNECT);

  Buffer b;
  auto ev = char_spawn_request_event("Eirich", 0);
  serialize(b, ev);
  ENetPacket *packet =
      enet_packet_create(&b.data[0], b.data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(serverp, 0, packet);
  enet_host_flush(clientp);

  map = make_blades_edge();
  sdb = make_spell_db();

  map.node = scene.add_aiscene("blades_edge.obj", nullptr, &map.material);

  clear_color = vec3(94. / 255., 155. / 255., 1.);
  scene.lights.light_count = 1;
  Light *light = &scene.lights.lights[0];
  light->position = vec3{25, 25, 200.};
  light->color = 3000.0f * vec3(1.0f, 0.93f, 0.92f);
  light->attenuation = vec3(1.0f, .045f, .0075f);
  light->direction = vec3(25.0f, 25.0f, 0.0f);
  light->ambient = 0.001f;
  light->cone_angle = 0.15f;
  light->type = Light_Type::spot;
  light->casts_shadows = false;

  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();
}

Warg_State::Warg_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{
  local = true;

  server = std::make_unique<Warg_Server>(true);
  server->connect(&out, &in);
  out.push(char_spawn_request_event("Cubeboi", 0));

  map = make_blades_edge();
  sdb = make_spell_db();

  map.node = scene.add_aiscene("blades_edge.obj", nullptr, &map.material);

  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();
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
      if (_e.key.keysym.sym == SDLK_SPACE && !free_cam)
      {
        out.push(jump_event(pc));
      }
    }
    else if (_e.type == SDL_KEYUP)
    {
      if (local)
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
          else if (pc >= chars.size() - 1)
          {
            free_cam = true;
          }
          else
          {
            pc += 1;
          }
        }
      }
      // if (_e.key.keysym.sym == SDLK_TAB && !free_cam)
      // {
      //   int first_lower = -1, first_higher = -1;
      //   for (int i = client->chars.size() - 1; i >= 0; i--)
      //   {
      //     if (client->chars[i].alive &&
      //         client->chars[i].team != client->chars[client->pc].team)
      //     {
      //       if (i < client->chars[client->pc].target)
      //       {
      //         first_lower = i;
      //       }
      //       else if (i > client->chars[client->pc].target)
      //       {
      //         first_higher = i;
      //       }
      //     }
      //   }

      //   if (first_higher != -1)
      //   {
      //     client->chars[client->pc].target = first_higher;
      //   }
      //   else if (first_lower != -1)
      //   {
      //     client->chars[client->pc].target = first_lower;
      //   }
      //   set_message("Target",
      //       s(client->chars[client->pc].name, " targeted ",
      //           client->chars[client->chars[client->pc].target].name),
      //       3.0f);
      // }
      // if (_e.key.keysym.sym == SDLK_1 && !free_cam)
      // {
      //   out.push(cast_event(
      //       client->pc, client->chars[client->pc].target, "Frostbolt"));
      // }
      // if (_e.key.keysym.sym == SDLK_2 && !free_cam)
      // {
      //   int target = (client->chars[pc].target < 0 ||
      //                    client->chars[pc].team !=
      //                        client->chars[client->chars[pc].target].team)
      //                    ? pc
      //                    : client->chars[pc].target;
      //   out.push(cast_event(pc, target, "Lesser Heal"));
      // }
      // if (_e.key.keysym.sym == SDLK_3 && !free_cam)
      // {
      //   out.push(cast_event(pc, client->chars[pc].target,
      //   "Counterspell"));
      // }
      // if (_e.key.keysym.sym == SDLK_4 && !free_cam)
      // {
      //   out.push(cast_event(pc, pc, "Ice Block"));
      // }
      // if (_e.key.keysym.sym == SDLK_5 && !free_cam)
      // {
      //   out.push(cast_event(pc, -1, "Arcane Explosion"));
      // }
      // if (_e.key.keysym.sym == SDLK_6 && !free_cam)
      // {
      //   out.push(cast_event(pc, -1, "Holy Nova"));
      // }
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

  if (!pc)
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

    if (right_button_down)
      out.push(dir_event(pc, normalize(-vec3(cam_rel.x, cam_rel.y, 0))));

    int m = Move_Status::None;
    if (is_pressed(SDL_SCANCODE_W))
      m |= Move_Status::Forwards;
    if (is_pressed(SDL_SCANCODE_S))
      m |= Move_Status::Backwards;
    if (is_pressed(SDL_SCANCODE_A))
      m |= Move_Status::Left;
    if (is_pressed(SDL_SCANCODE_D))
      m |= Move_Status::Right;
    out.push(move_event(pc, (Move_Status)m));

    ASSERT(pc && chars.count(pc));
    vec3 player_pos = chars[pc].pos;
    float effective_zoom = cam.zoom;
    for (auto &surface : map.surfaces)
    {
      vec3 intersection_point;
      bool intersects = ray_intersects_triangle(
          player_pos, cam_rel, surface, &intersection_point);
      if (intersects &&
          length(player_pos - intersection_point) < effective_zoom)
      {
        effective_zoom = length(player_pos - intersection_point);
      }
    }
    cam.pos = player_pos +
              vec3(cam_rel.x, cam_rel.y, cam_rel.z) * (effective_zoom * 0.98f);
    cam.dir = -vec3(cam_rel);
  }

  previous_mouse_state = mouse_state;
}

void Warg_State::process_packets()
{
  ENetEvent event;
  while (enet_host_service(clientp, &event, 0) > 0)
  {
    switch (event.type)
    {
      case ENET_EVENT_TYPE_NONE:
        break;
      case ENET_EVENT_TYPE_CONNECT:
        break;
      case ENET_EVENT_TYPE_RECEIVE:
      {
        Buffer b;
        b.insert((void *)event.packet->data, event.packet->dataLength);
        auto ev = deserialize_event(b);
        in.push(ev);
        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT:
        break;
      default:
        break;
    }
  }
}

void Warg_State::process_events()
{
  while (!in.empty())
  {
    Warg_Event ev = in.front();

    if (get_real_time() - ev.t < SIM_LATENCY / 2000.0f)
      return;

    ASSERT(ev.event);
    switch (ev.type)
    {
      case Warg_Event_Type::CharSpawn:
        process_event((CharSpawn_Event *)ev.event);
        break;
      case Warg_Event_Type::PlayerControl:
        process_event((PlayerControl_Event *)ev.event);
        break;
      case Warg_Event_Type::CharPos:
        process_event((CharPos_Event *)ev.event);
        break;
      case Warg_Event_Type::CastError:
        process_event((CastError_Event *)ev.event);
        break;
      case Warg_Event_Type::CastBegin:
        process_event((CastBegin_Event *)ev.event);
        break;
      case Warg_Event_Type::CastInterrupt:
        process_event((CastInterrupt_Event *)ev.event);
        break;
      case Warg_Event_Type::CharHP:
        process_event((CharHP_Event *)ev.event);
        break;
      case Warg_Event_Type::BuffAppl:
        process_event((BuffAppl_Event *)ev.event);
        break;
      case Warg_Event_Type::ObjectLaunch:
        process_event((ObjectLaunch_Event *)ev.event);
        break;
      default:
        break;
    }
    free_warg_event(ev);
    in.pop();
  }
}

void Warg_State::update()
{
  if (!local)
    process_packets();
  process_events();

  for (auto &c_ : chars)
  {
    auto &c = c_.second;
    if (!c.alive)
      c.pos = {-1000, -1000, 0};
    c.mesh->position = c.pos;
    c.mesh->orientation =
        angleAxis((float32)atan2(c.dir.y, c.dir.x) - half_pi<float32>(),
            vec3(0.f, 0.f, 1.f));
    c.mesh->scale = c.radius * vec3(2);
  }

  for (auto i = spell_objs.begin(); i != spell_objs.end();)
  {
    auto &o = *i;
    float32 d = length(chars[o.target].pos - o.pos);
    if (d < 0.5)
    {
      set_message("object hit",
          chars[o.caster].name + "'s " + o.def.name + " hit " +
              chars[o.target].name,
          3.0f);

      i = spell_objs.erase(i);
    }
    else
    {
      vec3 a = normalize(chars[o.target].pos - o.pos);
      a.x *= o.def.speed * dt;
      a.y *= o.def.speed * dt;
      a.z *= o.def.speed * dt;
      o.pos += a;

      o.mesh->position = o.pos;

      ++i;
    }
  }

  if (local)
  {
    server->update(dt);
  }
  else
  {
    while (!out.empty())
    {
      Warg_Event ev = out.front();

      Buffer b;
      serialize(b, ev);
      ENetPacket *packet = enet_packet_create(
          &b.data[0], b.data.size(), ENET_PACKET_FLAG_RELIABLE);
      enet_peer_send(serverp, 0, packet);
      enet_host_flush(clientp);
      free_warg_event(ev);

      out.pop();
    }
  }

  // meme
  if (pc)
  {
    ASSERT(chars.count(pc));

    clear_color = vec3(94. / 255., 155. / 255., 1.);
    scene.lights.light_count = 2;

    Light *light = &scene.lights.lights[0];

    scene.lights.light_count = 2;
    light->position = vec3{25.01f, 25.0f, 45.f};
    light->color = 1000.0f * vec3(1.0f, 0.93f, 0.92f);
    light->attenuation = vec3(1.0f, .045f, .0075f);
    light->ambient = 0.0005f;
    light->cone_angle = 0.15f;
    light->type = Light_Type::spot;
    light->casts_shadows = true;
    // there was a divide by 0 here, a camera can't point exactly straight down
    light->direction = vec3(0);
    // see Render.h for what these are for:
    light->shadow_blur_iterations = 1;
    light->shadow_blur_radius = 1.25005f;
    light->max_variance = 0.00000001;
    light->shadow_near_plane = 15.f;
    light->shadow_far_plane = 80.f;
    light->shadow_fov = radians(90.f);

    light = &scene.lights.lights[1];

    light->position = vec3{.5, .2, 10.10};
    light->color = 600.0f * vec3(1.f + sin(current_time * 1.35),
                                1.f + cos(current_time * 1.12),
                                1.f + sin(current_time * .9));
    light->attenuation = vec3(1.0f, .045f, .0075f);
    light->direction = chars.count(pc) ? chars[pc].pos : vec3(0);
    light->ambient = 0.0f;
    light->cone_angle = 0.012f;
    light->type = Light_Type::spot;
    light->casts_shadows = true;
    // see Render.h for what these are for:
    light->shadow_blur_iterations = 1;
    light->shadow_blur_radius = 0.55005f;
    light->max_variance = 0.0000003;
    light->shadow_near_plane = 4.51f;
    light->shadow_far_plane = 50.f;
    light->shadow_fov = radians(40.f);
  }
}

void Warg_State::process_event(CharSpawn_Event *spawn)
{
  ASSERT(spawn);
  ASSERT(spawn->id);

  add_char(spawn->id, spawn->team, spawn->name);
}

void Warg_State::process_event(PlayerControl_Event *ev)
{
  ASSERT(ev);
  ASSERT(ev->character);

  pc = ev->character;
}

void Warg_State::process_event(CharPos_Event *pos)
{
  ASSERT(pos);
  ASSERT(pos->character && chars.count(pos->character));
  chars[pos->character].dir = pos->dir;
  chars[pos->character].pos = pos->pos;
}

void Warg_State::process_event(CastError_Event *err)
{
  ASSERT(err);
  ASSERT(err->caster && chars.count(err->caster));

  std::string msg;
  msg += chars[err->caster].name;
  msg += " failed to cast ";
  msg += err->spell;
  msg += ": ";

  switch (err->err)
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
}

void Warg_State::process_event(CastBegin_Event *cast)
{
  ASSERT(cast);
  ASSERT(cast->caster && chars.count(cast->caster));

  std::string msg;
  msg += chars[cast->caster].name;
  msg += " begins casting ";
  msg += cast->spell;
  set_message("CastBegin:", msg, 10);
}

void Warg_State::process_event(CastInterrupt_Event *interrupt)
{
  ASSERT(interrupt);
  ASSERT(interrupt->caster && chars.count(interrupt->caster));

  std::string msg;
  msg += chars[interrupt->caster].name;
  msg += "'s casting was interrupted";
  set_message("CastInterrupt:", msg, 10);
}

void Warg_State::process_event(CharHP_Event *hpev)
{
  ASSERT(hpev);
  ASSERT(hpev->character && chars.count(hpev->character));

  chars[hpev->character].alive = hpev->hp > 0;

  std::string msg;
  msg += chars[hpev->character].name;
  msg += "'s HP is ";
  msg += std::to_string(hpev->hp);
  set_message("", msg, 10);
}

void Warg_State::process_event(BuffAppl_Event *appl)
{
  ASSERT(appl);
  ASSERT(appl->character && chars.count(appl->character));

  std::string msg;
  msg += appl->buff;
  msg += " applied to ";
  msg += chars[appl->character].name;
  set_message("ApplyBuff:", msg, 10);
}

void Warg_State::process_event(ObjectLaunch_Event *launch)
{
  ASSERT(launch);
  ASSERT(launch->caster && chars.count(launch->caster));
  ASSERT(launch->target && chars.count(launch->target));

  Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "test_emissive.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "world_origin_distance.frag";

  SpellObjectInst obji;
  obji.def = sdb->objects[launch->object];
  obji.caster = launch->caster;
  obji.target = launch->target;
  obji.pos = launch->pos;
  obji.mesh = scene.add_primitive_mesh(cube, "spell_object_cube", material);
  obji.mesh->scale = vec3(0.4f);
  spell_objs.push_back(obji);

  std::string msg;
  msg += (0 <= launch->caster && launch->caster < chars.size())
             ? chars[launch->caster].name
             : "unknown character";
  msg += " launched ";
  msg += sdb->objects[launch->object].name;
  msg += " at ";
  msg += (0 <= launch->target && launch->target < chars.size())
             ? chars[launch->target].name
             : "unknown character";
  set_message("ObjectLaunch:", msg, 10);
}

void Warg_State::add_char(UID id, int team, const char *name)
{
  ASSERT(name);

  vec3 pos = map.spawn_pos[team];
  vec3 dir = map.spawn_dir[team];

  Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";

  Character c;
  c.id = id;
  c.team = team;
  c.name = std::string(name);
  c.pos = pos;
  c.dir = dir;
  c.mesh = scene.add_primitive_mesh(cube, "player_cube", material);
  c.hp_max = 100;
  c.hp = c.hp_max;
  c.mana_max = 500;
  c.mana = c.mana_max;

  CharStats s;
  s.gcd = 1.5;
  s.speed = 4.0;
  s.cast_speed = 1;
  s.hp_regen = 2;
  s.mana_regen = 10;
  s.damage_mod = 1;
  s.atk_dmg = 5;
  s.atk_speed = 2;

  c.b_stats = s;
  c.e_stats = s;

  for (int i = 0; i < sdb->spells.size(); i++)
  {
    Spell s;
    s.def = &sdb->spells[i];
    s.cd_remaining = 0;
    c.spellbook[s.def->name] = s;
  }

  chars[id] = c;
}
