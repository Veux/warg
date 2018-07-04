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

  if (!pc || !game_state.characters.count(pc))
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

    ASSERT(pc && game_state.characters.count(pc));
    Character *player_character = &game_state.characters[pc];
    ASSERT(player_character);

    vec3 dir = right_button_down ? normalize(-vec3(cam_rel.x, cam_rel.y, 0)) : player_character->physics.dir;

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
  push(make_unique<Input_Message>(input_number, m, dir));
  Input cmd;
  cmd.number = input_number;
  cmd.m = m;
  cmd.dir = dir;
  input_buffer.push(cmd);
  input_number++;
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

void Warg_State::set_camera_geometry()
{
  if (!game_state.characters.count(pc))
    return;
  Character *player_character = &game_state.characters[pc];
  ASSERT(player_character);

  float effective_zoom = cam.zoom;
  for (Triangle &surface : collider_cache)
  {
    vec3 intersection_point;
    bool intersects = ray_intersects_triangle(player_character->physics.pos, cam_rel, surface, &intersection_point);
    if (intersects && length(player_character->physics.pos - intersection_point) < effective_zoom)
    {
      effective_zoom = length(player_character->physics.pos - intersection_point);
    }
  }
  cam.pos = player_character->physics.pos + vec3(cam_rel.x, cam_rel.y, cam_rel.z) * (effective_zoom * 0.98f);
  cam.dir = -vec3(cam_rel);
}

void Warg_State::draw_characters()
{
  for (auto &c : game_state.characters)
  {
    Character *character = &c.second;
    Character_Physics *physics = &character->physics;
    if (!character->alive)
      physics->pos = { -1000, -1000, 0 };

    Node_Ptr character_node = character_nodes[character->id];

    character_node->position = character->physics.pos;
    character_node->orientation =
      angleAxis((float32)atan2(physics->dir.y, physics->dir.x) - half_pi<float32>(), vec3(0.f, 0.f, 1.f));
    character_node->scale = character->radius * vec3(2);
  }
}

void Warg_State::draw_prediction_ghost()
{
  if (!game_state.characters.count(pc))
    return;
  Character *player_character = &server_state.characters[pc];
  ASSERT(player_character);

  Character_Physics *physics = &player_character->physics;

  static Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uses_transparency = true;
  material.albedo.mod = vec4(1, 1, 1, 0.5);

  static Node_Ptr ghost_mesh = scene.add_primitive_mesh(cube, "player_cube", material);
  ghost_mesh->position = physics->pos;
  ghost_mesh->scale = player_character->radius * vec3(2);
  ghost_mesh->velocity = physics->vel;
  ghost_mesh->orientation =
     angleAxis((float32)atan2(physics->dir.y, physics->dir.x) - half_pi<float32>(), vec3(0.f, 0.f, 1.f));
}

void Warg_State::draw_stats_bar()
{
  bool show_stats_bar = true;
  ImVec2 stats_bar_pos = { 10, 10 };
  ImGui::SetNextWindowPos(stats_bar_pos);
  ImGui::Begin("stats_bar", &show_stats_bar, ImVec2(10, 10), 0.5f,
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Ping: %dms", latency_tracker.get_latency());
  ImGui::End();
}

void Warg_State::predict_player_character()
{
  //if (!game_state.characters.count(pc))
  //  return;
  //Character *character = &game_state.characters[pc];
  //ASSERT(character);

  //if (character->physbuf.size() == 0)
  //  return;
  //if (input_buffer.size() == 0)
  //  return;

  //Character_Physics *lastphys = &character->physbuf.back();
  //int its = 0;
  //for (size_t i = 0; i < input_buffer.size(); i++)
  //{
  //  Input *cmd = &input_buffer[i];
  //  if (cmd->number == lastphys->cmdn + 1)
  //  {
  //    Character_Physics newphys = move_char(*lastphys, *cmd, character->radius, character->e_stats.speed, collider_cache);
  //    newphys.cmdn = cmd->number;
  //    newphys.command = *cmd;
  //    character->physbuf.push_back(newphys);
  //    lastphys = &character->physbuf.back();
  //    its++;
  //  }
  //}

  //set_message("collision iterations", s(its), 5);
  //character->physics = *lastphys;
  //set_message("buf size", s("phys: ", character->physbuf.size(), ", move: ", input_buffer.size()), 5);
}

void Warg_State::send_ping()
{
  if (latency_tracker.should_send_ping())
    push(make_unique<Ping_Message>());
}

void Warg_State::process_messages()
{
  if (!local)
    process_packets();
  process_events();
}

void Warg_State::send_messages()
{
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
}

bool prediction_correct(UID player_character_id, Game_State &server_state, Game_State &predicted_state)
{
  bool same_input = server_state.input_number == predicted_state.input_number;

  if (server_state.characters.count(player_character_id) == 0 &&
      predicted_state.characters.count(player_character_id) == 0)
    return same_input;

  if (server_state.characters.count(player_character_id) != predicted_state.characters.count(player_character_id))
    return false;

  ASSERT(server_state.characters.count(player_character_id));
  ASSERT(predicted_state.characters.count(player_character_id));

  Character_Physics *server_physics = &server_state.characters[player_character_id].physics;
  Character_Physics *predicted_physics = &predicted_state.characters[player_character_id].physics;
  bool same_player_physics = *server_physics == *predicted_physics;

  return same_input && same_player_physics;
}

void Warg_State::predict_state()
{
  game_state = server_state;

  if (server_state.characters.count(pc) == 0)
    return;

  if (input_buffer.size() == 0)
    return;

  while (state_buffer.size() > 0 && state_buffer.front().input_number < server_state.input_number)
    state_buffer.pop_front();

  Game_State predicted_state = server_state;
  size_t prediction_start = 0;
  if (state_buffer.size() && prediction_correct(pc, server_state, state_buffer.front()))
  {
    predicted_state = state_buffer.back();
    state_buffer.pop_front();
    prediction_start = input_buffer.size() - 1;
  }
  else
  {
    state_buffer = {};
    predicted_state = server_state;
  }

  size_t prediction_iterations = 0;
  for (size_t i = prediction_start; i < input_buffer.size(); i++)
  {
    Input &input = input_buffer[i];

    Character &player_character = predicted_state.characters[pc];
    Character_Physics &physics = player_character.physics;
    vec3 radius = player_character.radius;
    float32 movement_speed = player_character.e_stats.speed;

    move_char(player_character, input, collider_cache);
    if (vec3_has_nan(physics.pos))
      physics.pos = map.spawn_pos[player_character.team];

    predicted_state.input_number = input.number;
    state_buffer.push_back(predicted_state);

    prediction_iterations++;
  }
  set_message("prediction iterations:", s(prediction_iterations), 5);

  game_state = state_buffer.back();
}

void Warg_State::update_meshes()
{
  for (auto &c : game_state.characters)
  {
    Character *character = &c.second;
    if (!character_nodes.count(character->id))
    {
      add_character_mesh(character->id);
    }
  }
}

void Warg_State::update()
{
  process_messages();
  send_ping();
  draw_stats_bar();
  predict_state();
  set_camera_geometry();
  update_meshes();
  draw_characters();
  draw_prediction_ghost();
  send_messages();
}

void Warg_State::add_character_mesh(UID character_id)
{
  Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.backface_culling = false;
  character_nodes[character_id] = scene.add_primitive_mesh(cube, "player_cube", material);
}

void State_Message::handle(Warg_State &state)
{
  state.pc = pc;
  state.server_state.characters = characters;
  state.server_state.tick = tick;
  state.server_state.input_number = input_number;

  state.input_buffer.pop_older_than(input_number);

  //if (state.pc == 0)
  //  return;

  //ASSERT(state.game_state.characters.count(state.pc));
  //Character *player_character = &state.game_state.characters[state.pc];

  //while (player_character->physbuf.size() && player_character->physbuf.front().cmdn < player_character->physics.cmdn)
  //  player_character->physbuf.pop_front();

  //if (player_character->physbuf.size())
  //{
  //  ASSERT(player_character->physbuf.front().cmdn == player_character->physics.cmdn);
  //  if (player_character->physbuf.front() != player_character->physics)
  //  {
  //    auto &front = player_character->physbuf.front();

  //    player_character->physbuf.clear();
  //    player_character->physbuf.push_back(player_character->physics);
  //  }
  //}
  //else
  //{
  //  player_character->physbuf.push_back(player_character->physics);
  //}

  //while (state.movebuf.size() && state.movebuf.front().i < player_character->physics.cmdn)
  //  state.movebuf.pop_front();
}

void Cast_Error_Message::handle(Warg_State &state)
{
  ASSERT(state.game_state.characters.count(caster));

  std::string msg;
  msg += state.game_state.characters[caster].name;
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
  ASSERT(state.game_state.characters.count(caster));

  std::string msg;
  msg += state.game_state.characters[caster].name;
  msg += " begins casting ";
  msg += spell;
  set_message("CastBegin:", msg, 10);
}

void Cast_Interrupt_Message::handle(Warg_State &state)
{
  ASSERT(state.game_state.characters.count(caster));

  std::string msg;
  msg += state.game_state.characters[caster].name;
  msg += "'s casting was interrupted";
  set_message("CastInterrupt:", msg, 10);
}

void Char_HP_Message::handle(Warg_State &state)
{
  ASSERT(state.game_state.characters.count(character));
  auto &character_ = state.game_state.characters[character];

  character_.alive = hp > 0;

  std::string msg;
  msg += character_.name;
  msg += "'s HP is ";
  msg += std::to_string(hp);
  set_message("", msg, 10);
}

void Buff_Application_Message::handle(Warg_State &state)
{
  ASSERT(state.game_state.characters.count(character));

  std::string msg;
  msg += buff;
  msg += " applied to ";
  msg += state.game_state.characters[character].name;
  set_message("ApplyBuff:", msg, 10);
}

void Object_Launch_Message::handle(Warg_State &state)
{
  ASSERT(state.game_state.characters.count(caster));
  auto &caster_ = state.game_state.characters[caster];
  ASSERT(state.game_state.characters.count(target));
  auto &target_ = state.game_state.characters[target];

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
  character_nodes[id] = scene.add_primitive_mesh(cube, "player_cube", material);

  Character c;
  c.id = id;
  c.team = team;
  c.name = std::string(name);
  c.physics.pos = pos;
  c.physics.dir = dir;
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

  for (size_t i = 0; i < sdb->spells.size(); i++)
  {
    Spell s;
    s.def = &sdb->spells[i];
    s.cd_remaining = 0;
    c.spellbook[s.def->name] = s;
  }

  game_state.characters[id] = c;
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

uint32 Latency_Tracker::get_latency() { return (uint32)round(last_latency * 1000); }
