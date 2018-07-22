#include "stdafx.h"
#include "Warg_State.h"
#include "Globals.h"
#include "Render.h"
#include "State.h"
#include "Third_party/imgui/imgui.h"
#include "UI.h"

using namespace glm;

Warg_State::Warg_State(std::string name, SDL_Window *window, ivec2 window_size, Session *session_)
    : State(name, window, window_size)
{ 
  session = session_;

  map = make_blades_edge();
  sdb = make_spell_db();
  map.node = scene.add_aiscene("Blades Edge", "Blades_Edge/blades_edge.fbx", &map.material);
  collider_cache = collect_colliders(scene);
  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();

  session->push(make_unique<Char_Spawn_Request_Message>("Cubeboi", 0));
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
      if (SDLK_1 <= _e.key.keysym.sym && _e.key.keysym.sym <= SDLK_9 && !free_cam && get_character(&game_state, pc))
      {
        size_t key = _e.key.keysym.sym - SDLK_1;
        if (key < sdb->spells.size())
        {
          session->push(std::make_unique<Cast_Message>(target_id, sdb->spells[key].name.c_str()));
        }
      }
      if (_e.key.keysym.sym == SDLK_TAB && !free_cam && get_character(&game_state, pc))
      {
        for (size_t i = 0; i < game_state.character_count; i++)
        {
          Character *character = &game_state.characters[i];
          if (character->id != pc && character->alive)
            target_id = character->id;
        }
      }
    }
    else if (_e.type == SDL_KEYUP)
    {
      ASSERT(!block_kb);
        if (_e.key.keysym.sym == SDLK_F5)
        {
          free_cam = !free_cam;
          SDL_SetRelativeMouseMode(SDL_bool(free_cam));
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

  if (!pc || !get_character(&game_state, pc))
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
    character_to_camera = rx * normalize(vec4(0, -1, 0.0, 0));

    // perp is the camera-relative 'x' axis that moves with the camera
    // as the camera rotates around the Character-centered y axis
    // should always point towards the right of the screen
    vec3 perp = vec3(-character_to_camera.y, character_to_camera.x, 0);

    // construct a matrix that represents a rotation around perp by -phi
    mat4 ry = rotate(-cam.phi, perp);

    // rotate the camera vector around perp
    character_to_camera = normalize(ry * character_to_camera);

    ASSERT(pc);
    Character *player_character = get_character(&game_state, pc);
    ASSERT(player_character);

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

    register_move_command((Move_Status)m, orientation);
  }
  previous_mouse_state = mouse_state;
}

void Warg_State::register_move_command(Move_Status m, quat orientation)
{
  session->push(make_unique<Input_Message>(input_number, m, orientation, target_id));
  Input cmd;
  cmd.number = input_number;
  cmd.m = m;
  cmd.orientation = orientation;
  input_buffer.push(cmd);
  input_number++;
}

void Warg_State::process_messages()
{
  for (unique_ptr<Message> &message : session->pull())
    message->handle(*this);
}

void Warg_State::set_camera_geometry()
{
  // set_message(s("set_camera_geometry(), time:", current_time), "", 1.0f);
  Character *player_character = get_character(&game_state, pc);
  if (!player_character)
    return;

  float effective_zoom = cam.zoom;
  for (Triangle &surface : collider_cache)
  {
    vec3 intersection_point;
    bool intersects =
        ray_intersects_triangle(player_character->physics.position, character_to_camera, surface, &intersection_point);
    if (intersects && length(player_character->physics.position - intersection_point) < effective_zoom)
    {
      effective_zoom = length(player_character->physics.position - intersection_point);
    }
  }
  cam.pos = player_character->physics.position +
            vec3(character_to_camera.x, character_to_camera.y, character_to_camera.z) * (effective_zoom * 0.98f);
  cam.dir = -vec3(character_to_camera);
}

void Warg_State::update_hp_bar(UID character_id)
{
  // set_message("update_hp_bar()");
  Character *character = get_character(&game_state, character_id);
  Node_Index character_node = character_nodes[character_id];

  Node_Index hp_bar = scene.find_child_by_name(character_node, "hp_bar");

  if (!character->alive)
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

  static float32 debug_hp_percent = 1.f;
  ImGui::Begin("HP DEBUG WINDOW");
  ImGui::SliderFloat("hp slider", &debug_hp_percent, 0.0f, 1.f);
  ImGui::End();

  float32 hp_percent = ((float32)character->hp) / ((float32)character->hp_max);

  Material_Index material_index = scene.nodes[hp_bar].model[0].second;
  Material_Descriptor *mat = scene.resource_manager->retrieve_pool_material_descriptor(material_index);
  mat->emissive.mod = vec4(1.f - hp_percent, hp_percent, 0.f, 1.f);
  scene.resource_manager->update_material_pool_index(material_index);
}

void Warg_State::update_character_nodes()
{
  for (size_t i = 0; i < game_state.character_count; i++)
  {
    Character *character = &game_state.characters[i];

    Node_Index character_node = character_nodes[character->id];

    scene.nodes[character_node].velocity = (character->physics.position - scene.nodes[character_node].position) / dt;
    scene.nodes[character_node].position = character->physics.position;
    scene.nodes[character_node].orientation = character->physics.orientation;

    scene.nodes[character_node].scale = character->radius * vec3(2);

    update_hp_bar(character->id);

    animate_character(character->id);
  }
}

void Warg_State::update_prediction_ghost()
{
  Character *player_character = get_character(&game_state, pc);
  if (!player_character)
    return;

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
  ImGui::Begin("stats_bar", &show_stats_bar, ImVec2(10, 10), 0.5f,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
          ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Ping: %dms", latency_tracker.get_latency());
  ImGui::End();
}

bool prediction_correct(UID player_character_id, Game_State &server_state, Game_State &predicted_state)
{
  bool same_input = server_state.input_number == predicted_state.input_number;

  Character *server_character = get_character(&server_state, player_character_id);
  Character *predicted_character = get_character(&predicted_state, player_character_id);

  if (!server_character && !predicted_character)
    return same_input;

  if (!server_character != !predicted_character)
    return false;

  ASSERT(server_character);
  ASSERT(predicted_character);

  bool same_player_physics = server_character->physics == predicted_character->physics;

  return same_input && same_player_physics;
}

void Warg_State::predict_state()
{
  game_state = server_state;

  if (!get_character(&server_state, pc))
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

    Character *player_character = get_character(&predicted_state, pc);
    Character_Physics &physics = player_character->physics;
    vec3 radius = player_character->radius;
    float32 movement_speed = player_character->e_stats.speed;

    move_char(*player_character, input, collider_cache);
    if (vec3_has_nan(physics.position))
      physics.position = map.spawn_pos[player_character->team];

    predicted_state.input_number = input.number;
    state_buffer.push_back(predicted_state);

    prediction_iterations++;
  }
  set_message("prediction iterations:", s(prediction_iterations), 5);

  game_state = state_buffer.back();
}

void Warg_State::update_meshes()
{
  for (size_t i = 0; i < game_state.character_count; i++)
  {
    Character *character = &game_state.characters[i];
    if (!character_nodes.count(character->id))
    {
      add_character_mesh(character->id);
    }
  }
}

void Warg_State::update_spell_object_nodes()
{
  Material_Descriptor material;
  material.albedo.mod = vec4(0.f);
  material.emissive = "color(1, 1, 1, 1)";
  material.emissive.mod = vec4(0.5f, 0.5f, 100.f, 1.f);

  for (size_t i = 0; i < game_state.spell_object_count; i++)
  {
    Spell_Object *spell_object = &game_state.spell_objects[i];
    if (spell_object_nodes.count(spell_object->id) == 0)
    {
      spell_object_nodes[spell_object->id] = scene.add_mesh(cube, "spell_object_cube", &material);
      set_message(s("adding spell object node on tick ", game_state.tick), "", 20);
    }
    Node_Index mesh = spell_object_nodes[spell_object->id];
    scene.nodes[mesh].scale = vec3(0.4f);
    scene.nodes[mesh].position = spell_object->pos;
  }

  std::vector<UID> to_erase;
  for (auto &node : spell_object_nodes)
  {
    bool orphaned = true;
    for (size_t i = 0; i < game_state.spell_object_count; i++)
    {
      Spell_Object *spell_object = &game_state.spell_objects[i];
      if (node.first == spell_object->id)
        orphaned = false;
    }
    if (orphaned)
      to_erase.push_back(node.first);
  }
  for (UID node_id : to_erase)
  {
    spell_object_nodes.erase(node_id);
    set_message(s("erasing spell object node on tick ", game_state.tick), "", 20);
  }
}

void Warg_State::animate_character(UID character_id)
{
  set_message("animate_character uid:", s(character_id), 1.0f);
  static std::map<UID, float32> animation_times;
  static std::map<UID, vec3> last_positions;
  static std::map<UID, bool> last_grounded;

  Character *character = get_character(&game_state, character_id);
  ASSERT(character);

  if (!character->alive)
    return;

  ASSERT(character_nodes.count(character_id));
  Node_Index character_node = character_nodes[character_id];
  ASSERT(character_node != NODE_NULL);

  set_message("character_node:", s(character_node), 1.0f);

  Node_Index left_shoe = scene.find_child_by_name(character_node, "left_shoe");
  Node_Index right_shoe = scene.find_child_by_name(character_node, "right_shoe");
  Node_Index left_shoulder_joint = scene.find_child_by_name(character_node, "left_shoulder_joint");
  Node_Index right_shoulder_joint = scene.find_child_by_name(character_node, "right_shoulder_joint");
  ASSERT(left_shoe != NODE_NULL);
  ASSERT(right_shoe != NODE_NULL);
  ASSERT(left_shoulder_joint != NODE_NULL);
  ASSERT(right_shoulder_joint != NODE_NULL);

  if (!last_positions.count(character_id))
    last_positions[character_id] = scene.nodes[character_node].position;
  vec3 last_position = last_positions[character_id];

  if (!animation_times.count(character_id))
    animation_times[character_id] = 0;
  float32 *animation_time = &animation_times[character_id];
  *animation_time += dt;

  if (scene.nodes[character_node].position == last_position || !character->physics.grounded)
  {
    scene.nodes[left_shoe].position.y = scene.nodes[right_shoe].position.y = 0.f;
    *animation_time = 0;
    return;
  }

  auto linear_oscillate = [](float32 m, float32 x, float32 b) { return 4 * abs(fract(m * x + b) - 0.5) - 1; };

  float32 cadence = character->e_stats.speed / STEP_SIZE;

  float32 m = cadence / 2;
  float32 x = *animation_time;
  float32 b = -cadence / 4;
  float32 current_progress = linear_oscillate(m, x, b);

  scene.nodes[left_shoe].position.y = (STEP_SIZE / 2) * current_progress / scene.nodes[character_node].scale.y;
  scene.nodes[right_shoe].position.y = -(STEP_SIZE / 2) * current_progress / scene.nodes[character_node].scale.y;

  // vec3 axis = vec3(1.f, 0.f, 0.f);
  // left_shoulder_joint->orientation = angleAxis(step, axis);
  // right_shoulder_joint->orientation = angleAxis(-step, axis);

  last_positions[character_id] = scene.nodes[character_node].position;
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
        vec3(0.f, 0.f, animation_object.physics.velocity.z), collider_cache);

    scene.nodes[animation_object.node].position = animation_object.physics.position;

    if (animation_object.physics.grounded)
      animation_object.physics.velocity = vec3(0);

    scene.nodes[animation_object.collision_node].position = animation_object.physics.position;
    scene.nodes[animation_object.collision_node].scale = animation_object.radius;
  }
}

void Warg_State::update()
{
  set_message("Warg update. Time:", s(current_time), 1.0f);
  set_message("Warg time/dt:", s(current_time / dt), 1.0f);

  process_messages();
  update_stats_bar();
  game_state = server_state;
  Character *target = get_character(&game_state, target_id);
  if (!target || !target->alive)
    target_id = 0;
  predict_state();
  set_camera_geometry();
  update_meshes();
  update_character_nodes();
  update_prediction_ghost();
  update_spell_object_nodes();
  update_game_interface();
  // update_animation_objects();
  scene.draw_imgui();
}

void Warg_State::add_character_mesh(UID character_id)
{
  set_message("add_character_mesh with uid:", s(character_id), 1.0f);
  vec4 skin_color = rgb_vec4(253, 228, 200);

  Material_Descriptor material;
  // material.albedo = "crate_diffuse.png";
  // material.emissive = "test_emissive.png";
  // material.normal = "test_normal.png";
  // material.roughness = "crate_roughness.png";
  // material.vertex_shader = "vertex_shader.vert";
  // material.frag_shader = "fragment_shader.frag";
  // material.backface_culling = false;

  material.albedo.mod = skin_color;
  character_nodes[character_id] = scene.add_mesh(cube, s("character_id:", character_id), &material);

  Node_Index character_node = character_nodes[character_id];

  Material_Descriptor hp_bar_material;
  hp_bar_material.emissive = "color(1, 1, 1, 1)";
  Node_Index hp_bar = scene.add_mesh(cube, "hp_bar", &hp_bar_material);
  scene.set_parent(hp_bar, character_node);
  float32 bar_z_pos = game_state.characters[character_id].radius.z;
  scene.nodes[hp_bar].position = vec3(0.f, 0.f, 0.6f);
  scene.nodes[hp_bar].scale = vec3(1.2f, 1.2f, 0.07f);

  hp_bar_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
  Node_Index hp_bar_background = scene.add_mesh(cube, "hp_bar_background", &hp_bar_material);
  scene.set_parent(hp_bar_background, character_node);
  scene.nodes[hp_bar_background].position = vec3(0.f, 0.f, 0.6f);
  scene.nodes[hp_bar_background].scale = vec3(1.2f, 1.2f, 0.07f);

  hp_bar_material.albedo.mod = vec4(.4f, 100.f, .4f, 1.f);
  Node_Index hp_bar_foreground = scene.add_mesh(cube, "hp_bar_foreground", &hp_bar_material);
  scene.set_parent(hp_bar_foreground, character_node);
  scene.nodes[hp_bar_foreground].position = vec3(0.f, 0.f, 0.6f);
  scene.nodes[hp_bar_foreground].scale = vec3(1.21f, 1.21f, 0.08f);

  Material_Descriptor solid_material;
  Material_Descriptor suit_material;
  suit_material.albedo.mod = vec4(0.f);
  suit_material.roughness = "color(1, 1, 1, 1)";
  suit_material.normal = "cusion_normal.png";
  suit_material.uv_scale = vec2(1.f);
  Material_Descriptor shoe_material;
  shoe_material.roughness = "color(0.05, 0.05, 0.05, 0.05)";
  shoe_material.metalness = "color(1, 1, 1, 1)";
  shoe_material.albedo.mod = Conductor_Reflectivity::gold;
  {
    // shirt
    solid_material.albedo.mod = vec4(1.f);
    Node_Index shirt = scene.add_mesh(cube, "shirt", &solid_material);
    scene.set_parent(shirt, character_node);
    scene.nodes[shirt].position = vec3(0.f, 0.0f, 0.15f);
    scene.nodes[shirt].scale = vec3(1.05f, 1.05f, 0.3f);

    // tie
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index tie = scene.add_mesh(cube, "tie", &solid_material);
    scene.set_parent(tie, shirt);
    scene.nodes[tie].position = vec3(0.f, 0.f, 0.f);
    scene.nodes[tie].scale = vec3(0.2f, 1.1f, 1.f);

    // jacket left
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index jacket_left = scene.add_mesh(cube, "jacket_left", &solid_material);
    scene.set_parent(jacket_left, shirt);
    scene.nodes[jacket_left].position = vec3(0.425f, 0.0f, -0.1f);
    scene.nodes[jacket_left].scale = vec3(0.3f, 1.1f, 1.2f);

    // jacket right
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index jacket_right = scene.add_mesh(cube, "jacket_right", &solid_material);
    scene.set_parent(jacket_right, shirt);
    scene.nodes[jacket_right].position = vec3(-0.425f, 0.0f, -0.1f);
    scene.nodes[jacket_right].scale = vec3(0.3f, 1.1f, 1.2f);

    // jacket back
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index jacket_back = scene.add_mesh(cube, "jacket_back", &solid_material);
    scene.set_parent(jacket_back, shirt);
    scene.nodes[jacket_back].position = vec3(0.f, -0.5f, -0.1f);
    scene.nodes[jacket_back].scale = vec3(1.f, .2f, 1.2f);

    // belt
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index belt = scene.add_mesh(cube, "belt", &solid_material);
    scene.set_parent(belt, character_node);
    scene.nodes[belt].position = vec3(0.f, 0.f, 0.0f);
    scene.nodes[belt].scale = vec3(1.02f, 1.02f, 0.05f);

    // belt buckle
    solid_material.albedo.mod = vec4(0.75f, 0.75f, 0.75f, 1.f);
    Node_Index belt_buckle = scene.add_mesh(cube, "belt_buckle", &solid_material);
    scene.set_parent(belt_buckle, belt);
    scene.nodes[belt_buckle].position = vec3(0.f, 0.f, 0.0f);
    scene.nodes[belt_buckle].scale = vec3(0.1f, 1.02f, 1.f);

    // pants
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index pants = scene.add_mesh(cube, "pants", &solid_material);
    scene.set_parent(pants, character_node);
    scene.nodes[pants].position = vec3(0.f, 0.f, -0.25f);
    scene.nodes[pants].scale = vec3(1.05f, 1.05f, 0.5f);

    // left shoe
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index left_shoe = scene.add_mesh(cube, "left_shoe", &solid_material);
    scene.set_parent(left_shoe, character_node);
    scene.nodes[left_shoe].position = vec3(0.25f, 0.75f, -0.47f);
    scene.nodes[left_shoe].scale = vec3(.4f, 1.5f, 0.06f);

    // right shoe
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index right_shoe = scene.add_mesh(cube, "right_shoe", &solid_material);
    scene.set_parent(right_shoe, character_node);
    scene.nodes[right_shoe].position = vec3(-0.25f, 0.75f, -0.47f);
    scene.nodes[right_shoe].scale = vec3(.4f, 1.5f, 0.06f);

    // right shoulder_joint
    solid_material.albedo.mod = vec4(1.0f, 0.f, 0.f, 1.f);
    Node_Index right_shoulder_joint = scene.add_mesh(cube, "right_shoulder_joint", &solid_material);
    scene.set_parent(right_shoulder_joint, character_node);
    scene.nodes[right_shoulder_joint].visible = false;
    scene.nodes[right_shoulder_joint].propagate_visibility = false;
    scene.nodes[right_shoulder_joint].position = vec3(0.8f, 0.f, 0.3f);

    // right arm
    solid_material.albedo.mod = skin_color;
    Node_Index right_arm = scene.add_mesh(cube, "right_arm", &solid_material);
    scene.set_parent(right_arm, right_shoulder_joint);
    scene.nodes[right_arm].position = vec3(0.f, 0.f, -0.25f);
    scene.nodes[right_arm].scale = vec3(0.4f, 0.4f, 0.5f);

    // right sleeve
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index right_sleeve = scene.add_mesh(cube, "right_sleeve", &solid_material);
    scene.set_parent(right_sleeve, right_arm);
    scene.nodes[right_sleeve].position = vec3(0.f, 0.f, 0.35f);
    scene.nodes[right_sleeve].scale = vec3(1.15f, 1.15f, 0.35f);

    // left shoulder_joint
    solid_material.albedo.mod = vec4(1.0f, 0.f, 0.f, 1.f);
    Node_Index left_shoulder_joint = scene.add_mesh(cube, "left_shoulder_joint", &solid_material);
    scene.set_parent(left_shoulder_joint, character_node);
    scene.nodes[left_shoulder_joint].visible = false;
    scene.nodes[left_shoulder_joint].propagate_visibility = false;
    scene.nodes[left_shoulder_joint].position = vec3(-0.8f, 0.f, 0.3f);

    // left arm
    solid_material.albedo.mod = skin_color;
    Node_Index left_arm = scene.add_mesh(cube, "left_arm", &solid_material);
    scene.set_parent(left_arm, left_shoulder_joint);
    scene.nodes[left_arm].position = vec3(0.f, 0.f, -0.25f);
    scene.nodes[left_arm].scale = vec3(0.4f, 0.4f, 0.5f);

    // left sleeve
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index left_sleeve = scene.add_mesh(cube, "left_sleeve", &solid_material);
    scene.set_parent(left_sleeve, left_arm);
    scene.nodes[left_sleeve].position = vec3(0.f, 0.f, 0.1f);
    scene.nodes[left_sleeve].scale = vec3(1.5f, 1.5f, 0.85f);

    // head
    solid_material.albedo.mod = skin_color;
    Node_Index head = scene.add_mesh(cube, "head", &solid_material);
    scene.set_parent(head, character_node);
    scene.nodes[head].position = vec3(0.f, 0.f, 0.4f);
    scene.nodes[head].scale = vec3(1.1f, 1.1f, 0.2f);

    // face anchor
    Node_Index face = scene.add_mesh(cube, "face", &solid_material);
    scene.set_parent(face, head);
    scene.nodes[face].visible = false;
    scene.nodes[face].propagate_visibility = false;
    scene.nodes[face].position = vec3(0.f, 0.5f, 0.f);
    scene.nodes[face].scale = vec3(1.f, 1.f, 1.f);

    // left eyebrow
    solid_material.albedo.mod = rgb_vec4(2, 1, 1);
    Node_Index left_eyebrow = scene.add_mesh(cube, "left_eyebrow", &solid_material);
    scene.set_parent(left_eyebrow, face);
    scene.nodes[left_eyebrow].position = vec3(-0.2f, 0.f, 0.2f);
    scene.nodes[left_eyebrow].scale = vec3(0.3f, 0.01f, 0.05f);

    // left eye
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
    solid_material.albedo.mod = vec4(0.f, 0.f, 0.f, 1.f);
    Node_Index right_pupil = scene.add_mesh(cube, "right_pupil", &solid_material);
    scene.set_parent(right_pupil, right_iris);
    scene.nodes[right_pupil].position = vec3(0.f);
    scene.nodes[right_pupil].scale = vec3(0.5f, 1.5f, 0.5f);

    // hair
    solid_material.albedo.mod = rgb_vec4(40, 20, 25);
    Node_Index hair = scene.add_mesh(cube, "hair", &solid_material);
    scene.set_parent(hair, head);
    scene.nodes[hair].position = vec3(0.f, -0.1f, 0.1f);
    scene.nodes[hair].scale = vec3(1.2f, 1.f, 1.2f);

    // fringe
    Node_Index fringe = scene.add_mesh(cube, "fringe", &solid_material);
    scene.set_parent(fringe, head);
    scene.nodes[fringe].position = vec3(0.f, 0.5f, 0.5f);
    scene.nodes[fringe].scale = vec3(1.1f, 0.2f, 0.4f);

    // lips
    solid_material.albedo.mod = rgb_vec4(200, 80, 80);
    Node_Index lips = scene.add_mesh(cube, "lips", &solid_material);
    scene.set_parent(lips, head);
    scene.nodes[lips].position = vec3(0.f, 0.5f, -0.25f);
    scene.nodes[lips].scale = vec3(0.3f, 0.05f, 0.1f);
  }
}

void State_Message::handle(Warg_State &state)
{
  // set_message("State_Message::handle()");
  state.pc = pc;
  state.server_state.character_count = character_count;
  state.server_state.spell_object_count = spell_object_count;
  for (size_t i = 0; i < character_count; i++)
    state.server_state.characters[i] = characters[i];
  for (size_t i = 0; i < spell_object_count; i++)
    state.server_state.spell_objects[i] = spell_objects[i];
  // set_message(s("overwriting state.server_state.tick = ", state.server_state.tick, " with: ", tick), "", 1.0f);
  state.server_state.tick = tick;
  // set_message(
  //    s("overwriting state.server_state.input_number = ", state.server_state.input_number, " with: ", input_number),
  //    "", 1.0f);
  state.server_state.input_number = input_number;

  state.input_buffer.pop_older_than(input_number);
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
  ImGui::PopStyleVar(2);
}

void Warg_State::update_cast_bar()
{
  Character *player_character = get_character(&game_state, pc);
  if (!player_character)
    return;

  if (!player_character->casting)
    return;

  const vec2 resolution = CONFIG.resolution;

  ImVec2 size = ImVec2(200, 32);
  ImVec2 position = ImVec2(resolution.x / 2 - size.x / 2, CONFIG.resolution.y * 0.7f);
  float32 progress = player_character->cast_progress / player_character->casting_spell->formula->cast_time;

  create_cast_bar("player_cast_bar", progress, position, size);
}

void Warg_State::update_unit_frames()
{
  Character *player_character = get_character(&game_state, pc);
  if (!player_character)
    return;

  auto make_unit_frame = [&](const char *name, Character *character, ImVec2 size, ImVec2 position) {
    const char *character_name = character->name.c_str();

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

    ImGui::SetCursorPos(v(grid.get_position(0, 0)));
    ImGui::Text("%s", character_name);

    ImGui::SetCursorPos(v(grid.get_position(0, 1)));
    float32 hp_percentage = (float32)character->hp / (float32)character->hp_max;
    ImVec4 hp_color = ImVec4((1.f - hp_percentage) * 0.75f, hp_percentage * 0.75f, 0.f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hp_color);
    ImGui::ProgressBar(hp_percentage, v(grid.get_section_size(1, 2)), "");
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(v(grid.get_position(0, 3)));
    float32 mana_percentage = (float32)character->mana / (float32)character->mana_max;
    vec4 mana_color = rgb_vec4(64, 112, 255);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(mana_color.x, mana_color.y, mana_color.z, mana_color.w));
    ImGui::ProgressBar(mana_percentage, v(grid.get_section_size(1, 1)), "");
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar(3);
  };

  auto make_target_buffs = [&](std::vector<Buff> *buffs, vec2 position, vec2 size, bool debuffs) {
    uint32 num_buffs = buffs->size();

    Layout_Grid outer_grid(size, vec2(0), vec2(2), vec2(num_buffs, 1), vec2(1, 1), 1);

    for (size_t i = 0; i < num_buffs; i++)
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

      ImTextureID icon = (ImTextureID)(*buffs)[i].def.icon.get_handle();
      ImGui::SetCursorPos(v(inner_grid.get_position(0, 0)));
      ImGui::Image(icon, v(inner_grid.get_section_size(1, 1)));

      ImGui::End();
      ImGui::PopStyleVar(4);
    }
  };

  Layout_Grid grid(vec2(400, 160), vec2(10), vec2(5), 2, 6);

  make_unit_frame("player_unit_frame", player_character, v(grid.get_section_size(1, 3)), v(grid.get_position(0, 0)));

  // if (!game_state.characters.count(target_id))
  //  return;
  // Character *target = &game_state.characters[target_id]; // sponge
  Character *target = player_character;

  make_unit_frame("target_unit_frame", target, v(grid.get_section_size(1, 3)), v(grid.get_position(1, 0)));
  make_target_buffs(&target->buffs, grid.get_position(1, 3), grid.get_section_size(1, 1), false);
  make_target_buffs(&target->debuffs, grid.get_position(1, 4), grid.get_section_size(1, 1), true);
  if (!target->casting)
    return;
  float32 cast_progress = target->cast_progress / target->casting_spell->formula->cast_time;
  create_cast_bar("target_cast_bar", cast_progress, v(grid.get_position(1, 5)), v(grid.get_section_size(1, 1)));
}

void Warg_State::update_icons()
{
  Character *player_character = get_character(&game_state, pc);
  ASSERT(player_character);

  static Framebuffer framebuffer = Framebuffer();
  static Shader shader = Shader("passthrough.vert", "duration_spiral.frag");
  static std::vector<Texture *> sources;

  static bool configured = false;
  static size_t num_spells = 0;
  if (!configured)
  {
    Texture_Descriptor texture_descriptor;
    texture_descriptor.size = ivec2(56);
    texture_descriptor.minification_filter = GL_LINEAR;

    ASSERT(interface_state.action_bar_textures.size() == 0);
    ASSERT(sources.size() == 0);

    num_spells = sdb->spells.size();
    interface_state.action_bar_textures.resize(num_spells);
    sources.resize(num_spells);
    framebuffer.color_attachments.resize(num_spells);

    for (size_t i = 0; i < num_spells; i++)
    {
      texture_descriptor.name = s("duration-spiral-", i);
      interface_state.action_bar_textures[i] = Texture(texture_descriptor);
      sources[i] = &sdb->spells[i].icon;
      framebuffer.color_attachments[i] = interface_state.action_bar_textures[i];
    }
    configured = true;
  }

  framebuffer.init();
  framebuffer.bind();

  shader.use();
  shader.set_uniform("count", (int)sources.size());
  for (size_t i = 0; i < num_spells; i++)
  {
    std::string spell_name = sdb->spells[i].name;
    Spell *spell = &player_character->spellbook[spell_name];

    float32 cooldown_percent = 0.f;
    float32 cooldown_remaining = 0.f;

    float32 cooldown = sdb->spells[i].cooldown;
    if (cooldown > 0.f)
    {
      cooldown_remaining = spell->cooldown_remaining;
      cooldown_percent = cooldown_remaining / cooldown;
    }
    if (spell->formula->on_global_cooldown && cooldown_remaining < player_character->gcd)
      cooldown_percent = player_character->gcd / player_character->e_stats.gcd;
    shader.set_uniform(s("progress", i).c_str(), cooldown_percent);
    set_message(s("progress", i, ":"), s(cooldown_percent), 1.0f);
  }

  run_pixel_shader(&shader, &sources, &framebuffer);
}

void Warg_State::update_action_bar()
{
  update_icons();

  const vec2 resolution = CONFIG.resolution;
  const size_t number_abilities = interface_state.action_bar_textures.size();
  Layout_Grid grid(vec2(500, 40), vec2(2), vec2(1), vec2(number_abilities, 1), vec2(1, 1), 1);

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
  for (size_t i = 0; i < interface_state.action_bar_textures.size(); i++)
  {
    ImGui::SetCursorPos(v(grid.get_position(i, 0)));
    put_imgui_texture(&interface_state.action_bar_textures[i], grid.get_section_size(1, 1));
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
}

void Warg_State::update_buff_indicators()
{
  Character *player_character = get_character(&game_state, pc);
  ASSERT(player_character);

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

  auto create_indicator = [&](vec2 position, vec2 size, ImTextureID icon, float64 duration, bool debuff, size_t i) {
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
    ImGui::Image(icon, v(grid.get_section_size(1, 2)));
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(v(grid.get_position(0, 2)));
    ImGui::Text("%s", countdown_string.c_str());

    ImGui::End();
    ImGui::PopStyleVar(3);
  };

  size_t max_columns = 18;
  Layout_Grid grid(vec2(800, 300), vec2(10), vec2(5), max_columns, 3);

  for (size_t i = 0; i < player_character->buffs.size() && i < max_columns; i++)
  {
    Buff *buff = &player_character->buffs[i];

    float32 position_x = resolution.x - grid.get_position(i, 0).x - grid.get_section_size(1, 1).x;
    create_indicator(vec2(position_x, grid.get_position(i, 0).y), grid.get_section_size(1, 1),
        (ImTextureID)buff->def.icon.get_handle(), buff->duration, false, i);
  }

  for (size_t i = 0; i < player_character->debuffs.size() && i < max_columns; i++)
  {
    Buff *debuff = &player_character->debuffs[i];

    float32 position_x = resolution.x - grid.get_position(i, 0).x - grid.get_section_size(1, 1).x;
    create_indicator(vec2(position_x, grid.get_position(i, 2).y), grid.get_section_size(1, 1),
        (ImTextureID)debuff->def.icon.get_handle(), debuff->duration, true, i);
  }
}

void Warg_State::update_game_interface()
{
  Character *player_character = get_character(&game_state, pc);
  if (!player_character)
    return;

  const vec2 resolution = CONFIG.resolution;

  update_cast_bar();
  update_unit_frames();
  update_action_bar();
  update_buff_indicators();
}

Character *get_character(Game_State *game_state, UID id)
{
  for (size_t i = 0; i < game_state->character_count; i++)
  {
    Character *character = &game_state->characters[i];
    if (character->id == id)
      return character;
  }

  return nullptr;
}