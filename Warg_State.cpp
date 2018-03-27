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

  client = std::make_unique<Warg_Client>(&scene, &in);

  client->map.node =
      scene.add_aiscene("blades_edge.obj", nullptr, &client->map.material);

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

  client = std::make_unique<Warg_Client>(&scene, &in);

  client->map.node =
      scene.add_aiscene("blades_edge.obj", nullptr, &client->map.material);

  Material_Descriptor sky_mat;
  sky_mat.backface_culling = false;
  sky_mat.vertex_shader = "vertex_shader.vert";
  sky_mat.frag_shader = "skybox.frag";
  Node_Ptr skybox = scene.add_primitive_mesh(cube, "skybox", sky_mat);
  skybox->scale = vec3(500);
  scene.set_parent(skybox, scene.root, true);

  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();
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
        out.push(jump_event(client->pc));
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

  set_message("mouse position:", s(mouse.x, " ", mouse.y), 1.0f);
  set_message("mouse grab position:",
      s(last_grabbed_mouse_position.x, " ", last_grabbed_mouse_position.y),
      1.0f);

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
  bool last_seen_lmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool last_seen_rmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

  if (client->pc < 0)
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
    vec4 cam_rel;
    set_message(
        "mouse_is_relative_mode: ", s(SDL_GetRelativeMouseMode()), 1.0f);
    // grab mouse, rotate camera, restore mouse
    if ((left_button_down || right_button_down) &&
        (last_seen_lmb || last_seen_rmb))
    { // currently holding
      if (!mouse_grabbed)
      { // first hold
        set_message("mouse grab event", "", 1.0f);
        mouse_grabbed = true;
        last_grabbed_mouse_position = mouse;
        SDL_SetRelativeMouseMode(SDL_bool(true));
        SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);
      }
      set_message("mouse delta: ", s(mouse_delta.x, " ", mouse_delta.y), 1.0f);
      SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);
      cam.theta += mouse_delta.x * MOUSE_X_SENS;
      cam.phi += mouse_delta.y * MOUSE_Y_SENS;
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

    if (right_button_down)
      out.push(
          dir_event(client->pc, normalize(-vec3(cam_rel.x, cam_rel.y, 0))));

    int m = Move_Status::None;
    if (is_pressed(SDL_SCANCODE_W))
      m |= Move_Status::Forwards;
    if (is_pressed(SDL_SCANCODE_S))
      m |= Move_Status::Backwards;
    if (is_pressed(SDL_SCANCODE_A))
      m |= Move_Status::Left;
    if (is_pressed(SDL_SCANCODE_D))
      m |= Move_Status::Right;
    out.push(move_event(client->pc, (Move_Status)m));

    ASSERT(client->pc >= 0 && client->chars.count(client->pc));
    vec3 player_pos = client->chars[client->pc].pos;
    float effective_zoom = cam.zoom;
    for (auto &surface : client->map.surfaces)
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

void Warg_State::update()
{
  if (local)
  {
    server->update(dt);
    client->update(dt);
  }
  else
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

    client->update(dt);

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

  static bool show_warg_state_window = true;
  if (show_warg_state_window)
  {
    ImGui::Begin("warg_state.cpp Window", &show_warg_state_window);
    ImGui::Text("Hello from warg_state.cpp window!");
    if (ImGui::Button("Close Me"))
      show_warg_state_window = false;

    static Texture test("../Assets/Textures/pebbles_diffuse.png");

    ImGui::Image((ImTextureID)test.get_handle(), ImVec2(256, 256));
    ImGui::End();
  }  
  

  // meme
  if (client->pc >= 0)
  {
    ASSERT(client->chars.count(client->pc));
    Light *light = &scene.lights.lights[0];

    static bool first = true;
    if (first)
    {
      first = false;

      clear_color = vec3(94. / 255., 155. / 255., 1.);
      scene.lights.light_count = 2;

      scene.lights.light_count = 2;
      light->position = vec3{25.01f, 25.0f, 45.f};
      light->color = vec3(1.0f, 0.93f, 0.92f);
      light->brightness = 550.0f;
      light->attenuation = vec3(1.0f, .045f, .0075f);
      light->ambient = 0.0005f;
      light->cone_angle = 0.375f;

      light->type = Light_Type::spot;
      light->casts_shadows = true;
      // there was a divide by 0 here, a camera can't point exactly straight
      // down
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
      light->direction = client ? client->chars.size() > 0
                                      ? client->chars[client->pc].pos
                                      : vec3(0)
                                : vec3(0);
      light->ambient = 0.002f;
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

    imgui_light_array(scene.lights);

    // static float uv_scale = 14.575f;
    // ImGui::DragFloat("map_uv_scale", &uv_scale, 0.005f);

    ////->shh->bby.get()->is->ok->c++[0]->is->*->get()[0]->*(*fast);
    // client->map.node.get()->owned_children[0].get()->model[0].second.m.uv_scale
    // = vec2(uv_scale);

    //

    light = &scene.lights.lights[1];
    light->direction =
        client
            ? client->chars.size() > 0 ? client->chars[client->pc].pos : vec3(0)
            : vec3(0);
    light->color = 100.0f * vec3(1.f + sin(current_time * 1.35),
                                1.f + cos(current_time * 1.12),
                                1.f + sin(current_time * .9));
  }
}

void add_wall(std::vector<Triangle> &surfaces, Wall wall)
{
  Triangle t1, t2;

  t1.a = wall.p1;
  t1.b = vec3(wall.p2.x, wall.p2.y, t1.a.z);
  t1.c = vec3(wall.p2.x, wall.p2.y, wall.h);
  surfaces.push_back(t1);

  t2.a = wall.p1;
  t2.b = vec3(wall.p2.x, wall.p2.y, wall.h);
  t2.c = vec3(wall.p1.x, wall.p1.y, wall.h);
  surfaces.push_back(t2);
}

Map make_nagrand()
{
  Map nagrand;

  std::vector<Wall> walls;
  walls.push_back({{12, 8, 0}, {0, 8}, 10});
  walls.push_back({{12, 0, 0}, {12, 8}, 10});
  walls.push_back({{20, 0, 0}, {12, 0}, 10});
  walls.push_back({{20, 8, 0}, {20, 0}, 10});
  walls.push_back({{32, 8, 0}, {20, 8}, 10});
  walls.push_back({{32, 12, 0}, {28, 8}, 10});
  walls.push_back({{32, 36, 0}, {32, 8}, 10});
  walls.push_back({{20, 36, 0}, {32, 36}, 10});
  walls.push_back({{20, 44, 0}, {20, 36}, 10});
  walls.push_back({{12, 44, 0}, {20, 44}, 10});
  walls.push_back({{12, 36, 0}, {12, 44}, 10});
  walls.push_back({{0, 36, 0}, {12, 36}, 10});
  walls.push_back({{0, 8, 0}, {0, 36}, 10});
  walls.push_back({{4, 32, 0}, {4, 28}, 10});
  walls.push_back({{8, 32, 0}, {4, 32}, 10});
  walls.push_back({{8, 28, 0}, {8, 32}, 10});
  walls.push_back({{4, 28, 0}, {8, 28}, 10});
  walls.push_back({{24, 32, 0}, {24, 28}, 10});
  walls.push_back({{28, 32, 0}, {24, 32}, 10});
  walls.push_back({{28, 28, 0}, {28, 32}, 10});
  walls.push_back({{24, 28, 0}, {28, 28}, 10});
  walls.push_back({{28, 16, 0}, {24, 16}, 10});
  walls.push_back({{28, 12, 0}, {28, 16}, 10});
  walls.push_back({{24, 12, 0}, {28, 12}, 10});
  walls.push_back({{24, 16, 0}, {24, 12}, 10});
  walls.push_back({{8, 12, 0}, {8, 16}, 10});
  walls.push_back({{8, 16, 0}, {4, 16}, 10});
  walls.push_back({{4, 16, 0}, {4, 12}, 10});
  walls.push_back({{4, 12, 0}, {8, 12}, 10});
  for (auto &w : walls)
    add_wall(nagrand.surfaces, w);

  nagrand.surfaces.push_back({{0, 0, 0}, {32, 0, 0}, {0, 44, 0}});
  nagrand.surfaces.push_back({{32, 44, 0}, {0, 44, 0}, {32, 0, 0}});

  nagrand.surfaces.push_back({{15, 12, 0}, {17, 12, 0}, {15, 18, 5}});
  nagrand.surfaces.push_back({{15, 18, 5}, {17, 12, 0}, {17, 18, 5}});

  nagrand.spawn_pos[0] = {16, 4, 5};
  nagrand.spawn_pos[1] = {16, 40, 5};
  nagrand.spawn_dir[0] = {0, 1, 0};
  nagrand.spawn_dir[1] = {0, -1, 0};

  for (auto &s : nagrand.surfaces)
  {
    vec3 a, b, c, d;
    a = s.a;
    b = s.b;
    c = s.b;
    d = s.c;

    add_quad(a, b, c, d, nagrand.mesh);
  }
  nagrand.mesh.unique_identifier = "nagrand_arena_map";

  nagrand.material.albedo = "crate_diffuse.png";
  nagrand.material.emissive = "test_emissive.png";
  nagrand.material.normal = "test_normal.png";
  nagrand.material.roughness = "crate_roughness.png";
  nagrand.material.vertex_shader = "vertex_shader.vert";
  nagrand.material.frag_shader = "world_origin_distance.frag";

  return nagrand;
}

Map make_blades_edge()
{
  Map blades_edge;

  // spawns
  blades_edge.spawn_pos[0] = {0, 0, 15};
  blades_edge.spawn_pos[1] = {45, 45, 5};
  blades_edge.spawn_dir[0] = {0, 1, 0};
  blades_edge.spawn_dir[1] = {0, -1, 0};

  blades_edge.mesh.unique_identifier = "blades_edge_map";
  blades_edge.material.backface_culling = false;
  blades_edge.material.albedo = "bea_albedo.png";
  blades_edge.material.uses_transparency = false;
  blades_edge.material.discard_on_alpha = false;
  blades_edge.material.albedo.wrap_s = GL_CLAMP_TO_EDGE;
  blades_edge.material.albedo.wrap_t = GL_CLAMP_TO_EDGE;
  //blades_edge.material.albedo_alpha_override = 0.25f;
  blades_edge.material.emissive = "";
  //blades_edge.material.normal = "test_normal.png";
  blades_edge.material.roughness = "bea_roughness.png";
  blades_edge.material.vertex_shader = "vertex_shader.vert";
  blades_edge.material.frag_shader = "fragment_shader.frag";
  blades_edge.material.casts_shadows = true;
  blades_edge.material.uv_scale = vec2(1);

  return blades_edge;
}
