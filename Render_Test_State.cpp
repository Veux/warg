#include "Render_Test_State.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "State.h"
#include "Third_party/imgui/imgui.h"
#include <atomic>
#include <memory>
#include <sstream>
#include <thread>
using namespace glm;

void test_spheres(Scene_Graph scene)
{
  Node_Ptr node;

  Material_Descriptor material;
  material.albedo = "color(1,1,1,0.9)";
  // material.emissive = "";
  // material.normal = "test_normal.png";
  material.roughness = "color(1,1,1,0.9)";
  material.metalness = "color(1,1,1,.99)";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(1);
  material.casts_shadows = true;
  material.backface_culling = false;

  for (uint32 i = 0; i < 8; ++i)
  {
    for (uint32 j = 0; j < 8; ++j)
    {
      for (uint32 k = 0; k < 8; ++k)
      {
        float roughness = float(i) / 7.f;
        float metalness = float(j) / 7.f;
        float color = float(k) / 7.f;

        material.albedo.mod = vec4(color, 1, 1, 1.1);
        material.roughness.mod = vec4(roughness);
        material.metalness.mod = vec4(metalness);

        Node_Ptr node = scene.add_aiscene("smoothsphere.fbx", nullptr, &material);
        node->scale = vec3(0.5f);
        node->position = (node->scale * 2.f * vec3(i, k, j)) + vec3(0, 6, 1);
        scene.set_parent(node, scene.root, true);
      }
    }
  }
}

Render_Test_State::Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{
  free_cam = true;

#ifndef _DEBUG
  test_spheres(scene);
  gun = scene.add_aiscene("Cerberus/cerberus-warg.FBX");
  gun->position = vec3(4.0f, -3.0f, 2.0f);
  gun->scale = vec3(5);
#else
  gun = scene.add_primitive_mesh(cube, "debug", Material_Descriptor());
#endif

  Material_Descriptor material;
  material.albedo = "ground1_diffuse.png";
  material.emissive = "";
  material.normal = "ground1_normal.png";
  material.roughness = "color(1,1,1,1.0)";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(12);
  material.casts_shadows = true;
  material.backface_culling = false;
  ground = scene.add_primitive_mesh(cube, "world_cube", material);
  ground->position = {0.0f, 0.0f, -0.5f};
  ground->scale = {35.0f, 35.0f, 1.f};

  Material_Descriptor sky_mat;
  sky_mat.backface_culling = false;
  sky_mat.vertex_shader = "vertex_shader.vert";
  sky_mat.frag_shader = "skybox.frag";
  skybox = scene.add_primitive_mesh(cube, "skybox", sky_mat);

  material.casts_shadows = true;
  material.uv_scale = vec2(4);
  material.albedo = "color(1,1,1,1)";
  material.normal = "steel_normal.png";
  material.roughness = "steel_roughness.png";
  material.roughness.mod = vec4(.02);
  material.metalness = "color(1,1,1,1)";
  sphere = scene.add_aiscene("smoothsphere.fbx", nullptr, &material);
  // testobjects = scene.add_aiscene("testobjects.fbx", nullptr, &material);
  // testobjects->position = vec3(-5, 5, 1);
  material.roughness.mod = vec4(1);
  material.metalness.mod = vec4(1);

  // crates:
  material.albedo = "crate_albedo.png";
  material.emissive = "test_emissive.png";
  material.normal = "color(0.5,.5,1,0)";
  material.roughness = "crate_roughness.png";
  material.metalness = "crate_metalness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(1);
  material.uses_transparency = false;
  material.casts_shadows = true;

  cube_star = scene.add_primitive_mesh(cube, "star", material);
  cube_planet = scene.add_primitive_mesh(cube, "planet", material);
  scene.set_parent(cube_planet, cube_star, false);
  cube_moon = scene.add_primitive_mesh(cube, "moon", material);
  scene.set_parent(cube_moon, cube_planet, false);

  cam.phi = .25;
  cam.theta = -1.5f * half_pi<float32>();
  cam.pos = vec3(3.3, 2.3, 1.4);

  bool transp = false;

  Material_Descriptor chest_mat;
  chest_mat.metalness = BASE_ASSET_PATH + "Chest/chest_metalness.png";
  chest_mat.roughness = BASE_ASSET_PATH + "Chest/roughness.png";
  for (int y = -3; y < 3; ++y)
  {
    for (int x = -4; x < 3; ++x)
    {
      mat4 t = translate(vec3(x, y, 0.0));
      mat4 s = scale(vec3(0.25));
      mat4 basis = t * s;
      chests.push_back(scene.add_aiscene("Chest/Chest.fbx", &basis, &chest_mat));

      if (transp)
      {
        Material_Descriptor *m = &chests.back()->owned_children[0]->owned_children[0]->model[0].second.m;
        m->uses_transparency = true;
        m->albedo_alpha_override = 0.4f;
      }
      transp = !transp;
    }
  }
  Material_Descriptor tiger_mat;
  tiger_mat.backface_culling = false;
  tiger_mat.discard_on_alpha = true;
  tiger = scene.add_aiscene("tiger/tiger.fbx", &tiger_mat);
  tiger->position = vec3(-6, -3, 0.0);
  tiger->scale = vec3(2.0);
  // tiger->scale = vec3(0.45f);
  // scene.set_parent(tiger, cube_star, true);

  Node_Ptr tiger2 = scene.add_aiscene("tiger/tiger.fbx", &tiger_mat);
  tiger2->position = vec3(0, 0, 0.5);
  tiger2->scale = vec3(0.45f);
  scene.set_parent(tiger2, cube_planet, true);

  Node_Ptr tiger3 = scene.add_aiscene("tiger/tiger.fbx", &tiger_mat);
  tiger3->position = vec3(0, 0, 0.5);
  tiger3->scale = vec3(0.45f);
  scene.set_parent(tiger3, cube_moon, true);

  material.casts_shadows = false;
  material.albedo = "color(0,0,0,1)";
  material.emissive = "color(11,11,11,1)";
  material.roughness = "color(1,1,1,1)";
  material.metalness = "color(0,0,0,0)";
  cone_light1 = scene.add_aiscene("smoothsphere.fbx", &material);
  cone_light1->name = "conelight1";

  material.albedo = "color(0,0,0,1)";
  material.emissive = "color(3,3,1.5,1)";
  sun_light = scene.add_aiscene("smoothsphere.fbx", &material);
  sun_light->name = "sun";

  material.albedo = "color(0,0,0,1)";
  material.emissive = "color(1.15,0,1.15,1)";
  small_light = scene.add_aiscene("smoothsphere.fbx", &material);
}

void Render_Test_State::handle_input_events(const std::vector<SDL_Event> &events, bool block_kb, bool block_mouse)
{
  auto is_pressed = [block_kb](int key) {
    const static Uint8 *keys = SDL_GetKeyboardState(NULL);
    SDL_PumpEvents();
    return block_kb ? 0 : keys[key];
  };

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
    }
    else if (_e.type == SDL_KEYUP)
    {
      if (_e.key.keysym.sym == SDLK_F5)
      {
        free_cam = !free_cam;
        if (free_cam)
        {
          SDL_SetRelativeMouseMode(SDL_bool(true));
          ivec2 trash;
          SDL_GetRelativeMouseState(&trash.x, &trash.y);
        }
        else
        {
          SDL_SetRelativeMouseMode(SDL_bool(false));
        }
      }
    }
    else if (_e.type == SDL_MOUSEWHEEL)
    {
      ASSERT(!block_mouse);
      if (_e.wheel.y < 0)
        cam.zoom += ZOOM_STEP;
      else if (_e.wheel.y > 0)
        cam.zoom -= ZOOM_STEP;
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
  set_message("mouse grab position:", s(last_grabbed_mouse_position.x, " ", last_grabbed_mouse_position.y), 1.0f);

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
  bool last_seen_lmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool last_seen_rmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

  if (free_cam)
  {
    SDL_SetRelativeMouseMode(SDL_bool(true));
    SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);

    cam.theta += mouse_delta.x * MOUSE_X_SENS;
    cam.phi += mouse_delta.y * MOUSE_Y_SENS;
    // wrap x
    if (cam.theta > two_pi<float32>())
      cam.theta = cam.theta - two_pi<float32>();
    if (cam.theta < 0)
      cam.theta = two_pi<float32>() + cam.theta;
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (cam.phi > upper)
      cam.phi = upper;
    const float32 lower = -half_pi<float32>() + 100 * epsilon<float32>();
    if (cam.phi < lower)
      cam.phi = lower;

    mat4 rx = rotate(-cam.theta, vec3(0, 0, 1));
    vec4 vr = rx * vec4(0, 1, 0, 0);
    vec3 perp = vec3(-vr.y, vr.x, 0);
    mat4 ry = rotate(cam.phi, perp);
    cam.dir = normalize(vec3(ry * vr));

    if (is_pressed(SDL_SCANCODE_W))
      cam.pos += MOVE_SPEED * dt * cam.dir;
    if (is_pressed(SDL_SCANCODE_S))
      cam.pos -= MOVE_SPEED * dt * cam.dir;
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.x, vr.y, 0, 0);
      cam.pos += vec3(MOVE_SPEED * dt * r * v);
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.x, vr.y, 0, 0);
      cam.pos += vec3(MOVE_SPEED * dt * r * v);
    }
  }
  else
  { // wow style camera
    vec4 cam_rel;
    // grab mouse, rotate camera, restore mouse
    if ((left_button_down || right_button_down) && (last_seen_lmb || last_seen_rmb))
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
            s("from:", mouse.x, " ", mouse.y, " to:", last_grabbed_mouse_position.x, " ",
                last_grabbed_mouse_position.y),
            1.0f);
        SDL_SetRelativeMouseMode(SDL_bool(false));
        SDL_WarpMouseInWindow(nullptr, last_grabbed_mouse_position.x, last_grabbed_mouse_position.y);
      }
    }
    // wrap x
    if (cam.theta > two_pi<float32>())
      cam.theta = cam.theta - two_pi<float32>();
    if (cam.theta < 0)
      cam.theta = two_pi<float32>() + cam.theta;
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (cam.phi > upper)
      cam.phi = upper;
    const float32 lower = 100 * epsilon<float32>();
    if (cam.phi < lower)
      cam.phi = lower;

    //+x right, +y forward, +z up

    // construct a matrix that represents a rotation around the z axis by
    // theta(x) radians
    mat4 rx = rotate(-cam.theta, vec3(0, 0, 1));

    //'default' position of camera is behind the character
    // rotate that vector by our rx matrix
    cam_rel = rx * normalize(vec4(0, -1, 0.0, 0));

    // perp is the camera-relative 'x' axis that moves with the camera
    // as the camera rotates around the character-centered y axis
    // should always point towards the right of the screen
    vec3 perp = vec3(-cam_rel.y, cam_rel.x, 0);

    // construct a matrix that represents a rotation around perp by -phi
    mat4 ry = rotate(-cam.phi, perp);

    // rotate the camera vector around perp
    cam_rel = normalize(ry * cam_rel);

    if (right_button_down)
    {
      player_dir = normalize(-vec3(cam_rel.x, cam_rel.y, 0));
    }
    if (is_pressed(SDL_SCANCODE_W))
    {
      vec3 v = vec3(player_dir.x, player_dir.y, 0.0f);
      player_pos += MOVE_SPEED * dt * v;
    }
    if (is_pressed(SDL_SCANCODE_S))
    {
      vec3 v = vec3(player_dir.x, player_dir.y, 0.0f);
      player_pos -= MOVE_SPEED * dt * v;
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(player_dir.x, player_dir.y, 0, 0);
      player_pos += MOVE_SPEED * dt * vec3(r * v);
    }
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(player_dir.x, player_dir.y, 0, 0);
      player_pos += MOVE_SPEED * dt * vec3(r * v);
    }
    cam.pos = player_pos + vec3(cam_rel.x, cam_rel.y, cam_rel.z) * cam.zoom;
    cam.dir = -vec3(cam_rel);
  }
  previous_mouse_state = mouse_state;
}

void Render_Test_State::update()
{
  const float32 height = 1.25;
  cube_star->scale = vec3(.85);
  cube_star->position = vec3(0.5 * cos(current_time / 10.f), 0, height);
  const float32 anglestar = wrap_to_range(pi<float32>() * (float32)sin(current_time / 2.f), 0.0f, 2.0f * pi<float32>());
  cube_star->propagate_visibility = true;
  cube_star->orientation = angleAxis(anglestar, normalize(vec3(cos(current_time * .2), sin(current_time * .2), 1)));

  const float32 planet_scale = 0.35;
  const float32 planet_distance = 4;
  const float32 planet_year = 5;
  const float32 planet_day = 1;
  cube_planet->scale = vec3(planet_scale);
  cube_planet->position = planet_distance * vec3(cos(current_time / planet_year), sin(current_time / planet_year), 0);
  const float32 angle = wrap_to_range((float32)current_time, 0.0f, 2.0f * pi<float32>());
  cube_planet->orientation = angleAxis((float32)current_time / planet_day, vec3(0, 0, 1));
  cube_planet->visible = sin(current_time * 6) > 0;
  cube_planet->propagate_visibility = false;

  const float32 moon_scale = 0.25;
  const float32 moon_distance = 1.5;
  const float32 moon_year = .75;
  const float32 moon_day = .1;
  cube_moon->scale = vec3(moon_scale);
  cube_moon->position = moon_distance * vec3(cos(current_time / moon_year), sin(current_time / moon_year), 0);
  cube_moon->orientation = angleAxis((float32)current_time / moon_day, vec3(0, 0, 1));

  sphere->position = vec3(-4, -2, 3.5);
  sphere->scale = vec3(1.0);

  auto &lights = scene.lights.lights;
  static bool first = true;
  if (false)
  {
    scene.lights.light_count = 4;

    lights[0].color = 150000.f * vec3(1.0, .95, 1.0);
    lights[0].cone_angle = 0.042; //+ 0.14*sin(current_time);lights[0].casts_shadows = true;
    lights[0].shadow_blur_iterations = 6;
    lights[0].shadow_blur_radius = 1.25005f;
    lights[0].max_variance = 0.0000002;
    lights[0].shadow_near_plane = 110.f;
    lights[0].shadow_far_plane = 350.f;
    lights[0].shadow_fov = radians(15.5f);
    lights[0].casts_shadows = true;
    lights[0].type = Light_Type::spot;

    lights[1].type = Light_Type::spot;
    lights[1].direction = vec3(0);
    lights[1].color = 150.f * vec3(1.0, 1.00, 1.0);
    lights[1].cone_angle = 0.151; //+ 0.14*sin(current_time);
    lights[1].ambient = 0.0004;
    lights[1].casts_shadows = true;
    lights[1].max_variance = 0.0000002;
    lights[1].shadow_near_plane = .5f;
    lights[1].shadow_far_plane = 200.f;
    lights[1].shadow_fov = radians(90.f);
    lights[1].shadow_blur_iterations = 4;
    lights[1].shadow_blur_radius = 2.12;

    lights[2].color = 111.f * vec3(1, 0.05, 1.05);
    lights[2].type = Light_Type::omnidirectional;
    lights[2].attenuation = vec3(1.0, 1.7, 2.4);
    lights[2].ambient = 0.0001f;

    first = false;
  }

  imgui_light_array(scene.lights);
  lights[1].position = vec3(5 * cos(current_time * .0172), 5 * sin(current_time * .0172), 2.);
  lights[2].position = vec3(3 * cos(current_time * .12), 3 * sin(.03 * current_time), 0.5);

  sun_light->position = lights[0].position;
  sun_light->scale = vec3(lights[0].radius);
  cone_light1->position = lights[1].position;
  cone_light1->scale = vec3(lights[1].radius);
  small_light->position = lights[2].position;
  small_light->scale = vec3(lights[2].radius);

  gun->orientation = angleAxis((float32)(.02f * current_time), vec3(0.f, 0.f, 1.f));
  tiger->orientation = angleAxis((float32)(.03f * current_time), vec3(0.f, 0.f, 1.f));
  tiger->scale = vec3(2, 0.5, 0.5f);

  skybox->scale = vec3(4000);
  skybox->position = vec3(0, 0, 0);
  skybox->visible = true;
}