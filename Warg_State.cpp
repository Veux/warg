#include "Warg_State.h"
#include "Globals.h"
#include "Render.h"
#include "State.h"
#include "Third_party/imgui/imgui.h"
#include "UI.h"
#include <atomic>
#include <memory>
#include <sstream>
#include <thread>

using namespace glm;

Warg_State::Warg_State(
    std::string name, SDL_Window *window, ivec2 window_size, const char *address_, const char *char_name, uint8_t team)
    : State(name, window, window_size)
{
  local = false;

  map = make_blades_edge();
  sdb = make_spell_db();

  map.node = scene.add_aiscene("blades_edge.fbx", nullptr, &map.material);
  collider_cache = collect_colliders(scene);
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

  ASSERT(enet_host_service(clientp, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT);

  Buffer b;
  auto msg = Char_Spawn_Request_Message(char_name, 0);
  msg.t = get_real_time();
  msg.serialize(b);
  ENetPacket *packet = enet_packet_create(&b.data[0], b.data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(serverp, 0, packet);
  enet_host_flush(clientp);
}

Warg_State::Warg_State(std::string name, SDL_Window *window, ivec2 window_size) : State(name, window, window_size)
{
  local = true;
  renderer.name = "Warg_State";
  map = make_blades_edge();
  sdb = make_spell_db();

  map.node = scene.add_aiscene("Blades_Edge/blades_edge.fbx", nullptr, &map.material);
  collider_cache = collect_colliders(scene);
  //collider_cache.push_back({ { 1000, 1000, 0 },{ -1000,1000,0 },{ -1000,-1000,0 } });
  //collider_cache.push_back({ { -1000, -1000, 0 },{ 1000, -1000, 0 },{ 1000, 1000, 0 } });

  Material_Descriptor sky_mat;
  sky_mat.backface_culling = false;
  sky_mat.vertex_shader = "vertex_shader.vert";
  sky_mat.frag_shader = "skybox.frag";
  Node_Ptr skybox = scene.add_primitive_mesh(cube, "skybox", sky_mat);
  skybox->scale = vec3(500);
  scene.set_parent(skybox, scene.root, true);

  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();

  server = make_unique<Warg_Server>(true);
  server->connect(&out, &in);
  push(make_unique<Char_Spawn_Request_Message>("Cubeboi", 0));
}

void Warg_State::handle_input_events(const std::vector<SDL_Event> &events, bool block_kb, bool block_mouse)
{
  auto is_pressed = [block_kb](int key) {
    const static Uint8 *keys = SDL_GetKeyboardState(NULL);
    SDL_PumpEvents();
    return block_kb ? 0 : keys[key];
  };

  // set_message("warg state block kb state: ", s(block_kb), 1.0f);
  // set_message("warg state block mouse state: ", s(block_mouse), 1.0f);

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
          SDL_SetRelativeMouseMode(SDL_bool(free_cam));
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

  // set_message("mouse position:", s(mouse.x, " ", mouse.y), 1.0f);
  // set_message("mouse grab position:",
  //    s(last_grabbed_mouse_position.x, " ", last_grabbed_mouse_position.y),
  //    1.0f);

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
  bool last_seen_lmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool last_seen_rmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

  if (!pc || !chars.count(pc))
    return;
  if (free_cam)
  {
    SDL_SetRelativeMouseMode(SDL_bool(true));
    SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);
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
    // grab mouse, rotate camera, restore mouse
    if ((left_button_down || right_button_down) && (last_seen_lmb || last_seen_rmb))
    { // currently holding
      if (!mouse_grabbed)
      { // first hold
        // set_message("mouse grab event", "", 1.0f);
        mouse_grabbed = true;
        last_grabbed_mouse_position = mouse;
        SDL_SetRelativeMouseMode(SDL_bool(true));
        SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);
      }
      // set_message("mouse delta: ", s(mouse_delta.x, " ",
      // mouse_delta.y), 1.0f);
      SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);
      cam.theta += mouse_delta.x * MOUSE_X_SENS;
      cam.phi += mouse_delta.y * MOUSE_Y_SENS;
      // set_message("mouse is grabbed", "", 1.0f);
    }
    else
    { // not holding button
      // set_message("mouse is free", "", 1.0f);
      if (mouse_grabbed)
      { // first unhold
        // set_message("mouse release event", "", 1.0f);
        mouse_grabbed = false;
        // set_message("mouse warp:",
        //    s("from:", mouse.x, " ", mouse.y,
        //        " to:", last_grabbed_mouse_position.x, " ",
        //        last_grabbed_mouse_position.y),
        //    1.0f);
        SDL_SetRelativeMouseMode(SDL_bool(false));
        SDL_WarpMouseInWindow(nullptr, last_grabbed_mouse_position.x, last_grabbed_mouse_position.y);
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
    const float32 lower = 100 * epsilon<float32>() - half_pi<float32>();
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

    vec3 dir = right_button_down ? normalize(-vec3(cam_rel.x, cam_rel.y, 0)) : chars[pc].physics.dir;

    int m = Move_Status::None;
    if (is_pressed(SDL_SCANCODE_W))
      m |= Move_Status::Forwards;
    if (is_pressed(SDL_SCANCODE_S))
      m |= Move_Status::Backwards;
    if (is_pressed(SDL_SCANCODE_A))
      m |= Move_Status::Left;
    if (is_pressed(SDL_SCANCODE_D))
      m |= Move_Status::Right;
    if (is_pressed(SDL_SCANCODE_SPACE))
      m |= Move_Status::Jumping;

    register_move_command((Move_Status)m, dir);
  }
  previous_mouse_state = mouse_state;
}

void Warg_State::register_move_command(Move_Status m, vec3 dir)
{
  push(make_unique<Input_Message>(move_cmd_n, m, dir));
  Movement_Command cmd;
  cmd.i = move_cmd_n;
  cmd.m = m;
  cmd.dir = dir;
  movebuf.push_back(cmd);
  move_cmd_n++;
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
    if (get_real_time() - msg->t < SIM_LATENCY / 2000.0f)
      return;
    msg->handle(*this);
    in.pop();
  }
  return;
}

void Warg_State::push(unique_ptr<Message> msg)
{
  msg->t = current_time;
  out.push(std::move(msg));
}

void Warg_State::update()
{
  if (!local)
    process_packets();
  process_events();

  if (latency_tracker.should_send_ping())
    push(make_unique<Ping_Message>());

  bool show_stats_bar = true;
  ImVec2 stats_bar_pos = {10, 10};
  ImGui::SetNextWindowPos(stats_bar_pos);
  ImGui::Begin("stats_bar", &show_stats_bar, ImVec2(10, 10), 0.5f,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
          ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Ping: %dms", latency_tracker.get_latency());
  ImGui::End();

  if (chars.count(pc) && chars[pc].physbuf.size() && movebuf.size())
  {
    Character *p = &chars[pc];
    ASSERT(p);

    Character_Physics *lastphys = &p->physbuf.back();
    int its = 0;
    for (size_t i = 0; i < movebuf.size(); i++)
    {
      Movement_Command *cmd = &movebuf[i];
      if (cmd->i == lastphys->cmdn + 1)
      {
        Character_Physics newphys = move_char(*lastphys, *cmd, p->radius, p->e_stats.speed, collider_cache);
        newphys.cmdn = cmd->i;
        newphys.command = *cmd;
        p->physbuf.push_back(newphys);
        lastphys = &p->physbuf.back();
        its++;
      }
    }

    set_message("collision iterations", s(its), 5);
    p->physics = *lastphys;
    set_message("buf size", s("phys: ", p->physbuf.size(), ", move: ", movebuf.size()), 5);

    float effective_zoom = cam.zoom;
    for (Triangle &surface : collider_cache)
    {
      vec3 intersection_point;
      bool intersects = ray_intersects_triangle(p->physics.pos, cam_rel, surface, &intersection_point);
      if (intersects && length(p->physics.pos - intersection_point) < effective_zoom)
      {
        effective_zoom = length(p->physics.pos - intersection_point);
      }
    }
    cam.pos = p->physics.pos + vec3(cam_rel.x, cam_rel.y, cam_rel.z) * (effective_zoom * 0.98f);
    cam.dir = -vec3(cam_rel);
  }

  for (auto &c_ : chars)
  {
    auto &c = c_.second;
    if (!c.alive)
      c.physics.pos = {-1000, -1000, 0};
    c.mesh->position = c.physics.pos;
    c.mesh->orientation =
        angleAxis((float32)atan2(c.physics.dir.y, c.physics.dir.x) - half_pi<float32>(), vec3(0.f, 0.f, 1.f));
    c.mesh->scale = c.radius * vec3(2);

    if (c.id == pc)
    {
      static Material_Descriptor material;
      material.albedo = "crate_diffuse.png";
      material.emissive = "";
      material.normal = "test_normal.png";
      material.roughness = "crate_roughness.png";
      material.vertex_shader = "vertex_shader.vert";
      material.frag_shader = "fragment_shader.frag";
      static Node_Ptr serv_mesh = scene.add_primitive_mesh(cube, "player_cube", material);
      if (c.physbuf.size())
      {
        Character_Physics *phys = &c.physbuf.front();
        serv_mesh->position = phys->pos;
        serv_mesh->scale = c.radius * vec3(2);
        serv_mesh->velocity = phys->vel;
        serv_mesh->orientation =
          angleAxis((float32)atan2(phys->dir.y, phys->dir.x) - half_pi<float32>(), vec3(0.f, 0.f, 1.f));
      }
    }
  }

  for (auto i = spell_objs.begin(); i != spell_objs.end();)
  {
    auto &o = *i;
    float32 d = length(chars[o.target].physics.pos - o.pos);
    if (d < 0.5)
    {
      set_message("object hit", chars[o.caster].name + "'s " + o.def.name + " hit " + chars[o.target].name, 3.0f);

      i = spell_objs.erase(i);
    }
    else
    {
      vec3 a = normalize(chars[o.target].physics.pos - o.pos);
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
      ENetPacket *packet = enet_packet_create(&b.data[0], b.data.size(), flags);
      enet_peer_send(serverp, 0, packet);
      enet_host_flush(clientp);

      out.pop();
    }
  }
  static bool show_demo_window = false;
  static bool show_another_window = false;

  // meme
  if (pc && chars.count(pc))
  {
    check_gl_error();
    imgui_light_array(scene.lights);
    check_gl_error();
    Light *light = &scene.lights.lights[1];
    light->direction = chars.count(pc) ? chars[pc].physics.pos : vec3(0);
    light->color =
        50.0f * vec3(1.f + sin(current_time * 1.35), 1.f + cos(current_time * 1.12), 1.f + sin(current_time * .9));
  }
}

void State_Message::handle(Warg_State &state)
{
  state.pc = pc;
  for (size_t i = 0; i < id.size(); i++)
  {
    if (!state.chars.count(id[i]))
    {
      state.add_char(id[i], team[i], name[i].c_str());
    }

    auto &character = state.chars[id[i]];
    character.hp_max = hp_max[i];
    character.radius = radius[i];
    Character_Physics phys;
    phys.pos = pos[i];
    phys.dir = dir[i];
    phys.vel = vel[i];
    phys.grounded = grounded[i];
    phys.cmdn = command_n[i];
    character.physics = phys;

    if (id[i] == state.pc)
    {
      while (character.physbuf.size() && character.physbuf.front().cmdn < phys.cmdn)
        character.physbuf.pop_front();

      if (character.physbuf.size())
      {
        ASSERT(character.physbuf.front().cmdn == phys.cmdn);
        if (character.physbuf.front() != phys)
        {
          auto &front = character.physbuf.front();

          character.physbuf.clear();
          character.physbuf.push_back(phys);
        }
      }
      else
      {
        character.physbuf.push_back(phys);
      }
      while (state.movebuf.size() && state.movebuf.front().i < phys.cmdn)
        state.movebuf.pop_front();
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

  // Material_Descriptor material;
  // material.albedo = "crate_diffuse.png";
  // material.emissive = "test_emissive.png";
  // material.normal = "test_normal.png";
  // material.roughness = "crate_roughness.png";
  // material.vertex_shader = "vertex_shader.vert";
  // material.frag_shader = "world_origin_distance.frag";

  // SpellObjectInst obji;
  // obji.def = state.sdb->objects[object];
  // obji.caster = caster;
  // obji.target = target;
  // obji.pos = pos;
  // obji.mesh = state.scene.add_primitive_mesh(cube, "spell_object_cube", material);
  // obji.mesh->scale = vec3(0.4f);
  // spell_objs.push_back(obji);

  std::string msg;
  msg += caster_.name;
  msg += " launched ";
  msg += state.sdb->objects[object].name;
  msg += " at ";
  msg += target_.name;
  set_message("ObjectLaunch:", msg, 10);
}

void Ping_Message::handle(Warg_State &state) { state.push(make_unique<Ack_Message>()); }

void Ack_Message::handle(Warg_State &state) { state.latency_tracker.ack_received(); }

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
  material.backface_culling = false;

  Character c;
  c.id = id;
  c.team = team;
  c.name = std::string(name);
  c.physics.pos = pos;
  c.physics.dir = dir;
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

bool Latency_Tracker::should_send_ping()
{
  float64 current_time = get_real_time();
  if (!acked || current_time < last_ping + 1)
    return false;
  last_ping = current_time;
  acked = false;
  return true;
}

void Latency_Tracker::ack_received()
{
  last_ack = get_real_time();
  acked = true;
  last_latency = last_ack - last_ping;
}

uint32 Latency_Tracker::get_latency() { return round(last_latency * 1000); }
