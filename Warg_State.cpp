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
  server = std::make_unique<Warg_Server>();
  server->connect(&out, &in);

  client = std::make_unique<Warg_Client>(&scene, &in);

  Material_Descriptor material;
  material.albedo = "pebbles_diffuse.png";
  material.emissive = "";
  material.normal = "pebbles_normal.png";
  material.roughness = "pebbles_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "world_origin_distance.frag";
  material.backface_culling = false;
  material.uv_scale = vec2(2);

  Node_Ptr ground_mesh =
      scene.add_primitive_mesh(plane, "ground_plane", material);
  ground_mesh->position = client->map.ground_pos;
  ground_mesh->scale = client->map.ground_dim;
  map_meshes.push_back(ground_mesh);

  for (auto &wall : client->map.walls)
  {
    Mesh_Data data;
    vec3 a, b, c, d;
    a = wall.p1;
    b = vec3(wall.p2.x, wall.p2.y, 0);
    c = vec3(wall.p2.x, wall.p2.y, wall.h);
    d = vec3(wall.p1.x, wall.p1.y, wall.h);

    add_quad(a, b, c, d, data);
    auto mesh = scene.add_mesh(data, material, "some wall");

    map_meshes.push_back(mesh);
  }

  scene.lights.light_count = 1;
  Light *light = &scene.lights.lights[0];
  light->position = vec3{0, 0, 10};
  light->color = 30.0f * vec3(1.0f, 0.93f, 0.92f);
  light->attenuation = vec3(1.0f, .045f, .0075f);
  light->ambient = 0.02f;
  light->type = omnidirectional;

  out.push(char_spawn_request_event("Eirich", 0));
  out.push(char_spawn_request_event("Veuxia", 0));
  out.push(char_spawn_request_event("Selion", 1));
  out.push(char_spawn_request_event("Veuxe", 1));

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
        out.push(jump_event(client->pc));
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
          client->pc = 0;
        }
        else if (client->pc >= client->chars.size() - 1)
        {
          free_cam = true;
        }
        else
        {
          client->pc += 1;
        }
      }
      if (_e.key.keysym.sym == SDLK_TAB && !free_cam)
      {
        int first_lower = -1, first_higher = -1;
        for (int i = client->chars.size() - 1; i >= 0; i--)
        {
          if (client->chars[i].alive &&
              client->chars[i].team != client->chars[client->pc].team)
          {
            if (i < client->chars[client->pc].target)
            {
              first_lower = i;
            }
            else if (i > client->chars[client->pc].target)
            {
              first_higher = i;
            }
          }
        }

        if (first_higher != -1)
        {
          client->chars[client->pc].target = first_higher;
        }
        else if (first_lower != -1)
        {
          client->chars[client->pc].target = first_lower;
        }
        set_message("Target",
            s(client->chars[client->pc].name, " targeted ",
                client->chars[client->chars[client->pc].target].name),
            3.0f);
      }
      if (_e.key.keysym.sym == SDLK_1 && !free_cam)
      {
        out.push(cast_event(
            client->pc, client->chars[client->pc].target, "Frostbolt"));
      }
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

  if (client->pc < 0)
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

    cam.pos = client->chars[client->pc].pos +
              vec3(cam_rel.x, cam_rel.y, cam_rel.z) * cam.zoom;
    cam.dir = -vec3(cam_rel);
  }

  previous_mouse_state = mouse_state;
}

void Warg_State::update()
{
  server->update(dt);
  client->update(dt);
}

Map make_nagrand()
{
  Map nagrand;

  nagrand.walls.push_back({{0, 8, 0}, {12, 8}, 10});
  nagrand.walls.push_back({{12, 0, 0}, {12, 8}, 10});
  nagrand.walls.push_back({{12, 0, 0}, {20, 0}, 10});
  nagrand.walls.push_back({{20, 0, 0}, {20, 8}, 10});
  nagrand.walls.push_back({{20, 8, 0}, {32, 8}, 10});
  nagrand.walls.push_back({{28, 8, 0}, {32, 12}, 10});
  nagrand.walls.push_back({{32, 8, 0}, {32, 36}, 10});
  nagrand.walls.push_back({{32, 36, 0}, {20, 36}, 10});
  nagrand.walls.push_back({{20, 36, 0}, {20, 44}, 10});
  nagrand.walls.push_back({{20, 44, 0}, {12, 44}, 10});
  nagrand.walls.push_back({{12, 44, 0}, {12, 36}, 10});
  nagrand.walls.push_back({{12, 36, 0}, {0, 36}, 10});
  nagrand.walls.push_back({{0, 36, 0}, {0, 8}, 10});
  nagrand.walls.push_back({{4, 28, 0}, {4, 32}, 10});
  nagrand.walls.push_back({{4, 32, 0}, {8, 32}, 10});
  nagrand.walls.push_back({{8, 32, 0}, {8, 28}, 10});
  nagrand.walls.push_back({{8, 28, 0}, {4, 28}, 10});
  nagrand.walls.push_back({{24, 28, 0}, {24, 32}, 10});
  nagrand.walls.push_back({{24, 32, 0}, {28, 32}, 10});
  nagrand.walls.push_back({{28, 32, 0}, {28, 28}, 10});
  nagrand.walls.push_back({{28, 28, 0}, {24, 28}, 10});
  nagrand.walls.push_back({{24, 16, 0}, {28, 16}, 10});
  nagrand.walls.push_back({{28, 16, 0}, {28, 12}, 10});
  nagrand.walls.push_back({{28, 12, 0}, {24, 12}, 10});
  nagrand.walls.push_back({{24, 12, 0}, {24, 16}, 10});
  nagrand.walls.push_back({{8, 12, 0}, {8, 16}, 10});
  nagrand.walls.push_back({{8, 16, 0}, {4, 16}, 10});
  nagrand.walls.push_back({{4, 16, 0}, {4, 12}, 10});
  nagrand.walls.push_back({{4, 12, 0}, {8, 12}, 10});

  nagrand.ground_pos = vec3(16, 22, 0);
  nagrand.ground_dim = vec3(32, 44, 0.05);
  nagrand.ground_dir = vec3(0, 1, 0);

  nagrand.spawn_pos[0] = {16, 4, 0.5f};
  nagrand.spawn_pos[1] = {16, 40, 0.5f};
  nagrand.spawn_dir[0] = {0, 1, 0};
  nagrand.spawn_dir[1] = {0, -1, 0};

  return nagrand;
}
