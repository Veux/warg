#include "stdafx.h"
#include "Warg_State.h"
#include "Globals.h"
#include "Render.h"
#include "State.h"
#include "Animation_Utilities.h"
#include "UI.h"

using namespace glm;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

bool SERVER_SEEN = false;
bool CLIENT_SEEN = false;
bool WANT_SPAWN_OCTREE_BOX = false;
bool WANT_CLEAR_OCTREE = false;

extern void small_object_water_settings(Uniform_Set_Descriptor *dst);
//{
//  dst->float32_uniforms["index_of_refraction"] = 1.15f;
//  dst->float32_uniforms["refraction_offset_factor"] = 0.02501;
//  dst->float32_uniforms["water_eps"] = 0.0001;
//  dst->float32_uniforms["water_scale"] = 4.87;
//  dst->float32_uniforms["water_speed"] = 0.201;
//  dst->float32_uniforms["water_dist_scale"] = .81;
//  dst->float32_uniforms["water_dist_exp"] = 1.67;
//  dst->float32_uniforms["water_height_scale"] = 3.0;
//  dst->float32_uniforms["water_dist_min"] = 5.;
//  dst->float32_uniforms["water_scale2"] = 1.52;
//  dst->float32_uniforms["height_scale2"] = 3.0;
//  dst->float32_uniforms["water_time2"] = 0.0192;
//  dst->float32_uniforms["surfdotv_exp"] = 2.35f;
//  dst->bool_uniforms["water_use_uv"] = true;
//}

Warg_State::Warg_State(std::string name, SDL_Window *window, ivec2 window_size, Session *session_)
    : State(name, window, window_size)
{
  session = session_;

  map = new Blades_Edge(scene);
  spell_db = Spell_Database(); /*
   map.node = scene.add_aiscene("Blades_Edge/bea2.fbx", "Blades Edge");*/
                               // map.node = scene.add_aiscene("Blades Edge", "Blades_Edge/bea2.fbx", &map.material);
                               // collider_cache = collect_colliders(scene);

  scene.initialize_lighting("Environment_Maps/GrandCanyon_C_YumaPoint/GCanyon_C_YumaPoint_8k.jpg",
      "Environment_Maps/GrandCanyon_C_YumaPoint/irradiance.hdr");
  // scene.initialize_lighting(
  //    "Assets/Textures/black.jpg", "Assets/Textures/Environment_Maps/GrandCanyon_C_YumaPoint/irradiance.hdr");
  session->push(make_unique<Char_Spawn_Request_Message>("Cubeboi", 0));

  scene.particle_emitters.push_back({});
  scene.particle_emitters.push_back({});
  scene.particle_emitters.push_back({});
  scene.particle_emitters.push_back({});

  Material_Descriptor material;

  material.vertex_shader = "instance.vert";
  material.frag_shader = "emission.frag";
  material.emissive.mod = vec4(5.25f, .25f, .35f, 1.f);
  small_object_water_settings(&material.uniform_set);
  Node_Index particle_node = scene.add_mesh(cube, "particle0", &material);
  Mesh_Index mesh_index0 = scene.nodes[particle_node].model[0].first;
  Material_Index material_index0 = scene.nodes[particle_node].model[0].second;

  material.vertex_shader = "instance.vert";
  material.frag_shader = "emission.frag";
  material.emissive.mod = vec4(1.25f, .0f, .35f, 1.f);
  Node_Index particle_node1 = scene.add_mesh(cube, "particle1", &material);
  Mesh_Index mesh_index1 = scene.nodes[particle_node1].model[0].first;
  Material_Index material_index1 = scene.nodes[particle_node1].model[0].second;

  material.vertex_shader = "instance.vert";
  material.frag_shader = "emission.frag";
  material.emissive.mod = vec4(1.05f, .0f, 2.15f, 1.f);
  Node_Index particle_node2 = scene.add_mesh(cube, "particle2", &material);
  Mesh_Index mesh_index2 = scene.nodes[particle_node2].model[0].first;
  Material_Index material_index2 = scene.nodes[particle_node2].model[0].second;

  material.vertex_shader = "instance.vert";
  material.frag_shader = "emission.frag";
  material.emissive.mod = vec4(0.0f, 2.f, 3.f, 1.f);
  Node_Index particle_node3 = scene.add_mesh(cube, "particle3", &material);
  Mesh_Index mesh_index3 = scene.nodes[particle_node3].model[0].first;
  Material_Index material_index3 = scene.nodes[particle_node3].model[0].second;
  scene.nodes[particle_node].visible = false;
  scene.nodes[particle_node1].visible = false;
  scene.nodes[particle_node2].visible = false;
  scene.nodes[particle_node3].visible = false;

  scene.nodes[particle_node].scale = vec3(0.05);
  scene.nodes[particle_node1].scale = vec3(0.05);
  scene.nodes[particle_node2].scale = vec3(0.05);
  scene.nodes[particle_node3].scale = vec3(0.05);
  scene.nodes[particle_node].position = vec3(0, 0, 10.f);
  scene.nodes[particle_node1].position = vec3(0, 0, 10.f);
  scene.nodes[particle_node2].position = vec3(0, 0, 10.f);
  scene.nodes[particle_node3].position = vec3(0, 0, 10.f);

  scene.particle_emitters[0].mesh_index = mesh_index0;
  scene.particle_emitters[0].material_index = material_index0;

  scene.particle_emitters[1].mesh_index = mesh_index1;
  scene.particle_emitters[1].material_index = material_index1;
  scene.particle_emitters[2].mesh_index = mesh_index2;
  scene.particle_emitters[2].material_index = material_index2;
  scene.particle_emitters[3].mesh_index = mesh_index3;
  scene.particle_emitters[3].material_index = material_index3;

  scene.lights.light_count = 5;

  Light *light = &scene.lights.lights[4];
  light->position = vec3(1070.00000, -545.00000, 621.00000);
  light->direction = vec3(3.00000, 0.00000, 6.00000);
  light->brightness = 4580.93408;
  light->color = vec3(1.00000, 1.00000, 0.99999);
  light->attenuation = vec3(1.00000, 1.00000, 0.00000);
  light->ambient = 0.00000;
  light->radius = 33.24000;
  light->cone_angle = 0.24000;
  light->type = Light_Type::spot;
  light->casts_shadows = true;
  light->shadow_blur_iterations = 3;
  light->shadow_blur_radius = 2.75560;
  light->shadow_near_plane = 1280.10010;
  light->shadow_far_plane = 1387.00012;
  light->max_variance = 0.00000;
  light->shadow_fov = 0.05125;
  light->shadow_map_resolution = ivec2(4096, 4096);
}

Warg_State::~Warg_State()
{
  session->end();
  delete map;
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
          get_character(&current_game_state, player_character_id))
      {
        Character *player_character = get_character(&current_game_state, player_character_id);
        size_t key = _e.key.keysym.sym - SDLK_1;
        if (key < player_character->spell_set.spell_count)
        {
          Spell_Index spell_formula_index = player_character->spell_set.spell_statuses[key].formula_index;
          session->push(std::make_unique<Cast_Message>(target_id, spell_formula_index));
        }
      }
      if (_e.key.keysym.sym == SDLK_TAB && !free_cam && get_character(&current_game_state, player_character_id))
      {
        for (size_t i = 0; i < current_game_state.character_count; i++)
        {
          Character *character = &current_game_state.characters[i];
          if (character->id != player_character_id && character->alive)
            target_id = character->id;
        }
      }
      if (_e.key.keysym.sym == SDLK_g)
      {
        WANT_SPAWN_OCTREE_BOX = true;
      }
      if (_e.key.keysym.sym == SDLK_h)
      {
        WANT_CLEAR_OCTREE = true;
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

  if (!player_character_id || !get_character(&current_game_state, player_character_id))
    return;
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
        set_message("mouse grab event", "", 1.0f);
        mouse_grabbed = true;
        last_grabbed_cursor_position = cursor_position;
        mouse_relative_mode = true;
      }

      if (!IMGUI.context->IO.WantCaptureMouse)
      {
        IMGUI.ignore_all_input = true;
      }
      draw_cursor = false;
      set_message("mouse delta: ", s(mouse_delta.x, " ", mouse_delta.y), 1.0f);
      camera.theta += mouse_delta.x * MOUSE_X_SENS;
      camera.phi += mouse_delta.y * MOUSE_Y_SENS;
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
            s("from:", cursor_position.x, " ", cursor_position.y, " to:", last_grabbed_cursor_position.x, " ",
                last_grabbed_cursor_position.y),
            1.0f);
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
    Character *player_character = get_character(&current_game_state, player_character_id);
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
  std::vector<unique_ptr<Message>> messages = session->pull();
  set_message("process_messages(): recieved:", s(messages.size(), " messages"), 1.0f);
  for (unique_ptr<Message> &message : messages)
    message->handle(*this);
}

void Warg_State::set_camera_geometry()
{
  // set_message(s("set_camera_geometry()",s(" time:", current_time), 1.0f);
  Character *player_character = get_character(&current_game_state, player_character_id);
  if (!player_character)
    return;

  float effective_zoom = camera.zoom;
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
  Character *character = get_character(&current_game_state, character_id);
  Node_Index character_node = character_nodes[character_id];

  Node_Index hp_bar = scene.find_by_name(character_node, "hp_bar");

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

  float32 hp_percent = ((float32)character->hp) / ((float32)character->hp_max);
  Material_Descriptor *md = scene.get_modifiable_material_pointer_for(hp_bar, 0);
  md->emissive.mod = vec4(1.f - hp_percent, hp_percent, 0.f, 1.f);
}

void Warg_State::update_character_nodes()
{
  for (size_t i = 0; i < current_game_state.character_count; i++)
  {
    Character *character = &current_game_state.characters[i];

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
  Character *player_character = get_character(&current_game_state, player_character_id);
  if (!player_character)
    return;

  Character_Physics *physics = &player_character->physics;

  static Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
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
  // ImGui::Begin("stats_bar", &show_stats_bar, ImVec2(10, 10), 0.5f,
  //    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
  //    |
  //        ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Begin("stats_bar", &show_stats_bar,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
          ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Ping: %dms", latency_tracker.get_latency());
  ImGui::End();
}

bool prediction_correct(UID player_character_id, Game_State &server_state, Game_State &predicted_state)
{
  bool same_input = server_state.input_number == predicted_state.input_number;

  set_message("preduction_correct()",
      s("server_state.input_number:", server_state.input_number,
          " predicted_state.input_number:", predicted_state.input_number),
      1.0f);

  Character *server_character = get_character(&server_state, player_character_id);
  Character *predicted_character = get_character(&predicted_state, player_character_id);

  if (!server_character && !predicted_character)
  {
    return same_input;
  }

  if (!server_character != !predicted_character)
  {
    return false;
  }

  ASSERT(server_character);
  ASSERT(predicted_character);

  bool same_player_physics = server_character->physics == predicted_character->physics;

  return same_input && same_player_physics;
}

void Warg_State::predict_state()
{
  // game_state = server_state;
  return;
  if (!get_character(&last_recieved_server_state, player_character_id))
    return;

  if (input_buffer.size() == 0)
    return;

  while (state_buffer.size() > 0 && state_buffer.front().input_number < last_recieved_server_state.input_number)
    state_buffer.pop_front();

  Game_State predicted_state = last_recieved_server_state;
  size_t prediction_start = 0;

  bool prediction_correct_result = false;
  if (state_buffer.size() > 0)
  {
    prediction_correct_result =
        prediction_correct(player_character_id, last_recieved_server_state, state_buffer.front());
  }
  set_message("predict_state(): prediction_correct_result:", s(prediction_correct_result));
  if (prediction_correct_result)
  {
    predicted_state = state_buffer.back();
    state_buffer.pop_front();
    prediction_start = input_buffer.size() - 1;
  }
  else
  {
    state_buffer = {};
    predicted_state = last_recieved_server_state;
  }

  size_t prediction_iterations = 0;
  for (size_t i = prediction_start; i < input_buffer.size(); i++)
  {
    Input &input = input_buffer[i];

    Character *player_character = get_character(&predicted_state, player_character_id);
    Character_Physics &physics = player_character->physics;
    vec3 radius = player_character->radius;
    float32 movement_speed = player_character->effective_stats.speed;

    move_char(*player_character, input, &scene);
    if (vec3_has_nan(physics.position))
      physics.position = map->spawn_pos[player_character->team];

    predicted_state.input_number = input.number;
    state_buffer.push_back(predicted_state);

    prediction_iterations++;
  }
  // set_message("prediction iterations:", s(prediction_iterations), 5);

  current_game_state = state_buffer.back();
}

void Warg_State::update_meshes()
{
  for (size_t i = 0; i < current_game_state.character_count; i++)
  {
    Character *character = &current_game_state.characters[i];
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
  material.emissive.mod = vec4(0.5f, 0.5f, 100.f, 1.f);

  for (size_t i = 0; i < current_game_state.spell_object_count; i++)
  {
    Spell_Object *spell_object = &current_game_state.spell_objects[i];
    if (spell_object_nodes.count(spell_object->id) == 0)
    {
      spell_object_nodes[spell_object->id] = scene.add_mesh(cube, "spell_object_cube", &material);
      // set_message(s("adding spell object node on tick ", game_state.tick), "", 20);
    }
    Node_Index mesh = spell_object_nodes[spell_object->id];
    scene.nodes[mesh].scale = vec3(0.4f);
    scene.nodes[mesh].position = spell_object->pos;
  }

  std::vector<UID> to_erase;
  for (auto &node : spell_object_nodes)
  {
    bool orphaned = true;
    for (size_t i = 0; i < current_game_state.spell_object_count; i++)
    {
      Spell_Object *spell_object = &current_game_state.spell_objects[i];
      if (node.first == spell_object->id)
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

  Character *character = get_character(&current_game_state, character_id);
  ASSERT(character);

  if (!character->alive)
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

  float32 cadence = character->effective_stats.speed / STEP_SIZE;

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
        vec3(0.f, 0.f, animation_object.physics.velocity.z), &scene);

    scene.nodes[animation_object.node].position = animation_object.physics.position;

    if (animation_object.physics.grounded)
      animation_object.physics.velocity = vec3(0);

    scene.nodes[animation_object.collision_node].position = animation_object.physics.position;
    scene.nodes[animation_object.collision_node].scale = animation_object.radius;
  }
}

void Warg_State::update()
{

  // set_message("Warg update. Time:", s(current_time), 1.0f);
  // set_message("Warg time/dt:", s(current_time / dt), 1.0f);

  process_messages();
  update_stats_bar();
  current_game_state = last_recieved_server_state;
  Character *target = get_character(&current_game_state, target_id);
  if (!target || !target->alive)
    target_id = 0;
  predict_state();
  set_camera_geometry();
  update_meshes();
  update_character_nodes();
  update_prediction_ghost();
  update_spell_object_nodes();
  // update_animation_objects();

  // camera must be set before render entities, or they get a 1 frame lag

  renderer.set_camera(camera.pos, camera.dir);

  // scene.collision_octree.clear();

  Node_Index arena = scene.nodes[map->node].children[0];
  Node_Index arena_collider = scene.nodes[arena].collider;
  Mesh_Index meshindex = scene.nodes[arena_collider].model[0].first;
  Mesh *meshptr = &scene.resource_manager->mesh_pool[meshindex];
  Mesh_Descriptor *md = &meshptr->mesh->descriptor;
  static Timer beatimer(100);
  beatimer.start();
  // scene.collision_octree.push(md);
  beatimer.stop();
  // set_message("octree timer:", beatimer.string_report(), 1.0f);

  Character *me = get_character(&current_game_state, player_character_id);
  if (!me)
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
  static std::vector<Node_Index> spawned_server_nodes;

  if (WANT_SPAWN_OCTREE_BOX)
  {
    WANT_SPAWN_OCTREE_BOX = false;
    Material_Descriptor mat;
    mat.albedo = "3D_pattern_62/pattern_335/diffuse.png";
    mat.normal = "3D_pattern_62/pattern_335/normal.png";
    mat.roughness.mod.x = 1.0f;
    mat.metalness.mod.x = 0.f;

    Node_Index newnode = scene.add_mesh(cube, "spawned box", &mat);
    Local_Session *ptr = (Local_Session *)this->session;
    Node_Index servernode = ptr->server->scene.add_mesh(cube, "spawned box", &mat);

    spawned_nodes.push_back(newnode);
    spawned_server_nodes.push_back(servernode);
    vec3 dir = normalize(camera.dir);
    vec3 axis = normalize(cross(dir, vec3(0, 0, 1)));
    float32 angle2 = atan2(dir.y, dir.x);
    scene.nodes[newnode].position = camera.pos + 12.f * dir;
    scene.nodes[newnode].scale = vec3(2);
    scene.nodes[newnode].orientation = angleAxis(-camera.phi, axis) * angleAxis(angle2, vec3(0, 0, 1));

    ptr->server->scene.nodes[servernode].position = camera.pos + 12.f * dir;
    ptr->server->scene.nodes[servernode].scale = vec3(2);
    ptr->server->scene.nodes[servernode].orientation = angleAxis(-camera.phi, axis) * angleAxis(angle2, vec3(0, 0, 1));
  }
  Local_Session *ptr = (Local_Session *)this->session;
  // ptr->server->scene.collision_octree.clear();

  // ptr->server->scene.collision_octree.push(md);
  for (Node_Index node : spawned_nodes)
  {
    mat4 M = scene.build_transformation(node);
    // scene.collision_octree.push(&mesh, &M);
    AABB prober(scene.nodes[node].position);
    push_aabb(prober, scene.nodes[node].position + 0.5f * scene.nodes[node].scale);
    push_aabb(prober, scene.nodes[node].position - 0.5f * scene.nodes[node].scale);
    uint32 counter;
    scene.collision_octree.test(prober, &counter);
  }
  for (Node_Index node : spawned_server_nodes)
  {
    Local_Session *ptr = (Local_Session *)this->session;
    mat4 M = ptr->server->scene.build_transformation(node);
    // ptr->server->scene.collision_octree.push(&mesh, &M);
  }

  if (WANT_CLEAR_OCTREE)
  {
    WANT_CLEAR_OCTREE = false;
    for (Node_Index node : spawned_nodes)
    {
      scene.delete_node(node);
    }
    spawned_nodes.clear();

    for (Node_Index servernode : spawned_server_nodes)
    {
      Local_Session *ptr = (Local_Session *)this->session;
      ptr->server->scene.delete_node(servernode);
    }
    spawned_server_nodes.clear();
  }
  velocity = 300.f * (velocity + vec3(0., 0., .02));
  // scene.collision_octree.push(&mesh, &transform, &velocity);

  Material_Descriptor material;
  static Node_Index dynamic_collider_node = scene.add_mesh(cube, "dynamic_collider_node", &material);

  scene.nodes[dynamic_collider_node].position = probe.min + (0.5f * (probe.max - probe.min));
  transform = scene.build_transformation(dynamic_collider_node);
  // scene.collision_octree.push(&mesh, &transform, &velocity);

  // fire_emitter2(
  //   &renderer, &scene, &scene.particle_emitters[0], &scene.lights.lights[0], vec3(-5.15, -17.5, 8.6), vec2(.5));
  // fire_emitter2(
  //  &renderer, &scene, &scene.particle_emitters[1], &scene.lights.lights[1], vec3(5.15, -17.5, 8.6), vec2(.5));
  // fire_emitter2(&renderer, &scene, &scene.particle_emitters[2], &scene.lights.lights[2], vec3(0, 0, 10), vec2(.5));
  // fire_emitter2(
  // &renderer, &scene, &scene.particle_emitters[3], &scene.lights.lights[3], vec3(5.15, 17.5, 8.6), vec2(.5));
  static vec3 wind_dir;
  if (fract(sin(current_time)) > .5)
    wind_dir = vec3(.575, .575, .325) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

  if (fract(sin(current_time)) > .5)
    wind_dir = vec3(.575, .575, 0) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

  Node_Index p0 = scene.find_by_name(NODE_NULL, "particle0");
  Node_Index p1 = scene.find_by_name(NODE_NULL, "particle1");
  Node_Index p2 = scene.find_by_name(NODE_NULL, "particle2");
  Node_Index p3 = scene.find_by_name(NODE_NULL, "particle3");

  scene.particle_emitters[1].descriptor.position = scene.nodes[p1].position;
  scene.particle_emitters[1].descriptor.emission_descriptor.initial_position_variance = vec3(2, 2, 0);
  scene.particle_emitters[1].descriptor.emission_descriptor.particles_per_second = 1;
  scene.particle_emitters[1].descriptor.emission_descriptor.minimum_time_to_live = 15;
  scene.particle_emitters[1].descriptor.emission_descriptor.initial_scale = scene.nodes[p1].scale;
  // scene.particle_emitters[1].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  scene.particle_emitters[1].descriptor.physics_descriptor.type = wind;
  scene.particle_emitters[1].descriptor.physics_descriptor.direction = wind_dir;
  scene.particle_emitters[1].descriptor.physics_descriptor.octree = &scene.collision_octree;
  scene.particle_emitters[1].update(renderer.projection, renderer.camera, dt);
  scene.particle_emitters[1].descriptor.physics_descriptor.intensity = random_between(111.f, 111.f);
  scene.particle_emitters[1].descriptor.physics_descriptor.bounce_min = 0.82;
  scene.particle_emitters[1].descriptor.physics_descriptor.bounce_max = 0.95;

  scene.particle_emitters[2].descriptor.position = scene.nodes[p2].position;
  scene.particle_emitters[2].descriptor.emission_descriptor.initial_position_variance = vec3(2, 2, 0);
  scene.particle_emitters[2].descriptor.emission_descriptor.particles_per_second = 1;
  scene.particle_emitters[2].descriptor.emission_descriptor.minimum_time_to_live = 15;
  scene.particle_emitters[2].descriptor.emission_descriptor.initial_scale = scene.nodes[p2].scale;
  // scene.particle_emitters[2].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  scene.particle_emitters[2].descriptor.physics_descriptor.type = wind;
  scene.particle_emitters[2].descriptor.physics_descriptor.direction = wind_dir;
  scene.particle_emitters[2].descriptor.physics_descriptor.octree = &scene.collision_octree;
  scene.particle_emitters[2].update(renderer.projection, renderer.camera, dt);
  scene.particle_emitters[2].descriptor.physics_descriptor.intensity = random_between(111.f, 111.f);
  scene.particle_emitters[2].descriptor.physics_descriptor.bounce_min = 0.82;
  scene.particle_emitters[2].descriptor.physics_descriptor.bounce_max = 0.95;

  scene.particle_emitters[3].descriptor.position = scene.nodes[p3].position;
  scene.particle_emitters[3].descriptor.emission_descriptor.initial_position_variance = vec3(2, 2, 0);
  scene.particle_emitters[3].descriptor.emission_descriptor.particles_per_second = 1;
  scene.particle_emitters[3].descriptor.emission_descriptor.minimum_time_to_live = 15;
  scene.particle_emitters[3].descriptor.emission_descriptor.initial_scale = scene.nodes[p3].scale;
  // scene.particle_emitters[3].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  scene.particle_emitters[3].descriptor.physics_descriptor.type = wind;
  scene.particle_emitters[3].descriptor.physics_descriptor.direction = wind_dir;
  scene.particle_emitters[3].descriptor.physics_descriptor.octree = &scene.collision_octree;
  scene.particle_emitters[3].update(renderer.projection, renderer.camera, dt);
  scene.particle_emitters[3].descriptor.physics_descriptor.intensity = random_between(111.f, 111.f);
  scene.particle_emitters[3].descriptor.physics_descriptor.bounce_min = 0.82;
  scene.particle_emitters[3].descriptor.physics_descriptor.bounce_max = 0.95;

  scene.particle_emitters[0].descriptor.position = scene.nodes[p0].position;
  scene.particle_emitters[0].descriptor.emission_descriptor.initial_position_variance = vec3(2, 2, 0);
  scene.particle_emitters[0].descriptor.emission_descriptor.particles_per_second = 1;
  scene.particle_emitters[0].descriptor.emission_descriptor.minimum_time_to_live = 12;
  scene.particle_emitters[0].descriptor.emission_descriptor.initial_scale = scene.nodes[p0].scale;
  // scene.particle_emitters[0].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  scene.particle_emitters[0].descriptor.physics_descriptor.type = wind;
  scene.particle_emitters[0].descriptor.physics_descriptor.direction = wind_dir;
  scene.particle_emitters[0].descriptor.physics_descriptor.octree = &scene.collision_octree;
  scene.particle_emitters[0].update(renderer.projection, renderer.camera, dt);
  scene.particle_emitters[0].descriptor.physics_descriptor.intensity = random_between(111.f, 111.f);
  scene.particle_emitters[0].descriptor.physics_descriptor.bounce_min = 0.82;
  scene.particle_emitters[0].descriptor.physics_descriptor.bounce_max = 0.95;

  uint32 particle_count = scene.particle_emitters[0].shared_data->particles.MVP_Matrices.size();
  particle_count += scene.particle_emitters[1].shared_data->particles.MVP_Matrices.size();
  particle_count += scene.particle_emitters[2].shared_data->particles.MVP_Matrices.size();
  particle_count += scene.particle_emitters[3].shared_data->particles.MVP_Matrices.size();
  set_message("Total Particle count:", s(particle_count), 1.0f);
  uint32 i = sizeof(Octree);
}

void Warg_State::draw_gui()
{
  // opengl code is allowed in here
  IMGUI_LOCK lock(this); // you must own this lock during ImGui:: calls

  update_game_interface();
  scene.draw_imgui(state_name);
}

void Warg_State::add_girl_character_mesh(UID character_id)
{

  Character *character = get_character(&current_game_state, character_id);
  character_nodes[character_id];

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
  Mesh_Descriptor md(cube, "some generic cube");

  // add it as a referencable object to the resource manager
  Mesh_Index cube_mesh_index = scene.resource_manager->push_custom_mesh(&md);

  // add the materials as referencable objects to the resource manager
  Material_Index skin_material_index = scene.resource_manager->push_custom_material(&skin_material);
  Material_Index suit_material_index = scene.resource_manager->push_custom_material(&suit_material);
  Material_Index shirt_material_index = scene.resource_manager->push_custom_material(&shirt_material);
  Material_Index shoe_material_index = scene.resource_manager->push_custom_material(&shoe_material);
  Material_Index hair_material_index = scene.resource_manager->push_custom_material(&hair_material);

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

  // youre overwriting the scale and stuff in there so ill hijack the character root
  Node_Index fake_character_node = scene.new_node();

  Node_Index character_node = scene.new_node(s("character_id:", character_id), skin_pair, fake_character_node);
  scene.nodes[character_node].position = vec3(0, 0, -0.01);
  scene.nodes[character_node].scale = vec3(0.61, .68, 0.97);
  scene.nodes[character_node].scale_vertex = vec3(0.5, .7, 1.);
  character_nodes[character_id] = fake_character_node;

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
}

void Warg_State::add_character_mesh(UID character_id)
{
  if (character_id == 3)
  {
    add_girl_character_mesh(character_id);
    return;
  }

  Character *character = get_character(&current_game_state, character_id);
  character_nodes[character_id];

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

  Mesh_Descriptor md(cube, "girl's cube");
  Mesh_Index cube_mesh_index = scene.resource_manager->push_custom_mesh(&md);
  Material_Index skin_material_index = scene.resource_manager->push_custom_material(&skin_material);
  Material_Index suit_material_index = scene.resource_manager->push_custom_material(&suit_material);
  Material_Index shirt_material_index = scene.resource_manager->push_custom_material(&shirt_material);
  Material_Index shoe_material_index = scene.resource_manager->push_custom_material(&shoe_material);
  Material_Index hair_material_index = scene.resource_manager->push_custom_material(&hair_material);

  std::pair<Mesh_Index, Material_Index> skin_pair = {cube_mesh_index, skin_material_index};
  std::pair<Mesh_Index, Material_Index> suit_pair = {cube_mesh_index, suit_material_index};
  std::pair<Mesh_Index, Material_Index> shirt_pair = {cube_mesh_index, shirt_material_index};
  std::pair<Mesh_Index, Material_Index> shoe_pair = {cube_mesh_index, shoe_material_index};
  std::pair<Mesh_Index, Material_Index> hair_pair = {cube_mesh_index, hair_material_index};

  // youre overwriting the scale and stuff in there so ill hijack the character root
  Node_Index fake_character_node = scene.new_node();

  Node_Index character_node = scene.new_node(s("character_id:", character_id), skin_pair, fake_character_node);
  // scene.nodes[character_node].scale = vec3(0.7, 1, 0.9);
  scene.nodes[character_node].scale_vertex = vec3(1);
  character_nodes[character_id] = fake_character_node;

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
}

void State_Message::handle(Warg_State &state)
{
  state.player_character_id = pc;
  game_state_copy(&state.last_recieved_server_state, &game_state);
  set_message("State_Message::handle() type with \"input_number\" of:", s(game_state.input_number), 1.0f);
  set_message("State_Message::handle() type with \"tick\" of:", s(game_state.tick), 1.0f);
  state.input_buffer.pop_older_than(state.last_recieved_server_state.input_number);
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

uint32 Latency_Tracker::get_latency()
{
  return (uint32)round(last_latency * 1000);
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
  Character *player_character = get_character(&current_game_state, player_character_id);
  if (!player_character)
    return;

  if (!player_character->casting)
    return;

  const vec2 resolution = CONFIG.resolution;

  ImVec2 size = ImVec2(200, 32);
  ImVec2 position = ImVec2(resolution.x / 2 - size.x / 2, CONFIG.resolution.y * 0.7f);
  Spell_Formula *casting_spell_formula = spell_db.get_spell(get_casting_spell_formula_index(player_character));
  float32 progress = player_character->cast_progress / casting_spell_formula->cast_time;

  create_cast_bar("player_cast_bar", progress, position, size);
}

void Warg_State::update_unit_frames()
{
  Character *player_character = get_character(&current_game_state, player_character_id);
  if (!player_character)
    return;

  auto make_unit_frame = [&](const char *name, Character *character, ImVec2 size, ImVec2 position) {
    const char *character_name = character->name;

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

  auto make_target_buffs = [&](Buff *buffs, uint8 buff_count, vec2 position, vec2 size, bool debuffs) {
    Layout_Grid outer_grid(size, vec2(0), vec2(2), vec2(buff_count, 1), vec2(1, 1), 1);

    for (size_t i = 0; i < buff_count; i++)
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

      BuffDef *buff_formula = spell_db.get_buff(buffs[i].formula_index);
      ImGui::SetCursorPos(v(inner_grid.get_position(0, 0)));
      put_imgui_texture(&buff_formula->icon, inner_grid.get_section_size(1, 1));
      ImGui::End();
      ImGui::PopStyleVar(4);
    }
  };

  Layout_Grid grid(vec2(400, 160), vec2(10), vec2(5), 2, 6);

  make_unit_frame("player_unit_frame", player_character, v(grid.get_section_size(1, 3)), v(grid.get_position(0, 0)));

  Character *target = get_character(&current_game_state, target_id);
  if (!target)
    return;

  make_unit_frame("target_unit_frame", target, v(grid.get_section_size(1, 3)), v(grid.get_position(1, 0)));
  make_target_buffs(target->buffs, target->buff_count, grid.get_position(1, 3), grid.get_section_size(1, 1), false);
  make_target_buffs(target->debuffs, target->debuff_count, grid.get_position(1, 4), grid.get_section_size(1, 1), true);
  if (!target->casting)
    return;
  Spell_Formula *casting_spell_formula = spell_db.get_spell(get_casting_spell_formula_index(target));
  float32 cast_progress = target->cast_progress / casting_spell_formula->cast_time;
  create_cast_bar("target_cast_bar", cast_progress, v(grid.get_position(1, 5)), v(grid.get_section_size(1, 1)));
}

void Warg_State::update_icons()
{

  Character *player_character = get_character(&current_game_state, player_character_id);
  ASSERT(player_character);

  static Framebuffer framebuffer = Framebuffer();
  static Shader shader = Shader("passthrough.vert", "duration_spiral.frag");
  static std::vector<Texture> sources;

  static bool setup_complete = false;
  static bool all_textures_ready = true;
  static size_t num_spells = 0;
  if (!setup_complete)
  {
    Texture_Descriptor texture_descriptor;
    texture_descriptor.source = "generate";
    texture_descriptor.minification_filter = GL_LINEAR;
    texture_descriptor.size = vec2(90, 90);
    texture_descriptor.format = GL_RGB;
    texture_descriptor.levels = 1;

    if (sources.size() != 0)
    {
      for (size_t i = 0; i < num_spells; i++)
      {
        if (!sources[i].bind_for_sampling_at(0))
        {
          all_textures_ready = false;
        }
        if (!interface_state.action_bar_textures[i].bind_for_sampling_at(0))
        {
          all_textures_ready = false;
        }
      }
         if (!all_textures_ready)
        return;
    }
    else
    {
      ASSERT(interface_state.action_bar_textures.size() == 0);
      ASSERT(sources.size() == 0);

      num_spells = player_character->spell_set.spell_count;
      interface_state.action_bar_textures.resize(num_spells);
      sources.resize(num_spells);
      framebuffer.color_attachments.resize(num_spells);

      for (size_t i = 0; i < num_spells; i++)
      {
        Spell_Formula *formula = spell_db.get_spell(player_character->spell_set.spell_statuses[i].formula_index);
        texture_descriptor.name = s("duration-spiral-", i);
        interface_state.action_bar_textures[i] = Texture(texture_descriptor);
        sources[i] = formula->icon;
        if (!sources[i].bind_for_sampling_at(0))
        {
          all_textures_ready = false;
        }
        if (!interface_state.action_bar_textures[i].bind_for_sampling_at(0))
        {
          all_textures_ready = false;
        }
        framebuffer.color_attachments[i] = interface_state.action_bar_textures[i];
      }
      if (!all_textures_ready)
        return;
    }
    setup_complete = true;
  }

  framebuffer.init();
  framebuffer.bind_for_drawing_dst();

  shader.use();
  shader.set_uniform("count", (int)sources.size());
  for (size_t i = 0; i < num_spells; i++)
  {
    Spell_Status *spell_status = &player_character->spell_set.spell_statuses[i];
    Spell_Formula *spell_formula = spell_db.get_spell(spell_status->formula_index);

    float32 cooldown_percent = 0.f;
    float32 cooldown_remaining = 0.f;

    float32 cooldown = spell_formula->cooldown;
    if (cooldown > 0.f)
    {
      cooldown_remaining = spell_status->cooldown_remaining;
      cooldown_percent = cooldown_remaining / cooldown;
    }
    if (spell_formula->on_global_cooldown && cooldown_remaining < player_character->global_cooldown)
      cooldown_percent = player_character->global_cooldown / player_character->effective_stats.global_cooldown;
    shader.set_uniform(s("progress", i).c_str(), cooldown_percent);
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
  Character *player_character = get_character(&current_game_state, player_character_id);
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

  auto create_indicator = [&](vec2 position, vec2 size, Texture_Descriptor *icon, float64 duration, bool debuff,
                              size_t i) {
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

  for (size_t i = 0; i < player_character->buff_count && i < max_columns; i++)
  {
    Buff *buff = &player_character->buffs[i];
    BuffDef *buff_formula = spell_db.get_buff(buff->formula_index);

    float32 position_x = resolution.x - grid.get_position(i, 0).x - grid.get_section_size(1, 1).x;
    create_indicator(vec2(position_x, grid.get_position(i, 0).y), grid.get_section_size(1, 1), &buff_formula->icon,
        buff->duration, false, i);
  }

  for (size_t i = 0; i < player_character->debuff_count && i < max_columns; i++)
  {
    Buff *debuff = &player_character->debuffs[i];
    BuffDef *debuff_formula = spell_db.get_buff(debuff->formula_index);

    float32 position_x = resolution.x - grid.get_position(i, 0).x - grid.get_section_size(1, 1).x;
    create_indicator(vec2(position_x, grid.get_position(i, 2).y), grid.get_section_size(1, 1), &debuff_formula->icon,
        debuff->duration, true, i);
  }
}

void Warg_State::update_game_interface()
{
  Character *player_character = get_character(&current_game_state, player_character_id);
  if (!player_character)
    return;

  const vec2 resolution = CONFIG.resolution;

  update_cast_bar();
  update_unit_frames();
  update_action_bar();
  update_buff_indicators();
  update_stats_bar();
}

Character *get_character(Game_State *game_state, UID id)
{
  for (size_t i = 0; i < game_state->character_count; i++)
  {
    Character *character = &game_state->characters[i];
    if (character->id == id)
      return character;
  }

  set_message("get_character failed with UID:", s(id), 1.0f);
  return nullptr;
}
