#include "Render_Test_State.h"
#include "Globals.h"
#include "Render.h"
#include "State.h"
#include <atomic>
#include <memory>
#include <sstream>
#include <thread>
using namespace glm;

Render_Test_State::Render_Test_State(std::string name, SDL_Window *window,
                                     ivec2 window_size)
    : State(name, window, window_size)
{
  free_cam = true;

  Material_Descriptor material;
  material.albedo = "pebbles_diffuse.png";
  material.emissive = "";
  material.normal = "pebbles_normal.png";
  material.roughness = "pebbles_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(30);
  material.casts_shadows = true;
  material.backface_culling = false;
  ground = scene.add_primitive_mesh(cube, "world_cube", material);
  ground->position = {0.0f, 0.0f, -5.f};
  ground->scale = {40.0f, 40.0f, 10.f};



  Material_Descriptor sky_mat;
  sky_mat.uv_scale = vec3(1.f);
  sky_mat.backface_culling = false;
  sky_mat.albedo = "skybox.jpg";
  sky_mat.vertex_shader = "vertex_shader.vert";
  sky_mat.frag_shader = "passthrough.frag";
  sky_sphere = scene.add_aiscene("sphere.obj", &sky_mat);


  material.casts_shadows = true;
  material.uv_scale = vec2(4);
  sphere = scene.add_aiscene("sphere.obj", nullptr, &material);

  material.albedo = "crate_diffuse.png";
  material.emissive = "test_emissive.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(2);
  material.albedo_alpha_override = 128;
  material.uses_transparency = false;
  material.casts_shadows = true;

  cube_star = scene.add_primitive_mesh(cube, "star", material);
  cube_planet = scene.add_primitive_mesh(cube, "planet", material);
  scene.set_parent(cube_planet, cube_star, false);
  cube_moon = scene.add_primitive_mesh(cube, "moon", material);
  scene.set_parent(cube_moon, cube_planet, false);


  material.albedo_alpha_override = 0;
  material.uses_transparency = false;

  cam.phi = .25;
  cam.theta = -1.5f * half_pi<float32>();
  cam.pos = vec3(3.3, 2.3, 1.4);

  for (int y = -3; y < 3; ++y)
  {
    for (int x = -3; x < 3; ++x)
    {
      mat4 t = translate(vec3(x, y, 0.0));
      mat4 s = scale(vec3(0.25));
      mat4 basis = t * s;
      chests.push_back(scene.add_aiscene("Chest/Chest.obj", &basis));
    }
  }

  Material_Descriptor tiger_mat;
  tiger_mat.backface_culling = false;
  tiger = scene.add_aiscene("tiger/tiger.obj", &tiger_mat);
  tiger->position = vec3(-3, -3, 0);

  material.casts_shadows = false;
  material.albedo = "color(0,255,0,255)";
  material.emissive = "color(0,255,0,255)";
  cone_light1 = scene.add_aiscene("sphere.obj", &material);
  cone_light1->name = "conelight1";

  material.albedo = "color(255,255,255,255)";
  material.emissive = "color(255,255,120,255)";
  sun_light = scene.add_aiscene("sphere.obj", &material);
  sun_light->name = "sun";

  material.albedo = "color(0,0,0,255)";
  material.emissive = "color(255,0,255,255)";
  small_light = scene.add_aiscene("sphere.obj", &material);


}

void Render_Test_State::handle_input(State **current_state,
                                     std::vector<State *> available_states)
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
    }
    else if (_e.type == SDL_KEYUP)
    {
      if (_e.key.keysym.sym == SDLK_F5)
      {
        free_cam = !free_cam;
      }
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
    }
    else if (_e.type == SDL_MOUSEWHEEL)
    {
      if (_e.wheel.y < 0)
        cam.zoom += ZOOM_STEP;
      else if (_e.wheel.y > 0)
        cam.zoom -= ZOOM_STEP;
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

  if (free_cam)
  {
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
      cam.pos += MOVE_SPEED * cam.dir;
    if (is_pressed(SDL_SCANCODE_S))
      cam.pos -= MOVE_SPEED * cam.dir;
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
    { // won't track mouse delta that happened when mouse button was not pressed
      cam.theta += mouse_delta.x * MOUSE_X_SENS;
      cam.phi += mouse_delta.y * MOUSE_Y_SENS;
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
      player_pos += MOVE_SPEED * v;
    }
    if (is_pressed(SDL_SCANCODE_S))
    {
      vec3 v = vec3(player_dir.x, player_dir.y, 0.0f);
      player_pos -= MOVE_SPEED * v;
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(player_dir.x, player_dir.y, 0, 0);
      player_pos += MOVE_SPEED * vec3(r * v);
    }
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(player_dir.x, player_dir.y, 0, 0);
      player_pos += MOVE_SPEED * vec3(r * v);
    }
    cam.pos = player_pos + vec3(cam_rel.x, cam_rel.y, cam_rel.z) * cam.zoom;
    cam.dir = -vec3(cam_rel);
  }
  previous_mouse_state = mouse_state;
}

void Render_Test_State::update()
{

  const float32 height = 1.25;

  cube_star->scale = vec3(.85); // +0.65f*vec3(sin(current_time*.2));
  cube_star->position = vec3(0.5 * cos(current_time / 10.f), 0, height);
  const float32 anglestar = wrap_to_range(pi<float32>() * sin(current_time / 2),
                                          0, 2 * pi<float32>());
  //cube_star->visible = sin(current_time * 1.2) > -.25;
  cube_star->propagate_visibility = true;
  cube_star->orientation = angleAxis(anglestar,
   normalize(vec3(cos(current_time*.2), sin(current_time*.2), 1)));

  const float32 planet_scale = 0.35;
  const float32 planet_distance = 4;
  const float32 planet_year = 5;
  const float32 planet_day = 1;
  cube_planet->scale = vec3(planet_scale);
  cube_planet->position =
      planet_distance *
      vec3(cos(current_time / planet_year), sin(current_time / planet_year), 0);
  const float32 angle = wrap_to_range(current_time, 0, 2 * pi<float32>());
  cube_planet->orientation =
      angleAxis((float32)current_time / planet_day, vec3(0, 0, 1));
  cube_planet->visible = sin(current_time * 6) > 0;
  cube_planet->propagate_visibility = false;

  const float32 moon_scale = 0.25;
  const float32 moon_distance = 1.5;
  const float32 moon_year = .75;
  const float32 moon_day = .1;
  cube_moon->scale = vec3(moon_scale);
  cube_moon->position = moon_distance * vec3(cos(current_time / moon_year),
                                             sin(current_time / moon_year), 0);
  cube_moon->orientation =
      angleAxis((float32)current_time / moon_day, vec3(0, 0, 1));

  sphere->position = vec3(-3, 3, 1.5);
  sphere->scale = vec3(0.4);

  sky_sphere->scale = vec3(500);
  sky_sphere->position = vec3(0, 0, -350);
  sky_sphere->visible = false;



  auto &lights = scene.lights.lights;
  scene.lights.light_count = 4;
  //float32 time_of_day = 12.;
  const vec4 night = vec4(0.015f,0.015f,0.015f,0.f);
  const vec4 day = vec4(14.f / 255.f, 155.f / 255.f, 1.,0.f);
  const vec4 sun_low = vec4(235.f/255.f, 28.f / 255., 0.f / 255.f, 0.f);
  const float time_day_scale = 0.12f;
  float32 time_of_day = wrap_to_range((time_day_scale*current_time)+135.f, 0.0f, 24.0f);
  //time_of_day = 12.f;
  float32 day_range = clamp(time_of_day, 5.85f, 18.85f);//5:30am to 7:30pm
  float32 day_t = (day_range-5.85f) / 12.7f; //0-1 daytime

  Bezier_Curve time_curve;
  time_curve.points = { vec4(1.0),vec4(0.0),vec4(1.0) };
  vec4 sky_color_t = time_curve.lerp(day_t);
  

  float parabola_day_t =  0.5f*(1.0f+pow((day_t-0.5f)*2.f, 15.f));

  Bezier_Curve sky_curve;
  sky_curve.points = { night,1.3f*sun_low,2.6f*day,1.3f*sun_low,night };
  vec4 sky_color = sky_curve.lerp(clamp(parabola_day_t,0.f,1.f));
  clear_color = vec3(clamp(sky_color.r,0.f,1.f), clamp(sky_color.g,0.f,1.f), clamp(sky_color.b,0.f,1.f));
   
  //clear_color = day_t*vec3(1);
  scene.lights.additional_ambient = vec3(sky_color.r, sky_color.g, sky_color.b)*vec3(0.03) + vec3(94.f / 255.f, 155.f / 255.f, 1.) * vec3(0.01);

  
  scene.lights.light_count = 3;

  vec3 sun_pos = 180.f* vec3(cos(two_pi<float32>()*((time_of_day-6.f)/24.f)),0,sin(two_pi<float32>()*((time_of_day-6.f) / 24.f)));
  sun_light->position = sun_pos;
  sun_light->scale = vec3(10.);
  lights[0].position = sun_pos;
  lights[0].type = Light_Type::spot;
  lights[0].direction = vec3(0);
  lights[0].color = 230000.f * vec3(1.0, .95, 1.0);
  lights[0].cone_angle = 0.042; //+ 0.14*sin(current_time);
  lights[0].ambient = 0.003*clamp(sin(two_pi<float32>()*((time_of_day - 6.f) / 24.f)),0.f,1.f);
  lights[0].casts_shadows = true;
  lights[0].shadow_blur_iterations = 6;
  lights[0].shadow_blur_radius = 1.25005f;
  lights[0].max_variance = 0.0000002;
  lights[0].shadow_near_plane = 110.f;
  lights[0].shadow_far_plane = 350.f;
  lights[0].shadow_fov = radians(15.5f);


  lights[1].position =
      vec3(5 * cos(current_time * .0172), 5 * sin(current_time * .0172),2.);
  lights[1].type = Light_Type::spot;
  lights[1].direction = vec3(0);
  lights[1].color = 200.f * vec3(1.10, 1.10, 1.0);
  lights[1].cone_angle = 0.151;//+ 0.14*sin(current_time);
  lights[1].ambient = 0.0004;
  lights[1].casts_shadows = true;
  lights[1].max_variance = 0.0000002;
  lights[1].shadow_near_plane = .5f;
  lights[1].shadow_far_plane = 200.f;
  lights[1].shadow_fov = radians(90.f);
  lights[1].shadow_blur_iterations = 4;
  lights[1].shadow_blur_radius = 2.12;
  cone_light1->position = lights[1].position + 0.5f*normalize(lights[1].position);
  cone_light1->scale = vec3(.25);

  lights[2].position =
      vec3(3 * cos(current_time * .12), 3 * sin(.03 * current_time), 0.5);
  lights[2].color = 31.f * vec3(1, 0.05, 1.05);
  lights[2].type = Light_Type::omnidirectional;
  lights[2].attenuation = vec3(1.0, 1.7, 2.4);
  lights[2].ambient = 0.0001f;
  small_light->position = lights[2].position;
  small_light->scale = vec3(0.1);


}