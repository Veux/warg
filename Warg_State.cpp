#include "Warg_State.h"
#include "Globals.h"
#include "Render.h"
#include "State.h"
#include "Third_party/imgui/imgui.h"
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
  auto msg = Char_Spawn_Request_Message(tick, "Eirich", 0);
  msg.serialize(b);
  ENetPacket *packet =
    enet_packet_create(&b.data[0], b.data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(serverp, 0, packet);
  enet_host_flush(clientp);
}

Warg_State::Warg_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{
  local = true;

  map = make_blades_edge();
  sdb = make_spell_db();

  map.node = scene.add_aiscene("blades_edge.obj", nullptr, &map.material);

  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();

  server = make_unique<Warg_Server>(true);
  server->connect(&out, &in);
  out.push(make_unique<Char_Spawn_Request_Message>(tick, "Cubeboi", 0));
}

void Warg_State::handle_input_events(
    const std::vector<SDL_Event> &events, bool block_kb, bool block_mouse)
{
  auto is_pressed = [block_kb](int key) {
    const static Uint8 *keys = SDL_GetKeyboardState(NULL);
    SDL_PumpEvents();
    return block_kb ? 0 : keys[key];
  };

  set_message("warg state block kb state: ", s(block_kb), 1.0f);
  set_message("warg state block mouse state: ", s(block_mouse), 1.0f);

  for (auto &_e : events)
  {
    if (_e.type == SDL_KEYDOWN)
    {
      ASSERT(!block_kb);
      if (_e.key.keysym.sym == SDLK_ESCAPE)
      {
        running = false;
        return;
      }
      if (_e.key.keysym.sym == SDLK_SPACE && !free_cam)
      {
        out.push(make_unique<Jump_Message>(tick));
      }
    }
    else if (_e.type == SDL_KEYUP)
    {
      ASSERT(!block_kb);
      if (local)
      {
        if (_e.key.keysym.sym == SDLK_F5)
        {
          free_cam = !free_cam;
        }
      }
    }
    else if (_e.type == SDL_MOUSEWHEEL)
    {
      ASSERT(!block_mouse);
      if (_e.wheel.y < 0)
      {
        cam.zoom += ZOOM_STEP;
      }
      else if (_e.wheel.y > 0)
      {
        cam.zoom -= ZOOM_STEP;
      }
    }
  }
  // first person style camera control:
  ivec2 mouse;
  uint32 mouse_state = SDL_GetMouseState(&mouse.x, &mouse.y);

  if (block_mouse)
  {
    mouse_state = 0;
    mouse = last_seen_mouse_position;
  }
  ivec2 mouse_delta = mouse - last_seen_mouse_position;
  last_seen_mouse_position = mouse;

  set_message("mouse position:", s(mouse.x, " ", mouse.y), 1.0f);
  set_message("mouse grab position:",
      s(last_grabbed_mouse_position.x, " ", last_grabbed_mouse_position.y),
      1.0f);

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
  bool last_seen_lmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool last_seen_rmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

  if (!pc || !chars.count(pc))
    return;
  if (free_cam)
  {
    SDL_SetRelativeMouseMode(SDL_bool(true));
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
    // grab mouse, rotate camera, restore mouse
    if ((left_button_down || right_button_down) &&
        (last_seen_lmb || last_seen_rmb))
    {
      cam.theta += mouse_delta.x * MOUSE_X_SENS;
      cam.phi += mouse_delta.y * MOUSE_Y_SENS;

      if (!mouse_grabbed)
      { // first hold
        set_message("mouse grab event", "", 1.0f);
        mouse_grabbed = true;
        last_grabbed_mouse_position = mouse;
        SDL_SetRelativeMouseMode(SDL_bool(true));
      }
      set_message("mouse is grabbed", "", 1.0f);
    }
    else
    { // not holding button
      set_message("mouse is free", "", 1.0f);
      if (mouse_grabbed)
      { // first unhold
        set_message("mouse release event", "", 1.0f);
        mouse_grabbed = false;

        set_message("mouse warp:",
            s("from:", mouse.x, " ", mouse.y,
                " to:", last_grabbed_mouse_position.x, " ",
                last_grabbed_mouse_position.y),
            1.0f);
        SDL_SetRelativeMouseMode(SDL_bool(false));
        SDL_WarpMouseInWindow(nullptr, last_grabbed_mouse_position.x,
            last_grabbed_mouse_position.y);
      }
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

    ASSERT(pc && chars.count(pc));

    vec3 dir = right_button_down ?
      normalize(-vec3(cam_rel.x, cam_rel.y, 0)) :
      chars[pc].dir;

    int m = Move_Status::None;
    if (is_pressed(SDL_SCANCODE_W))
      m |= Move_Status::Forwards;
    if (is_pressed(SDL_SCANCODE_S))
      m |= Move_Status::Backwards;
    if (is_pressed(SDL_SCANCODE_A))
      m |= Move_Status::Left;
    if (is_pressed(SDL_SCANCODE_D))
      m |= Move_Status::Right;
    
    out.push(
      make_unique<Player_Movement_Message>(tick, (Move_Status)m, dir));

    vec3 player_pos = chars[pc].pos;
    float effective_zoom = cam.zoom;
    /* for (auto &surface : )
      {
        vec3 intersection_point;
        bool intersects = ray_intersects_triangle(
            player_pos, cam_rel, surface, &intersection_point);
        if (intersects &&
            length(player_pos - intersection_point) < effective_zoom)
        {
          effective_zoom = length(player_pos - intersection_point);
        }
      } */
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
        in.push(deserialize_message(b));
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
    auto &msg = in.front();
    //if (get_real_time() - msg->t < SIM_LATENCY / 2000.0f)
    //  return;
    if (msg->tick > server_tick)
      server_tick = msg->tick;
    msg->handle(*this);
    in.pop();
  }
  return;
}

void Warg_State::update()
{
  tick += 1;

  if (!local)
    process_packets();
  process_events();

  if (tick % 300 == 0)
  {
    out.push(make_unique<Ping_Message>(tick));
    last_ping_sent = get_real_time();
  }

  bool show_stats_bar = true;
  ImVec2 stats_bar_pos = { 10, 10 };
  ImGui::SetNextWindowPos(stats_bar_pos);
  ImGui::Begin("stats_bar", &show_stats_bar, ImVec2(10, 10), 0.5f,
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Tick: %u | Server tick: %u | Ping: %dms",
              tick,
              server_tick,
              (int)(last_latency * 1000));
  ImGui::End();

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
      auto ev = std::move(out.front());

      Buffer b;
      ev->serialize(b);
      enet_uint32 flags = ev->reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
      ENetPacket *packet = enet_packet_create(
          &b.data[0], b.data.size(), flags);
      enet_peer_send(serverp, 0, packet);
      enet_host_flush(clientp);

      out.pop();
    }
  }
  static bool show_demo_window = false;
  static bool show_another_window = false;

  {
    static float f = 0.0f;
    static int counter = 0;
    ImGui::Text(
        "Hello, world!"); // Display some text (you can use a format string too)
    ImGui::SliderFloat("float", &f, 0.0f,
        1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::ColorEdit3("clear color",
        (float *)&clear_color); // Edit 3 floats representing a color

    ImGui::Checkbox("Demo Window",
        &show_demo_window); // Edit bools storing our windows open/close state
    ImGui::Checkbox("Another Window", &show_another_window);

    if (ImGui::Button("Button")) // Buttons return true when clicked (NB: most
                                 // widgets return true when edited/activated)
      counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text(
        "Application average %.3f ms per state update (%.1f updates/sec)",
        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  }

  // 2. Show another simple window. In most cases you will use an explicit
  // Begin/End pair to name your windows.
  if (show_another_window)
  {
    ImGui::Begin("Another Window", &show_another_window);
    ImGui::Text("Hello from another window!");
    if (ImGui::Button("Close Me"))
      show_another_window = false;
    ImGui::End();
  }

  // 3. Show the ImGui demo window. Most of the sample code is in
  // ImGui::ShowDemoWindow(). Read its code to learn more about Dear ImGui!
  if (show_demo_window)
  {
    ImGui::SetNextWindowPos(ImVec2(650, 20),
        ImGuiCond_FirstUseEver); // Normally user code doesn't need/want to call
                                 // this because positions are saved in .ini
                                 // file anyway. Here we just want to make the
                                 // demo initial state a bit more friendly!
    ImGui::ShowDemoWindow(&show_demo_window);
  }

  // meme
  if (pc && chars.count(pc))
  {
    ASSERT(chars.count(pc));

    clear_color = vec3(94. / 255., 155. / 255., 1.);
    scene.lights.light_count = 2;

    Light *light = &scene.lights.lights[0];

    scene.lights.light_count = 2;
    light->position = vec3{25.01f, 25.0f, 45.f};
    light->color = 700.0f * vec3(1.0f, 0.93f, 0.92f);
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
    light->color = 100.0f * vec3(1.f + sin(current_time * 1.35),
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

void Connection_Message::handle(Warg_State &state)
{
  state.tick = tick;
  state.server_tick = tick;
}

void Char_Spawn_Message::handle(Warg_State &state)
{
  state.add_char(id, team, name.c_str());
}

void Player_Control_Message::handle(Warg_State &state)
{
  state.pc = character;
}

void Player_Geometry_Message::handle(Warg_State &state)
{
  for (size_t i = 0; i < character_ids.size(); i++)
  {
    if (state.chars.count(character_ids[i]))
    {
      auto &character = state.chars[character_ids[i]];
      character.pos = character_pos[i];
      character.dir = character_dir[i];
      character.vel = character_vel[i];
    }
  }
}

void Cast_Error_Message::handle(Warg_State &state)
{
  ASSERT(state.chars.count(caster));

  std::string msg;
  msg += state.chars[caster].name;
  msg += " failed to cast ";
  msg += spell;
  msg += ": ";

  switch (err)
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

void Cast_Begin_Message::handle(Warg_State &state)
{
  ASSERT(state.chars.count(caster));

  std::string msg;
  msg += state.chars[caster].name;
  msg += " begins casting ";
  msg += spell;
  set_message("CastBegin:", msg, 10);
}

void Cast_Interrupt_Message::handle(Warg_State &state)
{
  ASSERT(state.chars.count(caster));

  std::string msg;
  msg += state.chars[caster].name;
  msg += "'s casting was interrupted";
  set_message("CastInterrupt:", msg, 10);
}

void Char_HP_Message::handle(Warg_State &state)
{
  ASSERT(state.chars.count(character));
  auto &character_ = state.chars[character];

  character_.alive = hp > 0;

  std::string msg;
  msg += character_.name;
  msg += "'s HP is ";
  msg += std::to_string(hp);
  set_message("", msg, 10);
}

void Buff_Application_Message::handle(Warg_State &state)
{
  ASSERT(state.chars.count(character));

  std::string msg;
  msg += buff;
  msg += " applied to ";
  msg += state.chars[character].name;
  set_message("ApplyBuff:", msg, 10);
}

void Object_Launch_Message::handle(Warg_State &state)
{
  ASSERT(state.chars.count(caster));
  auto &caster_ = state.chars[caster];
  ASSERT(state.chars.count(target));
  auto &target_ = state.chars[target];

  //Material_Descriptor material;
  //material.albedo = "crate_diffuse.png";
  //material.emissive = "test_emissive.png";
  //material.normal = "test_normal.png";
  //material.roughness = "crate_roughness.png";
  //material.vertex_shader = "vertex_shader.vert";
  //material.frag_shader = "world_origin_distance.frag";

  //SpellObjectInst obji;
  //obji.def = state.sdb->objects[object];
  //obji.caster = caster;
  //obji.target = target;
  //obji.pos = pos;
  //obji.mesh = state.scene.add_primitive_mesh(cube, "spell_object_cube", material);
  //obji.mesh->scale = vec3(0.4f);
  //spell_objs.push_back(obji);

  std::string msg;
  msg += caster_.name;
  msg += " launched ";
  msg += state.sdb->objects[object].name;
  msg += " at ";
  msg += target_.name;
  set_message("ObjectLaunch:", msg, 10);
}

void Ping_Message::handle(Warg_State &state)
{
  state.out.push(make_unique<Ack_Message>(tick));
}

void Ack_Message::handle(Warg_State &state)
{
  state.last_latency = get_real_time() - state.last_ping_sent;
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
