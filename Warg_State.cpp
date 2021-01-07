#include "stdafx.h"
#include "Warg_State.h"
#include "Globals.h"
#include "Render.h"
#include "State.h"
#include "Animation_Utilities.h"
#include "UI.h"
#include "Render_Test_State.h"
using namespace glm;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

bool SERVER_SEEN = false;
bool CLIENT_SEEN = false;
bool WANT_SPAWN_OCTREE_BOX = false;
bool WANT_CLEAR_OCTREE = false;

extern void small_object_water_settings(Uniform_Set_Descriptor *dst);
extern void spawn_grabbyarm(Scene_Graph *scene, vec3 position);
extern void update_grabbyarm(Scene_Graph *scene, float64 current_time);

Node_Index add_boi_character_mesh(Scene_Graph &scene);
Node_Index add_girl_character_mesh(Scene_Graph &scene);

Warg_State::Warg_State(
    std::string name, SDL_Window *window, ivec2 window_size, std::string_view character_name, int32 team)
    : State(name, window, window_size), character_name(character_name), team(team)
{
  // session = session_;

  // set_session((Session *)new Local_Session());
}

Warg_State::~Warg_State()
{
  delete map;
}

void Warg_State::initialize_warg_state()
{
  player_character_id = 0;
  target_id = 0;
  character_nodes.clear();
  spell_object_nodes.clear();
  animation_objects.clear();
  scene.clear();
  resource_manager.clear();
  if (map)
    delete map;
  map = new Blades_Edge(scene);
  scene.initialize_lighting("Assets/Textures/Environment_Maps/GrandCanyon_C_YumaPoint/radiance.hdr",
      "Assets/Textures/Environment_Maps/GrandCanyon_C_YumaPoint/irradiance.hdr");

  session->request_spawn(character_name, team);
}

void Warg_State::session_swapper()
{
  ImGui::Begin("Session swapper");

  if (session)
  {
    ImGui::Checkbox("Enable prediction", &session->prediction_enabled);
    if (ImGui::Button("Disconnect"))
    {
      session->disconnect();
      delete session;
      session = nullptr;
      scene.clear();
      resource_manager.clear();
      renderer.render_entities.clear();
      renderer.render_instances.clear();
      want_set_intro_scene = true;
      frostbolt_effect2 = nullptr;
    }
  }
  else
  {

    if (ImGui::Button("Local"))
    {
      if (session)
        delete session;

      session = (Session *)new Local_Session();
      initialize_warg_state();
    }

    ImGui::Text(s("Wargspy IP:", CONFIG.wargspy_address, " -").c_str());
    if (wargspy_state.wargspy)
    {
      ImGui::SameLine();
      ImGui::TextColored(imgui_green, "online");
    }
    else
    {
      ImGui::SameLine();
      ImGui::TextColored(imgui_red, "offline");
    }

    ImGui::Text("Server:");
    ImGui::SameLine();
    if (wargspy_state.game_server_address)
    {
      ImGui::TextColored(imgui_green, "%s", ip_address_string(wargspy_state.game_server_address).c_str());
      ImGui::SameLine();
      if (ImGui::Button("Connect"))
      {
        if (session)
          delete session;
        Network_Session *network_session = new Network_Session();
        network_session->connect(wargspy_state.game_server_address);
        session = (Session *)network_session;
        initialize_warg_state();
      }
    }
    else
    {
      ImGui::TextColored(imgui_red, "offline");
    }
  }

  ImGui::End();
}

void Warg_State::handle_input_events()
{
  if (!recieves_input)
    return;

  auto is_pressed = [this](int key) { return key_state[key]; };

  // set_message("warg state block kb state: ", s(block_kb), 1.0f);
  // set_message("warg state block mouse state: ", s(block_mouse), 1.0f);

  for (auto &_e : events_this_tick)
  {
    if (_e.type == SDL_KEYDOWN)
    {
      if (_e.key.keysym.sym == SDLK_ESCAPE)
      {
        running = false;
        return;
      }
      if (_e.key.keysym.sym == SDLK_SPACE && !free_cam)
      {
      }
      if (SDLK_1 <= _e.key.keysym.sym && _e.key.keysym.sym <= SDLK_9 && !free_cam &&
          any_of(current_game_state.living_characters, [&](auto &lc) { return lc.id == player_character_id; }))
      {
        int i = 0;
        const auto &spell = find_if(current_game_state.character_spells,
            [&](auto &cs) { return cs.character == player_character_id && i++ == _e.key.keysym.sym - SDLK_1; });
        if (spell != current_game_state.character_spells.end())
          session->try_cast_spell(target_id, spell->spell);
      }
      if (_e.key.keysym.sym == SDLK_TAB && !free_cam &&
          any_of(current_game_state.living_characters, [&](auto &lc) { return lc.id == player_character_id; }))
      {
        for (auto &lc : current_game_state.living_characters)
          if (lc.id != player_character_id)
            target_id = lc.id;
      }
      if (_e.key.keysym.sym == SDLK_g)
      {
        WANT_SPAWN_OCTREE_BOX = true;
      }
      if (_e.key.keysym.sym == SDLK_h)
      {
        WANT_CLEAR_OCTREE = true;
      }
      if (_e.key.keysym.sym == SDLK_RETURN)
      {
        interface_state.focus_chat = true;
      }
    }
    else if (_e.type == SDL_KEYUP)
    {
      if (_e.key.keysym.sym == SDLK_F5)
      {
        free_cam = !free_cam;
        mouse_relative_mode = free_cam;
      }
    }
    else if (_e.type == SDL_MOUSEWHEEL)
    {
      if (_e.wheel.y < 0)
      {
        camera.zoom += ZOOM_STEP;
      }
      else if (_e.wheel.y > 0)
      {
        camera.zoom -= ZOOM_STEP;
      }
    }
  }
  // first person style camera control:

  set_message("cursor_position:", s(cursor_position.x, " ", cursor_position.y), 1.0f);
  set_message("mouse grab position:", s(last_grabbed_cursor_position.x, " ", last_grabbed_cursor_position.y), 1.0f);

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
  bool last_seen_lmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool last_seen_rmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

  if (!player_character_id ||
      none_of(current_game_state.characters, [&](auto &c) { return c.id == player_character_id; }))
    return;
  if (player_character_id &&
      none_of(current_game_state.living_characters, [&](auto &lc) { return lc.id == player_character_id; }))
  {
    session->request_spawn(character_name, team);
  }
  if (free_cam)
  {
    mouse_relative_mode = true;
    draw_cursor = false;

    if (!IMGUI.context->IO.WantCaptureMouse)
      IMGUI.ignore_all_input = true;

    camera.theta += mouse_delta.x * MOUSE_X_SENS;
    camera.phi += mouse_delta.y * MOUSE_Y_SENS;
    // wrap x
    if (camera.theta > two_pi<float32>())
    {
      camera.theta = camera.theta - two_pi<float32>();
    }
    if (camera.theta < 0)
    {
      camera.theta = two_pi<float32>() + camera.theta;
    }
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (camera.phi > upper)
    {
      camera.phi = upper;
    }
    const float32 lower = -half_pi<float32>() + 100 * epsilon<float32>();
    if (camera.phi < lower)
    {
      camera.phi = lower;
    }

    mat4 rx = rotate(-camera.theta, vec3(0, 0, 1));
    vec4 vr = rx * vec4(0, 1, 0, 0);
    vec3 perp = vec3(-vr.y, vr.x, 0);
    mat4 ry = rotate(camera.phi, perp);
    camera.dir = normalize(vec3(ry * vr));

    if (is_pressed(SDL_SCANCODE_W))
    {
      camera.pos += MOVE_SPEED * camera.dir;
    }
    if (is_pressed(SDL_SCANCODE_S))
    {
      camera.pos -= MOVE_SPEED * camera.dir;
    }
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.x, vr.y, 0, 0);
      camera.pos += vec3(MOVE_SPEED * r * v);
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.x, vr.y, 0, 0);
      camera.pos += vec3(MOVE_SPEED * r * v);
    }
  }
  else
  {
    // wow style camera
    // grab mouse, rotate camera, restore mouse
    if ((left_button_down || right_button_down) && (last_seen_lmb || last_seen_rmb))
    { // currently holding
      if (!mouse_grabbed)
      { // first hold
        // set_message("mouse grab event", "", 1.0f);
        mouse_grabbed = true;
        last_grabbed_cursor_position = cursor_position;
        mouse_relative_mode = true;
      }

      if (!IMGUI.context->IO.WantCaptureMouse)
      {
        IMGUI.ignore_all_input = true;
      }
      draw_cursor = false;
      // set_message("mouse delta: ", s(mouse_delta.x, " ", mouse_delta.y), 1.0f);
      camera.theta += mouse_delta.x * MOUSE_X_SENS;
      camera.phi += mouse_delta.y * MOUSE_Y_SENS;
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
        // s("from:", cursor_position.x, " ", cursor_position.y, " to:", last_grabbed_cursor_position.x, " ",
        //    last_grabbed_cursor_position.y),
        //  1.0f);
        mouse_relative_mode = false;
        request_cursor_warp_to = last_grabbed_cursor_position;
      }
      draw_cursor = true;
      IMGUI.ignore_all_input = false;
    }
    // wrap x
    if (camera.theta > two_pi<float32>())
    {
      camera.theta = camera.theta - two_pi<float32>();
    }
    if (camera.theta < 0)
    {
      camera.theta = two_pi<float32>() + camera.theta;
    }
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (camera.phi > upper)
    {
      camera.phi = upper;
    }
    const float32 lower = 100 * epsilon<float32>() - half_pi<float32>();
    if (camera.phi < lower)
    {
      camera.phi = lower;
    }

    //+x right, +y forward, +z up

    // construct a matrix that represents a rotation around the z axis by
    // theta(x) radians
    mat4 rx = rotate(-camera.theta, vec3(0, 0, 1));

    //'default' position of camera is behind the Character

    // rotate that vector by our rx matrix
    character_to_camera = rx * normalize(vec4(0, -1, 0.0, 0));

    // perp is the camera-relative 'x' axis that moves with the camera
    // as the camera rotates around the Character-centered y axis
    // should always point towards the right of the screen
    vec3 perp = vec3(-character_to_camera.y, character_to_camera.x, 0);

    // construct a matrix that represents a rotation around perp by -phi
    mat4 ry = rotate(-camera.phi, perp);

    // rotate the camera vector around perp
    character_to_camera = normalize(ry * character_to_camera);

    ASSERT(player_character_id);
    // auto player_character =
    //    find_if(current_game_state.characters, [&](auto &c) { return c.id == player_character_id; });
    auto player_character = find_by(current_game_state.characters, &Character::id, player_character_id);
    ASSERT(player_character != current_game_state.characters.end());

    quat orientation;
    static quat last_rmb_orientation = player_character->physics.orientation;
    if (right_button_down)
    {
      orientation =
          angleAxis(half_pi<float32>() + atan2(character_to_camera.y, character_to_camera.x), vec3(0.f, 0.f, 1.f));
      last_rmb_orientation = orientation;
    }
    else
      orientation = last_rmb_orientation;

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

    if (session)
      session->move_command(m, orientation, target_id);
  }
  previous_mouse_state = mouse_state;
}

void Warg_State::set_camera_geometry()
{
  // set_message(s("set_camera_geometry()",s(" time:", current_time), 1.0f);
  auto player_character = find_if(current_game_state.characters, [&](auto &c) { return c.id == player_character_id; });
  if (player_character == current_game_state.characters.end())
    return;

  float effective_zoom = camera.zoom;

  // todo: fix camera collision
  // for (Triangle &surface : collider_cache)
  //{
  //  break;
  //  vec3 intersection_point;
  //  bool intersects =
  //      ray_intersects_triangle(player_character->physics.position, character_to_camera, surface,
  //      &intersection_point);
  //  if (intersects && length(player_character->physics.position - intersection_point) < effective_zoom)
  //  {
  //    effective_zoom = length(player_character->physics.position - intersection_point);
  //  }
  //}
  camera.pos = player_character->physics.position +
               vec3(character_to_camera.x, character_to_camera.y, character_to_camera.z) * (effective_zoom * 0.98f);
  camera.dir = -vec3(character_to_camera);
}

void Warg_State::update_hp_bar(UID character_id)
{
  // set_message("update_hp_bar()");
  Node_Index character_node = character_nodes[character_id];
  Node_Index hp_bar = scene.find_by_name(character_node, "hp_bar");

  const auto &lc = std::find_if(current_game_state.living_characters.begin(),
      current_game_state.living_characters.end(), [&](auto &lc) { return lc.id == character_id; });
  if (lc == current_game_state.living_characters.end())
  {
    scene.delete_node(hp_bar);

    // if (found_hp_bars)
    //{
    //  const mat4 Tp = translate(character_node->position);
    //  const mat4 Sp = scale(character_node->scale);
    //  const mat4 Rp = toMat4(character_node->orientation);
    //  ASSERT(character_node->basis == mat4(1));

    //  const mat4 final_transformation_p = Tp * Rp * Sp;

    //  while (owned_children.size())
    //  {
    //    Node_Ptr child = owned_children.front();
    //    ASSERT(child);
    //    const mat4 Tc = translate(child->position);
    //    const mat4 Sc = scale(child->scale);
    //    const mat4 Rc = toMat4(child->orientation);
    //    const mat4 final_transformation_c = final_transformation_p * Tc * Rc * Sc;
    //    child->basis = final_transformation_c;
    //    scene.set_parent(child, scene.root);

    //    Animation_Object animation_object;
    //    animation_object.node = child;
    //    animation_object.physics.position = character->physics.position;
    //    animation_object.physics.velocity = random_within(vec3(2.f, 2.f, 5.f)) - vec3(1.f, 1.f, 0);
    //    animation_object.physics.orientation = child->orientation;
    //    animation_object.physics.grounded = false;
    //    animation_object.radius = child->basis * vec4(child->scale, 1.f);

    //    Material_Descriptor material;
    //    material.albedo.mod = vec4(100.f, 0.5f, 0.5f, 1.f);
    //    animation_object.collision_node = scene.add_primitive_mesh(cube, "collision boi", material);

    //    animation_objects.push_back(animation_object);
    //  }

    //  character_node->owned_children.clear();
    //  found_hp_bars = false;
    //}

    return;
  }

  if (hp_bar == NODE_NULL)
    return;

  float32 hp_percent = ((float32)lc->hp) / ((float32)lc->hp_max);
  Material_Descriptor *md = scene.get_modifiable_material_pointer_for(hp_bar, 0);
  md->emissive.mod = vec4(1.f - hp_percent, hp_percent, 0.f, 1.f);
}

void Warg_State::update_character_nodes()
{
  for (auto &character : current_game_state.characters)
  {
    Node_Index character_node = character_nodes[character.id];

    scene.nodes[character_node].velocity = (character.physics.position - scene.nodes[character_node].position) / dt;
    scene.nodes[character_node].position = character.physics.position;
    scene.nodes[character_node].orientation = character.physics.orientation;

    scene.nodes[character_node].scale = character.radius * vec3(2);

    update_hp_bar(character.id);

    animate_character(character.id);
  }
}

void Warg_State::update_prediction_ghost()
{
  auto player_character = find_if(current_game_state.characters, [&](auto &c) { return c.id == player_character_id; });
  if (player_character == current_game_state.characters.end())
    return;

  Character_Physics *physics = &player_character->physics;

  static Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.translucent_pass = true;
  material.albedo.mod = vec4(1, 1, 1, 0.5);

  static Node_Index ghost_mesh = scene.add_mesh(cube, "ghost_mesh", &material);
  scene.nodes[ghost_mesh].position = physics->position;
  scene.nodes[ghost_mesh].scale = player_character->radius * vec3(2);
  scene.nodes[ghost_mesh].velocity = physics->velocity;
  scene.nodes[ghost_mesh].orientation = physics->orientation;
}

void Warg_State::update_stats_bar()
{

  vec2 resolution = CONFIG.resolution;

  bool show_stats_bar = true;
  ImVec2 stats_bar_pos = {resolution.x - 50, resolution.y - 30};
  ImGui::SetNextWindowPos(stats_bar_pos);
  // ImGui::Begin("stats_bar", &show_stats_bar, ImVec2(10, 10), 0.5f,
  //    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
  //    |
  //        ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Begin("stats_bar", &show_stats_bar,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
          ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::End();
}

bool prediction_correct(UID player_character_id, Game_State &server_state, Game_State &predicted_state)
{
  // bool same_input = server_state.input_number == predicted_state.input_number;

  // set_message("preduction_correct()",
  //    s("server_state.input_number:", server_state.input_number,
  //        " predicted_state.input_number:", predicted_state.input_number),
  //    1.0f);

  // auto server_character = find_if(server_state.characters, [&](auto &c) { return c.id == player_character_id; });
  // auto predicted_character = find_if(predicted_state.characters, [&](auto &c) { return c.id == player_character_id;
  // });

  // if (server_character == server_state.characters.end() && predicted_character == predicted_state.characters.end())
  //  return same_input;

  ////if (!server_character != !predicted_character)
  ////  return false;

  // ASSERT(server_character);
  // ASSERT(predicted_character);

  // bool same_player_physics = server_character->physics == predicted_character->physics;

  // return same_input && same_player_physics;
  return true;
}

void Warg_State::predict_state()
{
  //// game_state = server_state;
  // return;
  // if (!last_recieved_server_state.get_character(player_character_id))
  //  return;

  // if (input_buffer.size() == 0)
  //  return;

  // while (state_buffer.size() > 0 && state_buffer.front().input_number < last_recieved_server_state.input_number)
  //  state_buffer.pop_front();

  // Game_State predicted_state = last_recieved_server_state;
  // size_t prediction_start = 0;

  // bool prediction_correct_result = false;
  // if (state_buffer.size() > 0)
  //{
  //  prediction_correct_result =
  //      prediction_correct(player_character_id, last_recieved_server_state, state_buffer.front());
  //}
  // set_message("predict_state(): prediction_correct_result:", s(prediction_correct_result));
  // if (prediction_correct_result)
  //{
  //  predicted_state = state_buffer.back();
  //  state_buffer.pop_front();
  //  prediction_start = input_buffer.size() - 1;
  //}
  // else
  //{
  //  state_buffer = {};
  //  predicted_state = last_recieved_server_state;
  //}

  // size_t prediction_iterations = 0;
  // for (size_t i = prediction_start; i < input_buffer.size(); i++)
  //{
  //  Input &input = input_buffer[i];

  //  Character *player_character = predicted_state.get_character(player_character_id);
  //  Character_Physics &physics = player_character->physics;
  //  vec3 radius = player_character->radius;
  //  /*float32 movement_speed = player_character->effective_stats.speed;*/

  //  move_char(predicted_state, *player_character, input, scene);
  //  if (vec3_has_nan(physics.position))
  //    physics.position = map->spawn_pos[player_character->team];

  //  predicted_state.input_number = input.number;
  //  state_buffer.push_back(predicted_state);

  //  prediction_iterations++;
  //}
  //// set_message("prediction iterations:", s(prediction_iterations), 5);

  // current_game_state = state_buffer.back();
}

void Warg_State::update_meshes()
{
  for (auto &character : current_game_state.characters)
  {
    if (!character_nodes.count(character.id))
    {
      add_character_mesh(character.id);
    }
  }
}

void Warg_State::update_spell_object_nodes()
{
  Material_Descriptor material;
  material.albedo.mod = vec4(0.f);
  material.emissive.mod = vec4(0.5f, 0.5f, 100.f, 1.f);

  for (auto &so : current_game_state.spell_objects)
  {
    if (spell_object_nodes.count(so.id) == 0)
    {
      spell_object_nodes[so.id] = scene.add_mesh(cube, "spell_object_cube", &material);
      // set_message(s("adding spell object node on tick ", game_state.tick), "", 20);
    }
    Node_Index mesh = spell_object_nodes[so.id];
    scene.nodes[mesh].scale = vec3(0.4f);
    scene.nodes[mesh].position = so.pos;
  }

  std::vector<UID> to_erase;
  for (auto &node : spell_object_nodes)
  {
    bool orphaned = true;
    for (auto &so : current_game_state.spell_objects)
    {
      if (node.first == so.id)
        orphaned = false;
    }
    if (orphaned)
      to_erase.push_back(node.first);
  }
  for (UID node_id : to_erase)
  {
    scene.delete_node(spell_object_nodes[node_id]);
    spell_object_nodes.erase(node_id);
    // set_message(s("erasing spell object node on tick ", game_state.tick), "", 20);
  }
}

void Warg_State::animate_character(UID character_id)
{
  // set_message("animate_character uid:", s(character_id), 1.0f);
  static std::map<UID, float32> animation_times;
  static std::map<UID, vec3> last_positions;
  static std::map<UID, bool> last_grounded;

  auto character = find_if(current_game_state.characters, [&](auto &c) { return c.id == character_id; });

  const auto &lc = std::find_if(current_game_state.living_characters.begin(),
      current_game_state.living_characters.end(), [&](auto &lc) { return lc.id == character_id; });

  if (lc == current_game_state.living_characters.end())
    return;

  ASSERT(character_nodes.count(character_id));

  // see boi dressing room function for why this is named fake_character_node
  Node_Index fake_character_node = character_nodes[character_id];
  ASSERT(fake_character_node != NODE_NULL);

  // set_message("character_node:", s(character_node), 1.0f);

  Node_Index character_node = scene.nodes[fake_character_node].children[0];

  Node_Index left_shoe = scene.find_by_name(character_node, "left_shoe");
  Node_Index right_shoe = scene.find_by_name(character_node, "right_shoe");
  Node_Index left_shoulder_joint = scene.find_by_name(character_node, "left_shoulder_joint");
  Node_Index right_shoulder_joint = scene.find_by_name(character_node, "right_shoulder_joint");
  ASSERT(left_shoe != NODE_NULL);
  ASSERT(right_shoe != NODE_NULL);
  ASSERT(left_shoulder_joint != NODE_NULL);
  ASSERT(right_shoulder_joint != NODE_NULL);

  if (!last_positions.count(character_id))
    last_positions[character_id] = scene.nodes[fake_character_node].position;
  vec3 last_position = last_positions[character_id];

  if (!animation_times.count(character_id))
    animation_times[character_id] = 0;
  float32 *animation_time = &animation_times[character_id];
  *animation_time += dt;

  if (scene.nodes[fake_character_node].position == last_position || !character->physics.grounded)
  {
    scene.nodes[left_shoe].position.y = scene.nodes[right_shoe].position.y = 0.f;
    *animation_time = 0;
    return;
  }

  auto linear_oscillate = [](float32 m, float32 x, float32 b) {
    return float32(4.f * abs(fract(m * x + b) - 0.5f) - 1.f);
  };

  float32 cadence = lc->effective_stats.speed / STEP_SIZE;

  float32 m = cadence / 2;
  float32 x = *animation_time;
  float32 b = -cadence / 4;
  float32 current_progress = linear_oscillate(m, x, b);

  scene.nodes[left_shoe].position.y = (STEP_SIZE / 2) * current_progress / scene.nodes[character_node].scale.y;
  scene.nodes[right_shoe].position.y = -(STEP_SIZE / 2) * current_progress / scene.nodes[character_node].scale.y;

  // vec3 axis = vec3(1.f, 0.f, 0.f);
  // left_shoulder_joint->orientation = angleAxis(step, axis);
  // right_shoulder_joint->orientation = angleAxis(-step, axis);

  last_positions[character_id] = scene.nodes[fake_character_node].position;
}

void Warg_State::update_animation_objects()
{
  for (Animation_Object &animation_object : animation_objects)
  {
    animation_object.physics.velocity.z -= 9.81f * dt;
    if (animation_object.physics.velocity.z < -53)
      animation_object.physics.velocity.z = -53;
    if (animation_object.physics.grounded)
      animation_object.physics.velocity.z = 0;

    collide_and_slide_char(animation_object.physics, animation_object.radius, animation_object.physics.velocity,
        vec3(0.f, 0.f, animation_object.physics.velocity.z), scene);

    scene.nodes[animation_object.node].position = animation_object.physics.position;

    if (animation_object.physics.grounded)
      animation_object.physics.velocity = vec3(0);

    scene.nodes[animation_object.collision_node].position = animation_object.physics.position;
    scene.nodes[animation_object.collision_node].scale = animation_object.radius;
  }
}

void Warg_State::send_wargspy_request()
{
  ASSERT(wargspy_state.host);
  ASSERT(wargspy_state.wargspy);
  ASSERT(!wargspy_state.connecting);

  uint8 byte = 2;
  ENetPacket *packet = enet_packet_create(&byte, 1, 0);
  enet_peer_send(wargspy_state.wargspy, 0, packet);
  enet_host_flush(wargspy_state.host);
  wargspy_state.asking = true;
}

void Warg_State::update_wargspy()
{
  if (wargspy_state.host == nullptr)
  {
    wargspy_state.host = enet_host_create(NULL, 1, 2, 0, 0);
    if (wargspy_state.host == nullptr)
      return;
  }

  if (wargspy_state.wargspy == nullptr)
  {
    ENetAddress addr;
    enet_address_set_host(&addr, CONFIG.wargspy_address.c_str());
    addr.port = WARGSPY_PORT;
    wargspy_state.wargspy = enet_host_connect(wargspy_state.host, &addr, 2, 0);
    wargspy_state.connecting = true;
  }

  if (wargspy_state.wargspy && !wargspy_state.connecting && (wargspy_state.time_till_next_ask -= dt) < 0)
  {
    send_wargspy_request();
    wargspy_state.time_till_next_ask = 3;
  }

  ENetEvent e;
  while (enet_host_service(wargspy_state.host, &e, 0) > 0)
  {
    if (e.type == ENET_EVENT_TYPE_CONNECT)
    {
      wargspy_state.connecting = false;
    }

    if (e.type == ENET_EVENT_TYPE_RECEIVE)
    {
      Buffer b;
      b.insert(e.packet->data, e.packet->dataLength);

      uint8 type;
      deserialize(b, type);
      wargspy_state.game_server_address = 0;
      if (type == 1)
      {
        uint32 game_server_ip_address;
        deserialize(b, wargspy_state.game_server_address);
      }
      wargspy_state.asking = false;
    }
  }
}

void Warg_State::update()
{
  update_wargspy();

  if (!session)
  {
    static bool first = true;
    static Resource_Manager *cached_rm = new Resource_Manager();
    static Scene_Graph *cached_scene = new Scene_Graph(cached_rm);
    static Node_Index boi = add_boi_character_mesh(*cached_scene);
    static Node_Index girl = add_girl_character_mesh(*cached_scene);
    static std::vector<Node_Index> cuber_things;
#ifdef NDEBUG
    uint32 max = 12;
#else
    uint32 max = 3;
#endif
    if (first)
    {
      cached_scene->nodes[boi].scale = vec3(.39, 0.30, 1.61);
      cached_scene->nodes[girl].scale = vec3(0.61, .68, 0.97) * vec3(.39, 0.30, 1.61);
      cached_scene->initialize_lighting("Assets/Textures/Environment_Maps/GrandCanyon_C_YumaPoint/irradiance.hdr",
          "Assets/Textures/Environment_Maps/GrandCanyon_C_YumaPoint/irradiance.hdr");
      cached_scene->nodes[girl].position = vec3(.7, 0, 0);
      cached_scene->nodes[boi].position = vec3(-.7, 0, 0);
      cached_scene->lights.light_count = 1;
      Light *light0 = &cached_scene->lights.lights[0];
      light0->position = vec3(0.00000, -4.00000, 2.00000);
      light0->direction = vec3(0.00000, 0.00000, 0.00000);
      light0->brightness = 66.00000;
      light0->color = vec3(1.00000, 1.00000, 0.99999);
      light0->attenuation = vec3(1.00000, 0.22000, 0.00000);
      light0->ambient = 0.00000;
      light0->radius = -0.33000;
      light0->cone_angle = 0.13200;
      light0->type = Light_Type::spot;
      light0->casts_shadows = 1;
      light0->shadow_blur_iterations = 4;
      light0->shadow_blur_radius = 0.051010999829;
      light0->shadow_near_plane = 2.400000095367;
      light0->shadow_far_plane = 8.000000000000;
      light0->max_variance = 0.000000000000;
      light0->shadow_fov = 1.592529296875;
      light0->shadow_map_resolution = ivec2(2048, 2048);

      Material_Descriptor mat;
      mat.albedo.mod = vec4(.1);
      Node_Index cube_ref = cached_scene->add_mesh(cube, s("cuberthingref"), &mat);
      cached_scene->nodes[cube_ref].visible = false;
      for (uint32 x = 0; x < max; ++x)
      {
        for (uint32 y = 0; y < max; ++y)
        {
          Node_Index node = cached_scene->new_node();
          cached_scene->nodes[node].model = cached_scene->nodes[cube_ref].model;
          cuber_things.push_back(node);
        }
      }
      first = false;
    }
    if (want_set_intro_scene)
    {
      scene.resource_manager = cached_rm;
      scene.nodes = cached_scene->nodes;
      scene.lights = cached_scene->lights;
      scene.highest_allocated_node = cached_scene->highest_allocated_node;
      want_set_intro_scene = false;
    }
    camera.pos = vec3(0, 5, .5) + vec3(cos(.2f * current_time), sin(.2f * current_time), 0);
    camera.dir = -camera.pos + vec3(0, 0, .5f);
    uint32 i = 0;
    for (uint32 x = 0; x < max; ++x)
    {
      for (uint32 y = 0; y < max; ++y)
      {
        Node_Index node = cuber_things[i];
        float32 z = .4f * (cos(x + 1.f * current_time) + sin(y + 1.f * current_time));
        vec3 pos = .3f * vec3(x, y, z) - 0.5f * .3f * vec3(max, max, 0);
        pos = pos - vec3(0, 0, 1.f);
        scene.nodes[node].position = pos;
        scene.nodes[node].scale = vec3(.3f);
        ++i;
      }
    }
    renderer.set_camera(camera.pos, camera.dir);
    return;
  }

  session->get_state(current_game_state, player_character_id);

  auto target = find_if(current_game_state.characters, [&](auto &c) { return c.id == target_id; });
  const auto &tlc = std::find_if(current_game_state.living_characters.begin(),
      current_game_state.living_characters.end(), [&](auto &lc) { return lc.id == target_id; });
  if (tlc == current_game_state.living_characters.end())
    target_id = 0;
  predict_state();
  set_camera_geometry();
  update_meshes();
  update_character_nodes();
  update_prediction_ghost();
  update_spell_object_nodes();
  // update_animation_objects();

  renderer.set_camera(camera.pos, camera.dir);

  // scene.collision_octree.clear();

  {
    Node_Index arena = scene.nodes[map->node].children[0];
    // Node_Index arena_collider = scene.nodes[arena].collider;
    // Mesh_Index meshindex = scene.nodes[arena_collider].model[0].first;
    // Mesh *meshptr = &scene.resource_manager->mesh_pool[meshindex];
    // Mesh_Descriptor *blades_edge_mesh_descriptor = &meshptr->mesh->descriptor;

    // scene.collision_octree.push(blades_edge_mesh_descriptor);

    // set_message("octree timer:", beatimer.string_report(), 1.0f);

    auto me = find_if(current_game_state.characters, [&](auto &c) { return c.id == player_character_id; });
    if (me == current_game_state.characters.end())
      return;
    Mesh_Descriptor mesh(cube, "spawnedcube");
    mat4 transform = scene.build_transformation(character_nodes[player_character_id]);
    AABB probe(me->physics.position);
    vec3 size = me->radius;

    float32 epsilon = 0.05;
    probe.min = me->physics.position - size;
    probe.max = me->physics.position + size;

    transform[0][0] = size.x * 2.f;
    transform[1][1] = size.y * 2.f;
    transform[2][2] = size.z * 2.f;

    const Triangle_Normal *result = nullptr;

    static vec3 last_position = me->physics.position;
    vec3 velocity = me->physics.position - last_position;
    last_position = me->physics.position;

    static std::vector<Node_Index> spawned_nodes;

    // if (WANT_SPAWN_OCTREE_BOX)
    //{
    //  WANT_SPAWN_OCTREE_BOX = false;
    //  Material_Descriptor mat;
    //  mat.albedo = "Assets/Textures/3D_pattern_62/pattern_335/diffuse.png";
    //  mat.normal = "3D_pattern_62/pattern_335/normal.png";
    //  mat.roughness.mod.x = 1.0f;
    //  mat.metalness.mod.x = 0.f;

    //  Node_Index newnode = scene.add_mesh(cube, "spawned box", &mat);
    //  Local_Session *ptr = (Local_Session *)this->session;
    //  Node_Index servernode = ptr->scene.add_mesh(cube, "spawned box", &mat);

    //  spawned_nodes.push_back(newnode);
    //  spawned_server_nodes.push_back(servernode);
    //  vec3 dir = normalize(camera.dir);
    //  vec3 axis = normalize(cross(dir, vec3(0, 0, 1)));
    //  float32 angle2 = atan2(dir.y, dir.x);
    //  scene.nodes[newnode].position = camera.pos + 12.f * dir;
    //  scene.nodes[newnode].scale = vec3(2);
    //  scene.nodes[newnode].orientation = angleAxis(-camera.phi, axis) * angleAxis(angle2, vec3(0, 0, 1));

    //  ptr->scene.nodes[servernode].position = camera.pos + 12.f * dir;
    //  ptr->scene.nodes[servernode].scale = vec3(2);
    //  ptr->scene.nodes[servernode].orientation = angleAxis(-camera.phi, axis) * angleAxis(angle2, vec3(0, 0, 1));
    //}
    // Local_Session *ptr = (Local_Session *)this->session;
    // ptr->scene.collision_octree.clear();

    // ptr->scene.collision_octree.push(md);
    // for (Node_Index node : spawned_nodes)
    //{
    //  mat4 M = scene.build_transformation(node);
    //  scene.collision_octree.push(&mesh, &M);
    //  AABB prober(scene.nodes[node].position);
    //  push_aabb(prober, scene.nodes[node].position + 0.5f * scene.nodes[node].scale);
    //  push_aabb(prober, scene.nodes[node].position - 0.5f * scene.nodes[node].scale);
    //  uint32 counter;
    //  scene.collision_octree.test(prober, &counter);
    //}
    // for (Node_Index node : spawned_server_nodes)
    //{
    //  Local_Session *ptr = (Local_Session *)this->session;
    //  mat4 M = ptr->scene.build_transformation(node);
    //  ptr->scene.collision_octree.push(&mesh, &M);
    //}

    if (WANT_CLEAR_OCTREE)
    {
      ASSERT(0);
      WANT_CLEAR_OCTREE = false;
      for (Node_Index node : spawned_nodes)
      {
        scene.delete_node(node);
      }
      spawned_nodes.clear();

      //  for (Node_Index servernode : spawned_server_nodes)
      //  {
      //    Local_Session *ptr = (Local_Session *)this->session;
      //    ptr->scene.delete_node(servernode);
      //  }
      //  spawned_server_nodes.clear();
    }
    velocity = 300.f * (velocity + vec3(0., 0., .02));
    // scene.collision_octree.push(&mesh, &transform, &velocity);

    Material_Descriptor material;
    // static Node_Index dynamic_collider_node = scene.add_mesh(cube, "dynamic_collider_node", &material);

    // scene.nodes[dynamic_collider_node].position = probe.min + (0.5f * (probe.max - probe.min));
    // transform = scene.build_transformation(dynamic_collider_node);
    // scene.collision_octree.push(&mesh, &transform, &velocity);

    // fire_emitter2(
    //   &renderer, &scene, &scene.particle_emitters[0], &scene.lights.lights[0], vec3(-5.15, -17.5, 8.6), vec2(.5));
    // fire_emitter2(
    //  &renderer, &scene, &scene.particle_emitters[1], &scene.lights.lights[1], vec3(5.15, -17.5, 8.6), vec2(.5));
    // fire_emitter2(&renderer, &scene, &scene.particle_emitters[2], &scene.lights.lights[2], vec3(0, 0, 10), vec2(.5));
    // fire_emitter2(
    // &renderer, &scene, &scene.particle_emitters[3], &scene.lights.lights[3], vec3(5.15, 17.5, 8.6), vec2(.5));
    static vec3 wind_dir;
    if (fract(sin(current_time)) > .85)
      wind_dir = vec3(.575, .575, .325) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

    if (fract(sin(current_time)) > .85)
      wind_dir = vec3(.575, .575, 0) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

      // Node_Index p0 = scene.find_by_name(NODE_NULL, "particle0");
      // Node_Index p1 = scene.find_by_name(NODE_NULL, "particle1");
      // Node_Index p2 = scene.find_by_name(NODE_NULL, "particle2");
      // Node_Index p3 = scene.find_by_name(NODE_NULL, "particle3");

#if 0
  scene.particle_emitters[1].descriptor.position = scene.nodes[p1].position;
  scene.particle_emitters[1].descriptor.emission_descriptor.initial_position_variance = vec3(1, 1, 0);
  scene.particle_emitters[1].descriptor.emission_descriptor.particles_per_second = 15;
  scene.particle_emitters[1].descriptor.emission_descriptor.minimum_time_to_live = 45;
  scene.particle_emitters[1].descriptor.emission_descriptor.initial_scale = scene.nodes[p1].scale;
  // scene.particle_emitters[1].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  scene.particle_emitters[1].descriptor.physics_descriptor.type = wind;
  scene.particle_emitters[1].descriptor.physics_descriptor.direction = wind_dir;
  scene.particle_emitters[1].descriptor.physics_descriptor.octree = &scene.collision_octree;
  //scene.particle_emitters[1].update(renderer.projection, renderer.camera, dt);
  scene.particle_emitters[1].descriptor.physics_descriptor.intensity = random_between(5.f, 5.f);
  scene.particle_emitters[1].descriptor.physics_descriptor.bounce_min = 0.82;
  scene.particle_emitters[1].descriptor.physics_descriptor.bounce_max = 0.95;

  scene.particle_emitters[2].descriptor.position = scene.nodes[p2].position;
  scene.particle_emitters[2].descriptor.emission_descriptor.initial_position_variance = vec3(1, 1, 0);
  scene.particle_emitters[2].descriptor.emission_descriptor.particles_per_second = 15;
  scene.particle_emitters[2].descriptor.emission_descriptor.minimum_time_to_live = 45;
  scene.particle_emitters[2].descriptor.emission_descriptor.initial_scale = scene.nodes[p2].scale;
  // scene.particle_emitters[2].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  scene.particle_emitters[2].descriptor.physics_descriptor.type = wind;
  scene.particle_emitters[2].descriptor.physics_descriptor.direction = wind_dir;
  scene.particle_emitters[2].descriptor.physics_descriptor.octree = &scene.collision_octree;
  //scene.particle_emitters[2].update(renderer.projection, renderer.camera, dt);
  scene.particle_emitters[2].descriptor.physics_descriptor.intensity = random_between(5.f, 5.f);
  scene.particle_emitters[2].descriptor.physics_descriptor.bounce_min = 0.82;
  scene.particle_emitters[2].descriptor.physics_descriptor.bounce_max = 0.95;

  scene.particle_emitters[3].descriptor.position = scene.nodes[p3].position;
  scene.particle_emitters[3].descriptor.emission_descriptor.initial_position_variance = vec3(1, 1, 0);
  scene.particle_emitters[3].descriptor.emission_descriptor.particles_per_second = 15;
  scene.particle_emitters[3].descriptor.emission_descriptor.minimum_time_to_live = 45;
  scene.particle_emitters[3].descriptor.emission_descriptor.initial_scale = scene.nodes[p3].scale;
  // scene.particle_emitters[3].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  scene.particle_emitters[3].descriptor.physics_descriptor.type = wind;
  scene.particle_emitters[3].descriptor.physics_descriptor.direction = wind_dir;
  scene.particle_emitters[3].descriptor.physics_descriptor.octree = &scene.collision_octree;
  //scene.particle_emitters[3].update(renderer.projection, renderer.camera, dt);
  scene.particle_emitters[3].descriptor.physics_descriptor.intensity = random_between(5.f, 5.f);
  scene.particle_emitters[3].descriptor.physics_descriptor.bounce_min = 0.82;
  scene.particle_emitters[3].descriptor.physics_descriptor.bounce_max = 0.95;

  scene.particle_emitters[0].descriptor.position = scene.nodes[p0].position;
  scene.particle_emitters[0].descriptor.emission_descriptor.initial_position_variance = vec3(1, 1, 0);
  scene.particle_emitters[0].descriptor.emission_descriptor.particles_per_second = 15;
  scene.particle_emitters[0].descriptor.emission_descriptor.minimum_time_to_live = 42;
  scene.particle_emitters[0].descriptor.emission_descriptor.initial_scale = scene.nodes[p0].scale;
  // scene.particle_emitters[0].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  scene.particle_emitters[0].descriptor.physics_descriptor.type = wind;
  scene.particle_emitters[0].descriptor.physics_descriptor.direction = wind_dir;
  scene.particle_emitters[0].descriptor.physics_descriptor.octree = &scene.collision_octree;
  //scene.particle_emitters[0].update(renderer.projection, renderer.camera, dt);
  scene.particle_emitters[0].descriptor.physics_descriptor.intensity = random_between(5.f, 5.f);
  scene.particle_emitters[0].descriptor.physics_descriptor.bounce_min = 0.82;
  scene.particle_emitters[0].descriptor.physics_descriptor.bounce_max = 0.95;
#endif

    uint32 i = sizeof(Octree);

    // if (terrain.apply_geometry_to_octree_if_necessary(&scene))
    //{
    // mat4 M_blades_edge = scene.build_transformation(map->node);
    // scene.collision_octree.push(blades_edge_mesh_descriptor, &M_blades_edge);

    // mat4 M_terrain = scene.build_transformation(terrain.ground);
    // scene.collision_octree.push(&terrain.terrain_geometry, &M_terrain);

    // for (Node_Index node : spawned_nodes)
    //{
    //  mat4  M = scene.build_transformation(node);
    //  Mesh_Descriptor *newmesh =
    //      &scene.resource_manager->mesh_pool[scene.nodes[node].model[0].first]
    //           .mesh->descriptor;
    //  scene.collision_octree.push(newmesh, &M);
    //}

    //  //this is bad: we can't just copy an octree like this because it ruins the pointers
    //  //however it works if we dont push to it again and only do tests...
    //  server_ptr->server->scene.collision_octree = scene.collision_octree;
    //}

    // bug:

    // when we make this frostbolt effect, we pass in the state
    // inside, it creates a new node
    // this node is being written to by some warg game code somewhere else that's assuming it
    // has this index

    // it appears that something is writing a warg character scale to it, but not a name
    // this is probably related to the bug of the flying characters
    // it can probably be found by checking for a specific name before u write to the scale/position
    // "frostbolt crystal" is the name i created this node with, try breaking on a match for that

    // you cant assume that scene graph node indices are the same for both the game client and game server
    // you can't create a node on the server, and then pass that node index to the client for it to use
    // a node index is only valid for the scene that created it
    // a local scene might have 30 little widgets that get rendered and initialized that the server doesnt
    // care about

    if (!frostbolt_effect2)
    {
      uint32 light_index = scene.lights.light_count + 1;
      frostbolt_effect2 = std::make_unique<Frostbolt_Effect_2>((State *)this, light_index);
    }

    vec3 ray = renderer.ray_from_screen_pixel(this->cursor_position);
    vec3 intersect_result;
    Node_Index node_result = scene.ray_intersects_node(renderer.camera_position, ray, arena, intersect_result);
    vec3 frostbolt_dst = vec3(0, 0, 1);
    if (node_result == arena)
    {
      frostbolt_dst = intersect_result;
    }
    if (!frostbolt_effect2->update(this, frostbolt_dst))
    {
      frostbolt_effect2->position = 31.f * random_3D_unit_vector();
      frostbolt_effect2->position.z = abs(frostbolt_effect2->position.z);
    }
  }
}

void Warg_State::draw_gui()
{
  // opengl code is allowed in here
  IMGUI_LOCK lock(this); // you must own this lock during ImGui:: calls

  while (!interface_state.icons_loaded)
  {
    int n_unready = 0;
    for (auto &[spell, icon] : SPELL_DB.icon)
    {
      icon.load();
      n_unready += !icon.is_ready();
    }
    for (auto &[spell, icon] : SPELL_DB.buff_icon)
    {
      icon.load();
      n_unready += !icon.is_ready();
    }
    interface_state.icons_loaded = n_unready == 0;
  }

  update_game_interface();
  // terrain.window_open = true;

  scene.draw_imgui(state_name);

  ImGui::Begin("warg state test");
  ImGui::Text("blah1");
  ImGui::Text("blah2");
  // terrain.run(this);
  ImGui::End();
  session_swapper();

  if (session)
    draw_chat_box();
}

void Warg_State::draw_chat_box()
{

  std::vector<Chat_Message> chat_log = session->get_chat_log();

  const int w = 500, h = 250;

  vec2 resolution = CONFIG.resolution;
  bool display = true;
  // ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowPos(ImVec2(10, resolution.y - h - 40));
  ImGui::SetNextWindowSize(ImVec2(w, h + 30));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10, 10));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("chatbox", &display,
      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing);

  // ImGui::SetCursorPos(v(grid.get_position(0, 0)));
  // ImGui::ProgressBar(progress, v(grid.get_section_size(1, 1)), "");

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

  static int want_scroll_down = 0;

  ImGuiWindowFlags childflags = ImGuiWindowFlags_None;
  ImGui::BeginChild("Console_Log:", ImVec2(w, h), true, childflags);

  for (auto &m : chat_log)
  {
    ImGui::TextWrapped("%s: %s", m.name.c_str(), m.message.c_str());
  }
  // ImGui::TextWrapped(graph_console_log.c_str());
  if (want_scroll_down > 0)
  {
    ImGui::SetScrollY(ImGui::GetScrollMaxY());
    want_scroll_down = want_scroll_down - 1;
  }
  ImGui::EndChild();

  ImGui::BeginChild("Console_Cmd:");
  static std::string buf;
  buf.resize(512);
  size_t size = buf.size();
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
  // ImGui::InputTextMultiline("blah", &buf[0], size, section_size, flags);
  ImGui::SetNextItemWidth(w);
  if (ImGui::InputText("", &buf[0], int(size), flags))
  {
    std::string_view sv = {buf.c_str(), strlen(buf.c_str())};
    if (sv.length())
    {
      session->send_chat_message(sv);
      buf.clear();
      buf.resize(512);
      want_scroll_down = 3;
    }
    interface_state.focus_chat = false;
  }

  if (interface_state.focus_chat)
  {
    ImGui::SetKeyboardFocusHere(-1);
    interface_state.focus_chat = false;
  }

  ImGui::IsItemEdited();
  ImGui::EndChild();
  ImGui::PopStyleVar(1);
  ImGui::End();
  ImGui::PopStyleVar(3);
}

Node_Index add_girl_character_mesh(Scene_Graph &scene)
{
  // Material_Descriptor hp_bar_material;
  // Node_Index hp_bar = scene.add_mesh(cube, "hp_bar", &hp_bar_material);
  // scene.set_parent(hp_bar, character_node);
  // float32 bar_z_pos = character->radius.z;
  // scene.nodes[hp_bar].position = vec3(0.f, 0.f, 0.6f);
  // scene.nodes[hp_bar].scale = vec3(1.2f, 1.2f, 0.07f);

  // hp_bar_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
  // Node_Index hp_bar_background = scene.add_mesh(cube, "hp_bar_background", &hp_bar_material);
  // scene.set_parent(hp_bar_background, character_node);
  // scene.nodes[hp_bar_background].position = vec3(0.f, 0.f, 0.6f);
  // scene.nodes[hp_bar_background].scale = vec3(1.2f, 1.2f, 0.07f);

  // hp_bar_material.albedo.mod = vec4(.4f, .5f, .4f, 1.f);
  // Node_Index hp_bar_foreground = scene.add_mesh(cube, "hp_bar_foreground", &hp_bar_material);
  // scene.set_parent(hp_bar_foreground, character_node);
  // scene.nodes[hp_bar_foreground].position = vec3(0.f, 0.f, 0.6f);
  // scene.nodes[hp_bar_foreground].scale = vec3(1.21f, 1.21f, 0.08f);

  Material_Descriptor skin_material;
  skin_material.albedo.mod = rgb_vec4(253, 228, 200);
  skin_material.metalness.mod = vec4(0);
  skin_material.roughness.mod = vec4(0.5);

  Material_Descriptor suit_material;
  suit_material.albedo.mod = vec4(0, 0, 0, 1);
  suit_material.roughness.mod = vec4(1);
  suit_material.metalness.mod = vec4(0);
  suit_material.normal = "cloth_normal.jpg";
  suit_material.uv_scale = vec2(0.25f);

  Material_Descriptor hair_material;
  hair_material.albedo.mod = vec4(0, 0, 0, 1);
  hair_material.roughness.mod = vec4(0.89);
  hair_material.metalness.mod = vec4(0);
  hair_material.normal = "hair_normal.jpg";
  hair_material.uv_scale = vec2(1.f);

  Material_Descriptor shoe_material;
  // shoe_material.albedo.mod = Conductor_Reflectivity::gold;
  shoe_material.albedo.mod = vec4(0, 0, 0, 1);
  shoe_material.roughness.mod = vec4(0.11);
  shoe_material.metalness.mod = vec4(0);

  Material_Descriptor shirt_material = suit_material;
  shirt_material.albedo.mod = vec4(1);
  shirt_material.uv_scale = vec2(1.);
  Material_Descriptor solid_material = shirt_material;

  // proper way to do this after we define the materials we want to use:

  // make a cube
  Mesh_Descriptor md(cube, "girl's cube");

  // add it as a referencable object to the resource manager
  Mesh_Index cube_mesh_index = scene.resource_manager->push_mesh(&md);

  // add the materials as referencable objects to the resource manager
  Material_Index skin_material_index = scene.resource_manager->push_material(&skin_material);
  Material_Index suit_material_index = scene.resource_manager->push_material(&suit_material);
  Material_Index shirt_material_index = scene.resource_manager->push_material(&shirt_material);
  Material_Index shoe_material_index = scene.resource_manager->push_material(&shoe_material);
  Material_Index hair_material_index = scene.resource_manager->push_material(&hair_material);

  // now, instead of add_mesh, we do:
  /*
    //get a blank empty node
    Node_Index shirt_node = scene.new_node();

    //set some properties of this node
    scene.nodes[shirt_node].name = "shirt";
    scene.set_parent(shirt_node, character_node);
    scene.nodes[shirt_node].position = vec3(0.f, 0.0f, 0.15f);
    scene.nodes[shirt_node].scale = vec3(1.05f, 1.05f, 0.3f);
    ...

    //and finally tell it what mesh and materials to use for it when rendering
    //these are "pointers" to the source mesh and materials above
    //we hold one real copy of the cube, and one real copy of each material
    //and when this node is rendered, it gets pointers to the real mesh/mat in the resource manager

    scene.nodes[shirt_node].model[0] = std::pair<Mesh_Index,Material_Index>(cube_mesh_index,skin_material_index);


    I made a helper function for this
    Node_Index my_node = scene.new_node("my_name",meshmat_pair,my_desired_parent_node);
    scene.nodes[my_node].position = vec3(0.f, 0.0f, 0.15f);
    scene.nodes[my_node].scale = vec3(1.05f, 1.05f, 0.3f);

  */

  // helper for assignment below
  std::pair<Mesh_Index, Material_Index> skin_pair = {cube_mesh_index, skin_material_index};
  std::pair<Mesh_Index, Material_Index> suit_pair = {cube_mesh_index, suit_material_index};
  std::pair<Mesh_Index, Material_Index> shirt_pair = {cube_mesh_index, shirt_material_index};
  std::pair<Mesh_Index, Material_Index> shoe_pair = {cube_mesh_index, shoe_material_index};
  std::pair<Mesh_Index, Material_Index> hair_pair = {cube_mesh_index, hair_material_index};

  Node_Index character_node = scene.new_node("cubegirl", skin_pair);
  scene.nodes[character_node].position = vec3(0, 0, -0.01);
  scene.nodes[character_node].scale = vec3(0.61, .68, 0.97);
  scene.nodes[character_node].scale_vertex = vec3(0.5, .7, 1.);

  {
    // shirt
    Node_Index shirt = scene.new_node("shirt", shirt_pair, character_node);
    scene.nodes[shirt].position = vec3(0.f, 0.0f, 0.15f);
    scene.nodes[shirt].scale = vec3(1.05f, 1.05f, 0.3f);

    // tie
    Node_Index tie = scene.new_node("tie", suit_pair, shirt);
    scene.nodes[tie].position = vec3(0.f, 0.f, 0.17f);
    scene.nodes[tie].scale = vec3(0.2f, 1.1f, .67f);

    // jacket left

    Node_Index jacket_left = scene.new_node("jacket_left", suit_pair, shirt);
    scene.nodes[jacket_left].position = vec3(0.425f, 0.0f, -0.09f);
    scene.nodes[jacket_left].scale = vec3(0.3f, 1.1f, 1.2f);

    // jacket right
    Node_Index jacket_right = scene.new_node("jacket_right", suit_pair, shirt);
    scene.nodes[jacket_right].position = vec3(-0.425f, 0.0f, -0.09f);
    scene.nodes[jacket_right].scale = vec3(0.3f, 1.1f, 1.2f);

    // jacket back
    Node_Index jacket_back = scene.new_node("jacket_back", suit_pair, shirt);
    scene.nodes[jacket_back].position = vec3(0.f, -0.5f, -0.1f);
    scene.nodes[jacket_back].scale = vec3(1.f, .2f, 1.2f);

    // belt
    Node_Index belt = scene.new_node("belt", hair_pair, character_node);
    scene.nodes[belt].position = vec3(0.f, 0.03f, 0.0f);
    scene.nodes[belt].scale = vec3(1.02f, 1.02f, 0.025f);

    // belt buckle
    Node_Index belt_buckle = scene.new_node("belt_buckle", shoe_pair, belt);
    scene.nodes[belt_buckle].position = vec3(0.f, 0.f, 0.0f);
    scene.nodes[belt_buckle].scale = vec3(0.1f, 1.02f, 1.f);

    // pants
    Node_Index pants = scene.new_node("pants", suit_pair, character_node);
    scene.nodes[pants].position = vec3(0.f, 0.f, -0.25f);
    scene.nodes[pants].scale = vec3(1.05f, 1.05f, 0.5f);

    // left shoe
    Node_Index left_shoe = scene.new_node("left_shoe", shoe_pair, character_node);
    scene.nodes[left_shoe].position = vec3(0.25f, 0.75f, -0.47f);
    scene.nodes[left_shoe].scale = vec3(.4f, 1.5f, 0.06f);

    // right shoe
    Node_Index right_shoe = scene.new_node("right_shoe", shoe_pair, character_node);
    scene.nodes[right_shoe].position = vec3(-0.25f, 0.75f, -0.47f);
    scene.nodes[right_shoe].scale = vec3(.4f, 1.5f, 0.06f);

    // right shoulder_joint
    Node_Index right_shoulder_joint = scene.new_node("right_shoulder_joint", suit_pair, character_node);
    scene.nodes[right_shoulder_joint].visible = false;
    scene.nodes[right_shoulder_joint].propagate_visibility = false;
    scene.nodes[right_shoulder_joint].position = vec3(0.8f, 0.f, 0.3f);

    // right arm
    Node_Index right_arm = scene.new_node("right_arm", skin_pair, right_shoulder_joint);
    scene.nodes[right_arm].position = vec3(-0.03f, 0.f, -0.15f);
    scene.nodes[right_arm].scale = vec3(0.3f, 0.3f, 0.29f);

    // right sleeve
    Node_Index right_sleeve = scene.new_node("right_sleeve", suit_pair, right_arm);
    scene.nodes[right_sleeve].position = vec3(0.f, 0.f, 0.35f);
    scene.nodes[right_sleeve].scale = vec3(1.15f, 1.15f, 0.35f);

    // left shoulder_joint
    Node_Index left_shoulder_joint = scene.new_node("left_shoulder_joint", suit_pair, character_node);
    scene.set_parent(left_shoulder_joint, character_node);
    scene.nodes[left_shoulder_joint].visible = false;
    scene.nodes[left_shoulder_joint].propagate_visibility = false;
    scene.nodes[left_shoulder_joint].position = vec3(-0.8f, 0.f, 0.3f);

    // left arm
    Node_Index left_arm = scene.new_node("left_arm", skin_pair, left_shoulder_joint);
    scene.nodes[left_arm].position = vec3(0.03f, 0.f, -0.15f);
    scene.nodes[left_arm].scale = vec3(0.3f, 0.3f, 0.29f);

    // left sleeve
    Node_Index left_sleeve = scene.new_node("left_sleeve", suit_pair, left_arm);
    scene.nodes[left_sleeve].position = vec3(0.f, 0.f, 0.1f);
    scene.nodes[left_sleeve].scale = vec3(1.5f, 1.5f, 0.85f);

    // head
    Node_Index head = scene.new_node("head", skin_pair, character_node);
    scene.nodes[head].position = vec3(0.f, 0.07f, 0.4f);
    scene.nodes[head].scale = vec3(.95f, .86f, 0.14f);

    // face anchor
    Node_Index face = scene.new_node("face", skin_pair, head);
    scene.nodes[face].visible = false;
    scene.nodes[face].propagate_visibility = false;
    scene.nodes[face].position = vec3(0.f, 0.5f, 0.f);
    scene.nodes[face].scale = vec3(1.f, 1.f, 1.f);

    // i give up

    // left eyebrow
    solid_material.albedo.mod = rgb_vec4(2, 1, 1);
    Node_Index left_eyebrow = scene.add_mesh(cube, "left_eyebrow", &solid_material);
    scene.set_parent(left_eyebrow, face);
    scene.nodes[left_eyebrow].position = vec3(-0.2f, 0.f, 0.2f);
    scene.nodes[left_eyebrow].scale = vec3(0.3f, 0.01f, 0.05f);

    // left eye
    solid_material.roughness.mod = vec4(0.1);
    solid_material.albedo.mod = vec4(1.f);
    Node_Index left_eye = scene.add_mesh(cube, "left_eye", &solid_material);
    scene.set_parent(left_eye, face);
    scene.nodes[left_eye].position = vec3(-0.2f, 0.f, 0.1f);
    scene.nodes[left_eye].scale = vec3(0.2f, 0.01f, 0.1f);

    // left iris
    solid_material.albedo.mod = rgb_vec4(51, 122, 44);
    Node_Index left_iris = scene.add_mesh(cube, "left_iris", &solid_material);
    scene.set_parent(left_iris, left_eye);
    scene.nodes[left_iris].position = vec3(0.f, 0.5f, 0.f);
    scene.nodes[left_iris].scale = vec3(0.5f, 0.1f, 1.f);

    // left pupil
    solid_material.roughness.mod = vec4(1);
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index left_pupil = scene.add_mesh(cube, "left_pupil", &solid_material);
    scene.set_parent(left_pupil, left_iris);
    scene.nodes[left_pupil].position = vec3(0.f);
    scene.nodes[left_pupil].scale = vec3(0.5f, 1.5f, 0.5f);

    // right eyebrow
    solid_material.albedo.mod = rgb_vec4(2, 1, 1);
    Node_Index right_eyebrow = scene.add_mesh(cube, "right_eyebrow", &solid_material);
    scene.set_parent(right_eyebrow, face);
    scene.nodes[right_eyebrow].position = vec3(0.2f, 0.f, 0.2f);
    scene.nodes[right_eyebrow].scale = vec3(0.3f, 0.01f, 0.05f);

    // right eye
    solid_material.roughness.mod = vec4(0.1);
    solid_material.albedo.mod = vec4(1.f);
    Node_Index right_eye = scene.add_mesh(cube, "right_eye", &solid_material);
    scene.set_parent(right_eye, face);
    scene.nodes[right_eye].position = vec3(0.2f, 0.f, 0.1f);
    scene.nodes[right_eye].scale = vec3(0.2f, 0.01f, 0.1f);

    // right iris
    solid_material.albedo.mod = rgb_vec4(51, 122, 44);
    Node_Index right_iris = scene.add_mesh(cube, "right_iris", &solid_material);
    scene.set_parent(right_iris, right_eye);
    scene.nodes[right_iris].position = vec3(0.f, 0.5f, 0.f);
    scene.nodes[right_iris].scale = vec3(0.5f, 0.1f, 1.f);

    // right pupil
    solid_material.roughness.mod = vec4(1);
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index right_pupil = scene.add_mesh(cube, "right_pupil", &solid_material);
    scene.set_parent(right_pupil, right_iris);
    scene.nodes[right_pupil].position = vec3(0.f);
    scene.nodes[right_pupil].scale = vec3(0.5f, 1.5f, 0.5f);

    // hair
    Node_Index hair = scene.add_mesh(cube, "hair", &hair_material);
    scene.set_parent(hair, head);
    scene.nodes[hair].position = vec3(0.f, -0.19f, 0.07f);
    scene.nodes[hair].scale = vec3(1.3f, 1.f, 1.33f);

    // fringe
    Node_Index fringe = scene.new_node("fringe", hair_pair, head);
    scene.nodes[fringe].position = vec3(0.f, 0.32f, 0.6f);
    scene.nodes[fringe].scale = vec3(1.1f, 0.44f, 0.35f);

    // lips
    solid_material.roughness.mod = vec4(.3);
    solid_material.albedo.mod = rgb_vec4(200, 80, 80);
    Node_Index lips = scene.add_mesh(cube, "lips", &solid_material);
    scene.set_parent(lips, head);
    scene.nodes[lips].position = vec3(0.f, 0.5f, -0.25f);
    scene.nodes[lips].scale = vec3(0.3f, 0.05f, 0.1f);
  }
  return character_node;
}

Node_Index add_boi_character_mesh(Scene_Graph &scene)
{
  Material_Descriptor skin_material;
  skin_material.albedo.mod = rgb_vec4(253, 228, 200);
  skin_material.metalness.mod = vec4(0);
  skin_material.roughness.mod = vec4(0.5);

  Material_Descriptor suit_material;
  suit_material.albedo.mod = vec4(0, 0, 0, 1);
  suit_material.roughness.mod = vec4(1);
  suit_material.roughness.source = "white";
  suit_material.metalness.mod = vec4(0);
  suit_material.normal = "cloth_normal.jpg";
  suit_material.uv_scale = vec2(0.25f);

  Material_Descriptor hair_material;
  hair_material.albedo.mod = 0.75f * rgb_vec4(63, 26, 3);
  hair_material.roughness.mod = vec4(0.82);
  hair_material.roughness.source = "white";
  hair_material.metalness.mod = vec4(0);
  hair_material.normal = "hair_normal.jpg";
  hair_material.uv_scale = vec2(1.f);

  Material_Descriptor shoe_material;
  shoe_material.albedo.mod = vec4(0, 0, 0, 1);
  shoe_material.roughness.mod = vec4(0.11);
  shoe_material.metalness.mod = vec4(0);

  Material_Descriptor shirt_material = suit_material;
  shirt_material.albedo.mod = vec4(1);
  shirt_material.uv_scale = vec2(1.);
  Material_Descriptor solid_material = shirt_material;

  Mesh_Descriptor md(cube, "boi's cube");
  Mesh_Index cube_mesh_index = scene.resource_manager->push_mesh(&md);
  Material_Index skin_material_index = scene.resource_manager->push_material(&skin_material);
  Material_Index suit_material_index = scene.resource_manager->push_material(&suit_material);
  Material_Index shirt_material_index = scene.resource_manager->push_material(&shirt_material);
  Material_Index shoe_material_index = scene.resource_manager->push_material(&shoe_material);
  Material_Index hair_material_index = scene.resource_manager->push_material(&hair_material);

  std::pair<Mesh_Index, Material_Index> skin_pair = {cube_mesh_index, skin_material_index};
  std::pair<Mesh_Index, Material_Index> suit_pair = {cube_mesh_index, suit_material_index};
  std::pair<Mesh_Index, Material_Index> shirt_pair = {cube_mesh_index, shirt_material_index};
  std::pair<Mesh_Index, Material_Index> shoe_pair = {cube_mesh_index, shoe_material_index};
  std::pair<Mesh_Index, Material_Index> hair_pair = {cube_mesh_index, hair_material_index};

  Node_Index character_node = scene.new_node("cubeboi", skin_pair);
  {
    // shirt

    Node_Index shirt = scene.new_node("shirt", shirt_pair, character_node);
    scene.nodes[shirt].position = vec3(0.f, 0.0f, 0.15f);
    scene.nodes[shirt].scale = vec3(1.05f, 1.05f, 0.3f);

    // tie
    Node_Index tie = scene.new_node("tie", suit_pair, shirt);
    scene.nodes[tie].position = vec3(0.f, 0.f, 0.08f);
    scene.nodes[tie].scale = vec3(0.2f, 1.1f, .85f);

    // jacket left

    Node_Index jacket_left = scene.new_node("jacket_left", suit_pair, shirt);
    scene.nodes[jacket_left].position = vec3(0.425f, 0.0f, -0.1f);
    scene.nodes[jacket_left].scale = vec3(0.3f, 1.1f, 1.2f);

    // jacket right
    Node_Index jacket_right = scene.new_node("jacket_right", suit_pair, shirt);
    scene.nodes[jacket_right].position = vec3(-0.425f, 0.0f, -0.1f);
    scene.nodes[jacket_right].scale = vec3(0.3f, 1.1f, 1.2f);

    // jacket back
    Node_Index jacket_back = scene.new_node("jacket_back", suit_pair, shirt);
    scene.nodes[jacket_back].position = vec3(0.f, -0.5f, -0.1f);
    scene.nodes[jacket_back].scale = vec3(1.f, .2f, 1.2f);

    // belt
    Node_Index belt = scene.new_node("belt", hair_pair, character_node);
    scene.nodes[belt].position = vec3(0.f, 0.03f, 0.0f);
    scene.nodes[belt].scale = vec3(1.02f, 1.02f, 0.025f);

    // belt buckle
    Node_Index belt_buckle = scene.new_node("belt_buckle", shoe_pair, belt);
    scene.nodes[belt_buckle].position = vec3(0.f, 0.f, 0.0f);
    scene.nodes[belt_buckle].scale = vec3(0.1f, 1.02f, 1.f);

    // pants
    Node_Index pants = scene.new_node("pants", suit_pair, character_node);
    scene.nodes[pants].position = vec3(0.f, 0.f, -0.25f);
    scene.nodes[pants].scale = vec3(1.05f, 1.05f, 0.5f);

    // left shoe
    Node_Index left_shoe = scene.new_node("left_shoe", shoe_pair, character_node);
    scene.nodes[left_shoe].position = vec3(0.25f, 0.75f, -0.47f);
    scene.nodes[left_shoe].scale = vec3(.4f, 1.5f, 0.06f);

    // right shoe
    Node_Index right_shoe = scene.new_node("right_shoe", shoe_pair, character_node);
    scene.nodes[right_shoe].position = vec3(-0.25f, 0.75f, -0.47f);
    scene.nodes[right_shoe].scale = vec3(.4f, 1.5f, 0.06f);

    // right shoulder_joint
    Node_Index right_shoulder_joint = scene.new_node("right_shoulder_joint", suit_pair, character_node);
    scene.nodes[right_shoulder_joint].visible = false;
    scene.nodes[right_shoulder_joint].propagate_visibility = false;
    scene.nodes[right_shoulder_joint].position = vec3(0.8f, 0.f, 0.3f);

    // right arm
    Node_Index right_arm = scene.new_node("right_arm", skin_pair, right_shoulder_joint);
    scene.nodes[right_arm].position = vec3(0.f, 0.f, -0.25f);
    scene.nodes[right_arm].scale = vec3(0.4f, 0.4f, 0.5f);

    // right sleeve
    Node_Index right_sleeve = scene.new_node("right_sleeve", suit_pair, right_arm);
    scene.nodes[right_sleeve].position = vec3(0.f, 0.f, 0.35f);
    scene.nodes[right_sleeve].scale = vec3(1.15f, 1.15f, 0.35f);

    // left shoulder_joint
    Node_Index left_shoulder_joint = scene.new_node("left_shoulder_joint", suit_pair, character_node);
    scene.set_parent(left_shoulder_joint, character_node);
    scene.nodes[left_shoulder_joint].visible = false;
    scene.nodes[left_shoulder_joint].propagate_visibility = false;
    scene.nodes[left_shoulder_joint].position = vec3(-0.8f, 0.f, 0.3f);

    // left arm
    Node_Index left_arm = scene.new_node("left_arm", skin_pair, left_shoulder_joint);
    scene.nodes[left_arm].position = vec3(0.f, 0.f, -0.25f);
    scene.nodes[left_arm].scale = vec3(0.4f, 0.4f, 0.5f);

    // left sleeve
    Node_Index left_sleeve = scene.new_node("left_sleeve", suit_pair, left_arm);
    scene.nodes[left_sleeve].position = vec3(0.f, 0.f, 0.1f);
    scene.nodes[left_sleeve].scale = vec3(1.5f, 1.5f, 0.85f);

    // head
    Node_Index head = scene.new_node("head", skin_pair, character_node);
    scene.nodes[head].position = vec3(0.f, 0.0f, 0.4f);
    scene.nodes[head].scale = vec3(1.1f, 1.1f, 0.2f);

    // face anchor
    Node_Index face = scene.new_node("face", skin_pair, head);
    scene.nodes[face].visible = false;
    scene.nodes[face].propagate_visibility = false;
    scene.nodes[face].position = vec3(0.f, 0.5f, 0.f);
    scene.nodes[face].scale = vec3(1.f, 1.f, 1.f);

    // i give up

    // left eyebrow
    solid_material.albedo.mod = rgb_vec4(2, 1, 1);
    Node_Index left_eyebrow = scene.add_mesh(cube, "left_eyebrow", &solid_material);
    scene.set_parent(left_eyebrow, face);
    scene.nodes[left_eyebrow].position = vec3(-0.2f, 0.f, 0.2f);
    scene.nodes[left_eyebrow].scale = vec3(0.3f, 0.01f, 0.05f);

    // left eye
    solid_material.roughness.mod = vec4(0.1);
    solid_material.albedo.mod = vec4(1.f);
    Node_Index left_eye = scene.add_mesh(cube, "left_eye", &solid_material);
    scene.set_parent(left_eye, face);
    scene.nodes[left_eye].position = vec3(-0.2f, 0.f, 0.1f);
    scene.nodes[left_eye].scale = vec3(0.2f, 0.01f, 0.1f);

    // left iris
    solid_material.albedo.mod = rgb_vec4(51, 122, 44);
    Node_Index left_iris = scene.add_mesh(cube, "left_iris", &solid_material);
    scene.set_parent(left_iris, left_eye);
    scene.nodes[left_iris].position = vec3(0.f, 0.5f, 0.f);
    scene.nodes[left_iris].scale = vec3(0.5f, 0.1f, 1.f);

    // left pupil
    solid_material.roughness.mod = vec4(1);
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index left_pupil = scene.add_mesh(cube, "left_pupil", &solid_material);
    scene.set_parent(left_pupil, left_iris);
    scene.nodes[left_pupil].position = vec3(0.f);
    scene.nodes[left_pupil].scale = vec3(0.5f, 1.5f, 0.5f);

    // right eyebrow
    solid_material.albedo.mod = rgb_vec4(2, 1, 1);
    Node_Index right_eyebrow = scene.add_mesh(cube, "right_eyebrow", &solid_material);
    scene.set_parent(right_eyebrow, face);
    scene.nodes[right_eyebrow].position = vec3(0.2f, 0.f, 0.2f);
    scene.nodes[right_eyebrow].scale = vec3(0.3f, 0.01f, 0.05f);

    // right eye
    solid_material.roughness.mod = vec4(0.1);
    solid_material.albedo.mod = vec4(1.f);
    Node_Index right_eye = scene.add_mesh(cube, "right_eye", &solid_material);
    scene.set_parent(right_eye, face);
    scene.nodes[right_eye].position = vec3(0.2f, 0.f, 0.1f);
    scene.nodes[right_eye].scale = vec3(0.2f, 0.01f, 0.1f);

    // right iris
    solid_material.albedo.mod = rgb_vec4(51, 122, 44);
    Node_Index right_iris = scene.add_mesh(cube, "right_iris", &solid_material);
    scene.set_parent(right_iris, right_eye);
    scene.nodes[right_iris].position = vec3(0.f, 0.5f, 0.f);
    scene.nodes[right_iris].scale = vec3(0.5f, 0.1f, 1.f);

    // right pupil
    solid_material.roughness.mod = vec4(1);
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index right_pupil = scene.add_mesh(cube, "right_pupil", &solid_material);
    scene.set_parent(right_pupil, right_iris);
    scene.nodes[right_pupil].position = vec3(0.f);
    scene.nodes[right_pupil].scale = vec3(0.5f, 1.5f, 0.5f);

    // hair
    Node_Index hair = scene.add_mesh(cube, "hair", &hair_material);
    scene.set_parent(hair, head);
    scene.nodes[hair].position = vec3(0.f, -0.1f, 0.1f);
    scene.nodes[hair].scale = vec3(1.2f, 1.f, 1.2f);

    // fringe
    Node_Index fringe = scene.new_node("fringe", hair_pair, head);
    scene.nodes[fringe].position = vec3(0.f, 0.5f, 0.5f);
    scene.nodes[fringe].scale = vec3(1.1f, 0.2f, 0.4f);

    // lips
    solid_material.roughness.mod = vec4(.3);
    solid_material.albedo.mod = rgb_vec4(200, 80, 80);
    Node_Index lips = scene.add_mesh(cube, "lips", &solid_material);
    scene.set_parent(lips, head);
    scene.nodes[lips].position = vec3(0.f, 0.5f, -0.25f);
    scene.nodes[lips].scale = vec3(0.3f, 0.05f, 0.1f);
  }
  return character_node;
}

Node_Index Warg_State::add_character_mesh(UID character_id)
{
  Node_Index spawned = NODE_NULL;
  const auto &character = find_if(current_game_state.characters, [&](auto &c) { return c.id == character_id; });
  if (character != current_game_state.characters.end() && character->name == "Veux")
  {
    spawned = add_girl_character_mesh(scene);
  }
  else
  {
    spawned = add_boi_character_mesh(scene);
  }
  Node_Index result = scene.new_node();
  scene.set_parent(spawned, result);
  character_nodes[character_id] = result;
  return result;
}

void create_cast_bar(const char *name, float32 progress, ImVec2 position, ImVec2 size)
{
  Layout_Grid grid(vec2(size.x, size.y), vec2(2), vec2(1), 1, 1);

  bool display_castbar = true;
  ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10, 10));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin(name, &display_castbar,
      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing);

  ImGui::SetCursorPos(v(grid.get_position(0, 0)));
  ImGui::ProgressBar(progress, v(grid.get_section_size(1, 1)), "");

  ImGui::End();
  ImGui::PopStyleVar(3);
}

void Warg_State::update_cast_bar()
{
  auto player_character = find_if(current_game_state.characters, [&](auto &c) { return c.id == player_character_id; });
  if (player_character == current_game_state.characters.end())
    return;

  auto cast = std::find_if(current_game_state.character_casts.begin(), current_game_state.character_casts.end(),
      [&](auto &c) { return c.caster == player_character->id; });
  if (cast == current_game_state.character_casts.end())
    return;

  const vec2 resolution = CONFIG.resolution;

  ImVec2 size = ImVec2(200, 32);
  ImVec2 position = ImVec2(resolution.x / 2 - size.x / 2, CONFIG.resolution.y * 0.7f);
  float32 progress = cast->progress / SPELL_DB.cast_time[cast->spell];

  create_cast_bar("player_cast_bar", progress, position, size);
}

void Warg_State::update_unit_frames()
{
  if (none_of(current_game_state.living_characters, [&](auto &c) { return c.id == player_character_id; }))
    return;

  auto make_unit_frame = [&](const char *name, UID character_id, ImVec2 size, ImVec2 position) {
    Layout_Grid grid(vec2(size.x, size.y), vec2(2), vec2(1), 1, 4);

    bool display_unit_frame = true;
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::Begin(name, &display_unit_frame,
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing);

    auto character = find_if(current_game_state.characters, [&](auto &c) { return c.id == character_id; });

    ImGui::SetCursorPos(v(grid.get_position(0, 0)));
    ImGui::Text("%s", character->name.c_str());

    const auto &lc = find_if(current_game_state.living_characters, [&](auto &lc) { return lc.id == character->id; });

    ImGui::SetCursorPos(v(grid.get_position(0, 1)));
    float32 hp_percentage = (float32)lc->hp / (float32)lc->hp_max;
    ImVec4 hp_color = ImVec4((1.f - hp_percentage) * 0.75f, hp_percentage * 0.75f, 0.f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hp_color);
    ImGui::ProgressBar(hp_percentage, v(grid.get_section_size(1, 2)), "");
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(v(grid.get_position(0, 3)));
    float32 mana_percentage = (float32)lc->mana / (float32)lc->mana_max;
    vec4 mana_color = rgb_vec4(64, 112, 255);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(mana_color.x, mana_color.y, mana_color.z, mana_color.w));
    ImGui::ProgressBar(mana_percentage, v(grid.get_section_size(1, 1)), "");
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar(3);
  };

  auto make_target_buffs = [&](std::vector<Character_Buff> cbs, vec2 position, vec2 size, bool debuffs) {
    int buff_count = cbs.size();

    Layout_Grid outer_grid(size, vec2(0), vec2(2), vec2(buff_count, 1), vec2(1, 1), 1);

    int i = 0;
    for (auto &cb : cbs)
    {
      Layout_Grid inner_grid(outer_grid.get_section_size(1, 1), vec2(2), vec2(0), 1, 1);

      bool display_buff = true;
      ImGui::SetNextWindowPos(v(position + outer_grid.get_position(i, 0)));
      ImGui::SetNextWindowSize(v(inner_grid.get_total_size()));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(3, 3));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
      ImGui::Begin(s("target ", debuffs ? "de" : "", "buff", i).c_str(), &display_buff,
          ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoFocusOnAppearing);

      ImGui::SetCursorPos(v(inner_grid.get_position(0, 0)));
      auto &icon = SPELL_DB.buff_icon[cb.buff_id];
      put_imgui_texture(&icon, inner_grid.get_section_size(1, 1));
      ImGui::End();
      ImGui::PopStyleVar(4);
      i++;
    }
  };

  Layout_Grid grid(vec2(400, 160), vec2(10), vec2(5), 2, 6);

  make_unit_frame("player_unit_frame", player_character_id, v(grid.get_section_size(1, 3)), v(grid.get_position(0, 0)));

  if (none_of(current_game_state.living_characters, [&](auto &lc) { return lc.id == target_id; }))
    return;

  make_unit_frame("target_unit_frame", target_id, v(grid.get_section_size(1, 3)), v(grid.get_position(1, 0)));
  std::vector<Character_Buff> buffs, debuffs;
  for (auto &cb : current_game_state.character_buffs)
  {
    if (cb.character != target_id)
      continue;
    if (SPELL_DB.buff_is_debuff.contains(cb.buff_id))
      debuffs.push_back(cb);
    else
      buffs.push_back(cb);
  }
  make_target_buffs(buffs, grid.get_position(1, 3), grid.get_section_size(1, 1), false);
  make_target_buffs(debuffs, grid.get_position(1, 4), grid.get_section_size(1, 1), true);

  auto cast = std::find_if(current_game_state.character_casts.begin(), current_game_state.character_casts.end(),
      [&](auto &c) { return c.caster == target_id; });
  if (cast == current_game_state.character_casts.end())
    return;

  float32 cast_progress = cast->progress / SPELL_DB.cast_time[cast->spell];
  create_cast_bar("target_cast_bar", cast_progress, v(grid.get_position(1, 5)), v(grid.get_section_size(1, 1)));
}

void Warg_State::update_icons()
{
  ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID);
  auto player_character = find_if(current_game_state.characters, [&](auto &c) { return c.id == player_character_id; });
  ASSERT(player_character != current_game_state.characters.end());

  Framebuffer *framebuffer = &interface_state.duration_spiral_fbo;
  Shader *shader = &interface_state.duration_spiral_shader;

  if (!interface_state.icon_setup_complete)
  {
    if (shader->fs == "")
      *shader = Shader("passthrough.vert", "duration_spiral.frag");

    if (!interface_state.icon_setup_complete)
    {
      size_t spell_index = 0;
      for (size_t i = 0; i < current_game_state.character_spells.size(); ++i)
      {
        Character_Spell &cs = current_game_state.character_spells[i];
        if (cs.character != player_character_id)
          continue;

        if (interface_state.action_bar_textures_sources.size() < spell_index + 1)
          interface_state.action_bar_textures_sources.emplace_back(SPELL_DB.icon[cs.spell]);

        Texture *icon = &interface_state.action_bar_textures_sources[spell_index];

        spell_index += 1;
        if (!icon->bind_for_sampling_at(0))
        {
          return;
        }
      }
    }

    for (size_t i = 0; i < interface_state.action_bar_textures_sources.size(); ++i)
    {
      Texture *icon = &interface_state.action_bar_textures_sources[i];
      if (!icon->bind_for_sampling_at(0))
        return;
    }

    Texture_Descriptor duration_spiral_descriptor;
    duration_spiral_descriptor.source = "generate";
    duration_spiral_descriptor.minification_filter = GL_LINEAR;
    duration_spiral_descriptor.size = vec2(90, 90);
    duration_spiral_descriptor.format = GL_SRGB8_ALPHA8;
    duration_spiral_descriptor.levels = 1;
    framebuffer->color_attachments.resize(interface_state.action_bar_textures_sources.size());
    for (size_t i = 0; i < framebuffer->color_attachments.size(); ++i)
    {
      duration_spiral_descriptor.name = s("duration-spiral-", i);
      framebuffer->color_attachments[i] = duration_spiral_descriptor;
      while (!framebuffer->color_attachments[i].bind_for_sampling_at(0))
      {
      }
    }
    interface_state.action_bar_textures_with_cooldown = framebuffer->color_attachments;
    interface_state.icon_setup_complete = true;
  }

  framebuffer->init();
  framebuffer->bind_for_drawing_dst();

  shader->use();
  shader->set_uniform("count", (uint32)framebuffer->color_attachments.size());

  int i = 0;
  for (auto &cs : current_game_state.character_spells)
  {
    if (cs.character != player_character_id)
      continue;

    float32 cooldown_percent = 0.f;
    float32 cooldown_remaining = 0.f;

    auto scd = find_if(current_game_state.spell_cooldowns,
        [&](auto &scd) { return scd.character == player_character_id && scd.spell == cs.spell; });
    if (scd != current_game_state.spell_cooldowns.end())
      cooldown_percent = scd->cooldown_remaining / SPELL_DB.cooldown[cs.spell];
    if (SPELL_DB.on_gcd.contains(cs.spell))
    {
      const auto &cg =
          find_if(current_game_state.character_gcds, [&](auto &cg) { return cg.character == player_character_id; });
      const auto &lc =
          find_if(current_game_state.living_characters, [&](auto &lc) { return lc.id == player_character_id; });
      if (cg != current_game_state.character_gcds.end() &&
          (scd == current_game_state.spell_cooldowns.end() ||
              cg->remaining / lc->effective_stats.cast_speed > scd->cooldown_remaining))
        cooldown_percent = cg->remaining / lc->effective_stats.global_cooldown;
    }

    std::string uniform_str = s("progress", i);

    // std::string msg_id = s("setting spiral i:", i);
    // std::string msg = s("uniform slot: ", uniform_str, "value:", cooldown_percent);
    // set_message(msg_id, msg, 1.0f);

    shader->set_uniform(uniform_str.c_str(), cooldown_percent);
    i++;
  }

  run_pixel_shader(shader, &interface_state.action_bar_textures_sources, framebuffer);
}

void Warg_State::update_action_bar()
{

  // todo: fix warg icons
  update_icons();

  const vec2 resolution = CONFIG.resolution;
  const size_t number_abilities = interface_state.action_bar_textures_with_cooldown.size();
  Layout_Grid grid(vec2(900, 90), vec2(2), vec2(1), vec2(number_abilities, 1), vec2(1, 1), 1);

  bool display_action_bar = true;
  ImGui::SetNextWindowPos(
      ImVec2(((resolution - grid.get_total_size()) / vec2(2)).x, resolution.y - grid.get_total_size().y - 10));
  ImGui::SetNextWindowSize(v(grid.get_total_size()));
  ImGui::SetNextWindowBgAlpha(1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
  ImGui::Begin("action_bar", &display_action_bar,
      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing);
  for (size_t i = 0; i < interface_state.action_bar_textures_with_cooldown.size(); i++)
  {
    ImGui::SetCursorPos(v(grid.get_position(i, 0)));
    put_imgui_texture(&interface_state.action_bar_textures_with_cooldown[i], grid.get_section_size(1, 1));
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
}

void Warg_State::update_buff_indicators()
{
  auto player_character = find_if(current_game_state.characters, [&](auto &c) { return c.id == player_character_id; });
  ASSERT(player_character != current_game_state.characters.end());

  auto duration_string = [](float32 seconds) {
    int32 seconds_int = ceil(seconds);

    if (seconds_int > 3600)
      return s(seconds_int / 3600, "h");
    else if (seconds_int > 60)
      return s(seconds_int / 60, "m");
    else
      return s(seconds_int, "s");
  };

  const vec2 resolution = CONFIG.resolution;

  auto create_indicator = [&](vec2 position, vec2 size, Texture *icon, float64 duration, bool debuff, size_t i) {
    Layout_Grid grid(size, vec2(2), vec2(1), vec2(1, 3), vec2(1, 2), 1);

    std::string countdown_string = duration_string(duration);
    float32 alpha = 1.f;
    if (duration < 5.f)
      alpha = 0.3f + ((sin(duration * pi<float32>()) + 1) / 2.f) * 0.7f;

    bool display_buffs = true;

    ImGui::SetNextWindowPos(v(position));
    ImGui::SetNextWindowSize(v(grid.get_total_size()));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::Begin(s(debuff ? "de" : "", "buff", i).c_str(), &display_buffs,
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::SetCursorPos(v(grid.get_position(0, 0)));
    put_imgui_texture(icon, grid.get_section_size(1, 2));
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(v(grid.get_position(0, 2)));
    ImGui::Text("%s", countdown_string.c_str());

    ImGui::End();
    ImGui::PopStyleVar(3);
  };

  size_t max_columns = 18;
  Layout_Grid grid(vec2(800, 300), vec2(10), vec2(5), max_columns, 3);

  std::vector<Character_Buff> buffs, debuffs;
  for (auto &cb : current_game_state.character_buffs)
  {
    if (cb.character != player_character_id)
      continue;
    if (SPELL_DB.buff_is_debuff.contains(cb.buff_id))
      debuffs.push_back(cb);
    else
      buffs.push_back(cb);
  }

  int col = 0;
  for (auto &cb : buffs)
  {
    if (col >= max_columns)
      break;
    float32 position_x = resolution.x - grid.get_position(col, 0).x - grid.get_section_size(1, 1).x;
    create_indicator(vec2(position_x, grid.get_position(col, 0).y), grid.get_section_size(1, 1),
        &SPELL_DB.buff_icon[cb.buff_id], cb.duration, false, col);
    col++;
  }

  col = 0;
  for (auto &cd : debuffs)
  {
    if (col >= max_columns)
      break;
    float32 position_x = resolution.x - grid.get_position(col, 0).x - grid.get_section_size(1, 1).x;
    create_indicator(vec2(position_x, grid.get_position(col, 2).y), grid.get_section_size(1, 1),
        &SPELL_DB.buff_icon[cd.buff_id], cd.duration, false, col);
    col++;
  }
}

void Warg_State::update_game_interface()
{
  if (none_of(current_game_state.living_characters, [&](auto &c) { return c.id == player_character_id; }))
    return;

  const vec2 resolution = CONFIG.resolution;

  update_cast_bar();
  update_unit_frames();
  update_action_bar();
  update_buff_indicators();
  update_stats_bar();
}
