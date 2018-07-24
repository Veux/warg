#include "stdafx.h"
#include "Render_Test_State.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "State.h"
#include "Third_party/imgui/imgui.h"
using namespace glm;

Render_Test_State::Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{
  free_cam = true;

  Material_Descriptor material;
  // test_spheres(scene);

  gun = scene.add_aiscene("gun", "Cerberus/cerberus-warg.FBX");
  scene.nodes[gun].position = vec3(4.0f, -3.0f, 2.0f);
  scene.nodes[gun].scale = vec3(5);

  // ground
  material.albedo = "grass_albedo.png";
  material.emissive = "";
  material.normal = "ground1_normal.png";
  material.roughness = "color(.7,.7,.7,1.0)";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(12);
  material.casts_shadows = true;
  material.backface_culling = false;
  ground = scene.add_mesh(cube, "world_cube", &material);
  scene.nodes[ground].position = {0.0f, 0.0f, -0.5f};
  scene.nodes[ground].scale = {35.0f, 35.0f, 1.f};
  material.albedo.mod = vec4(1);
  // sky
  Material_Descriptor sky_mat;
  sky_mat.backface_culling = false;
  sky_mat.vertex_shader = "vertex_shader.vert";
  sky_mat.frag_shader = "skybox.frag";
  skybox = scene.add_mesh(cube, "skybox", &sky_mat);

  // sphere
  material.casts_shadows = true;
  material.uv_scale = vec2(4);
  material.albedo = "color(1,1,1,1)";
  material.normal = "steel_normal.png";
  material.roughness = "steel_roughness.png";
  material.roughness.mod = vec4(.02);
  material.metalness = "color(1,1,1,1)";
  sphere = scene.add_aiscene("sphere", "smoothsphere.fbx", &material);
  scene.nodes[sphere].position = vec3(-4, -2, 3.5);
  scene.nodes[sphere].scale = vec3(1.0);
  material.roughness.mod = vec4(1);

  // testobjects = scene.add_aiscene("testobjects.fbx", nullptr, &material);
  // testobjects->position = vec3(-5, 5, 1);

  //// crates:
  // material.albedo = "crate_albedo.png";
  // material.emissive = "test_emissive.png";
  // material.normal = "color(0.5,.5,1,0)";
  // material.roughness = "crate_roughness.png";
  // material.metalness = "crate_metalness.png";
  // material.vertex_shader = "vertex_shader.vert";
  // material.frag_shader = "fragment_shader.frag";
  // material.uv_scale = vec2(1);
  // material.uses_transparency = false;
  // material.casts_shadows = true;

  // cubes
  material.albedo = s(Conductor_Reflectivity::gold);
  grabbycube = scene.add_mesh(cube, "grabbycube", &material);
  cube_planet = scene.add_mesh(cube, "planet", &material);
  scene.set_parent(cube_planet, grabbycube);
  cube_moon = scene.add_mesh(cube, "moon", &material);
  scene.set_parent(cube_moon, cube_planet);
  shoulder_joint = scene.new_node();
  arm_test = scene.add_mesh(cube, "world_cube", &material);

  scene.set_parent(shoulder_joint, grabbycube);
  scene.set_parent(arm_test, shoulder_joint);

  // camera spawn
  cam.phi = .25;
  cam.theta = -1.5f * half_pi<float32>();
  cam.pos = vec3(3.3, 2.3, 1.4);

  // tigers
  Material_Descriptor tiger_mat;
  tiger_mat.backface_culling = false;
  tiger_mat.discard_on_alpha = true;
  tiger = scene.add_aiscene("tiger", "tiger/tiger.fbx", &tiger_mat);
  scene.nodes[tiger].position = vec3(-6, -3, 0.0);
  scene.nodes[tiger].scale = vec3(1.0);

  tiger1 = scene.add_aiscene("tiger1", "tiger/tiger.fbx", &tiger_mat);
  scene.set_parent(tiger1, grabbycube);

  tiger2 = scene.add_aiscene("tiger2", "tiger/tiger.fbx", &tiger_mat);
  scene.set_parent(tiger2, grabbycube);

  // Node_Index tiger3 = scene.add_aiscene("tiger3", "tiger/tiger.fbx", &tiger_mat);
  // scene.nodes[tiger3].position = vec3(0, 0, 0.5);
  // scene.nodes[tiger3].scale = vec3(0.45f);
  // scene.set_parent(tiger3, cube_moon);

  // light spheres
  material.casts_shadows = false;
  material.albedo = "color(0,0,0,1)";
  material.emissive = "color(11,11,11,1)";
  material.roughness = "color(1,1,1,1)";
  material.metalness = "color(0,0,0,0)";
  Node_Index temp = scene.add_aiscene("cone_light1", "smoothsphere.fbx", &material);
  cone_light1 = scene.nodes[scene.nodes[temp].children[0]].children[0];
  scene.drop(cone_light1);
  scene.delete_node(temp);

  material.albedo = "color(0,0,0,1)";
  material.emissive = "color(3,3,1.5,1)";
  temp = scene.add_aiscene("sun_light", "smoothsphere.fbx", &material);
  sun_light = scene.nodes[scene.nodes[temp].children[0]].children[0];
  scene.drop(sun_light);
  scene.delete_node(temp);

  material.albedo = "color(0,0,0,1)";
  material.emissive = "color(1.15,0,1.15,1)";
  temp = scene.add_aiscene("small_light", "smoothsphere.fbx", &material);
  small_light = scene.nodes[scene.nodes[temp].children[0]].children[0];
  scene.drop(small_light);
  scene.delete_node(temp);

  Particle_Emitter_Descriptor ped;
  Mesh_Index mesh_index;
  Material_Index material_index;
  material.vertex_shader = "instance.vert";
  material.frag_shader = "emission.frag";
  material.emissive = "color(1,1,1,1)";
  material.emissive.mod = vec4(15.f, 3.3f, .7f, 1.f);
  Node_Index particle_node = scene.add_mesh(cube, "particle", &material);
  mesh_index = scene.nodes[particle_node].model[0].first;
  material_index = scene.nodes[particle_node].model[0].second;

  ped.emission_descriptor.initial_scale = vec3(0.035f);
  ped.emission_descriptor.initial_scale_variance = vec3(0.01f);
  ped.emission_descriptor.initial_velocity = vec3(0, 0, 2);
  ped.emission_descriptor.initial_velocity_variance = vec3(2, 2, 1);
  ped.emission_descriptor.particles_per_second = 12115.f;
  ped.emission_descriptor.time_to_live = .55f;
  ped.emission_descriptor.time_to_live_variance = 1.05f;

  ped.physics_descriptor.type = wind;
  ped.physics_descriptor.intensity = 20.f;
  scene.nodes[particle_node].visible = false;
  scene.particle_emitters.push_back(Particle_Emitter(ped, mesh_index, material_index));
  scene.lights.light_count = 1;
  scene.lights.lights[0].color = vec3(1.0f, .1f, .1f);
  scene.lights.lights[0].position.z = 1.0f;
  scene.lights.lights[0].ambient = 0.002f;
  scene.lights.lights[0].attenuation.z = 0.06f;
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
      if (block_kb)
        continue;
      if (_e.key.keysym.sym == SDLK_ESCAPE)
      {
        running = false;
        return;
      }
    }
    else if (_e.type == SDL_KEYUP)
    {
      if (block_kb)
        continue;
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
      if (block_mouse)
        continue;
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

  scene.nodes[skybox].position = cam.pos;

  const float32 height = 1.25;

  const float32 planet_scale = 0.35;
  const float32 planet_distance = 4;
  const float32 planet_year = 5;
  const float32 planet_day = 1;
  scene.nodes[cube_planet].scale = vec3(planet_scale);
  scene.nodes[cube_planet].position =
      planet_distance * vec3(cos(current_time / planet_year), sin(current_time / planet_year), 0);
  const float32 angle = wrap_to_range((float32)current_time, 0.0f, 2.0f * pi<float32>());
  scene.nodes[cube_planet].orientation = angleAxis((float32)current_time / planet_day, vec3(0, 0, 1));
  scene.nodes[cube_planet].visible = sin(current_time * 6) > 0;
  scene.nodes[cube_planet].propagate_visibility = false;

  const float32 moon_scale = 0.25;
  const float32 moon_distance = 1.5;
  const float32 moon_year = .75;
  const float32 moon_day = .1;
  scene.nodes[cube_moon].scale = vec3(moon_scale);
  scene.nodes[cube_moon].position =
      moon_distance * vec3(cos(current_time / moon_year), sin(current_time / moon_year), 0);
  scene.nodes[cube_moon].orientation = angleAxis((float32)current_time / moon_day, vec3(0, 0, 1));

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

  lights[1].position = vec3(5 * cos(current_time * .0172), 5 * sin(current_time * .0172), 2.);
  lights[2].position = vec3(3 * cos(current_time * .12), 3 * sin(.03 * current_time), 0.5);

  scene.nodes[sun_light].position = lights[0].position;
  scene.nodes[sun_light].scale = vec3(lights[0].radius);
  scene.nodes[cone_light1].position = lights[1].position;
  scene.nodes[cone_light1].scale = vec3(lights[1].radius);
  scene.nodes[small_light].position = lights[2].position;
  scene.nodes[small_light].scale = vec3(lights[2].radius);

  Material_Index light0_mat = scene.nodes[sun_light].model[0].second;
  Material_Descriptor *md0 = scene.resource_manager->retrieve_pool_material_descriptor(light0_mat);
  vec3 c0 = lights[0].brightness * lights[0].color;
  md0->emissive.name = s("color(", c0.r, ",", c0.g, ",", c0.b, ",", 1, ")");
  scene.push_material_change(light0_mat);

  Material_Index light1_mat = scene.nodes[cone_light1].model[0].second;
  Material_Descriptor *md1 = scene.resource_manager->retrieve_pool_material_descriptor(light1_mat);
  vec3 c1 = lights[1].brightness * lights[1].color;
  md1->emissive.name = s("color(", c1.r, ",", c1.g, ",", c1.b, ",", 1, ")");
  scene.push_material_change(light0_mat);

  Material_Index light2_mat = scene.nodes[small_light].model[0].second;
  Material_Descriptor *md2 = scene.resource_manager->retrieve_pool_material_descriptor(light2_mat);
  vec3 c2 = lights[2].brightness * lights[2].color;
  md2->emissive.name = s("color(", c2.r, ",", c2.g, ",", c2.b, ",", 1, ")");
  scene.push_material_change(light2_mat);

  // scene.nodes[gun].orientation = angleAxis((float32)(.02f * current_time), vec3(0.f, 0.f, 1.f));
  scene.nodes[tiger].orientation = angleAxis((float32)(.03f * current_time), vec3(0.f, 0.f, 1.f));
  scene.nodes[skybox].scale = vec3(4000);
  scene.nodes[skybox].position = vec3(0, 0, 0);
  scene.nodes[skybox].visible = true;
  scene.nodes[tiger2].position = vec3(.35 * cos(3 * current_time), .35 * sin(3 * current_time), 1);
  scene.nodes[tiger2].scale = vec3(.25);

  // build_transformation transfer child test
  static float32 last = (float32)current_time;
  static float32 time_of_reset = 0.f;
  static bool is_world = false;
  static bool first1 = true;

  if (first1)
  {
    first1 = false;
    scene.copy_all_materials_to_new_pool_slots(grabbycube);
  }
  if (last < current_time - 3)
  {
    if (!is_world)
    {
      scene.set_parent(tiger2, grabbycube);

      scene.drop(tiger1);
      Material_Index material_index = scene.nodes[arm_test].model[0].second;
      Material_Descriptor *md = scene.resource_manager->retrieve_pool_material_descriptor(material_index);
      md->albedo.mod = vec4(1, 0, 0, 1);
      scene.push_material_change(material_index);
      // alternatively we could just have one resource manager function that pushes all the descriptors into
      // the pool at once, called at prepare_renderer every frame

      last = (float32)current_time;
      is_world = true;
      if (time_of_reset < current_time - 15)
      {
        set_message(s("resetting tiger to origin"), "", 3.f);
        scene.nodes[tiger1].position = vec3(0);
        scene.nodes[tiger1].scale = vec3(1);
        scene.nodes[tiger1].orientation = {};
        scene.nodes[tiger1].import_basis = mat4(1);
        time_of_reset = (float32)current_time;
      }
    }
    else
    {
      Material_Index material_index = scene.nodes[arm_test].model[0].second;
      Material_Descriptor *md = scene.resource_manager->retrieve_pool_material_descriptor(material_index);
      md->albedo.mod = vec4(0, 1, 0, 1);
      scene.push_material_change(material_index);

      set_message(s("arm_test GRAB"), "", 1.5f);
      scene.grab(arm_test, tiger1);

      scene.set_parent(tiger2, sphere);

      is_world = false;
      last = (float32)current_time;
    }
  }

  const float32 theta = (float32)sin(current_time / 12.f);
  float cube_diameter = 1;

  scene.nodes[grabbycube].scale_vertex = vec3(1, 1, 2); // the size of the cube - affects *only* this object
  scene.nodes[grabbycube].scale = vec3(cube_diameter);  // the scale of the node - affects all children
  scene.nodes[grabbycube].position = vec3(0, 0, height + 3.f * (0.5 + 0.5 * (sin(current_time))));
  scene.nodes[grabbycube].orientation = angleAxis(theta, vec3(cos(.25f * current_time), sin(.25f * current_time), 0));

  scene.nodes[shoulder_joint].position = {0.50f * cube_diameter, 0.0f, cube_diameter};
  scene.nodes[shoulder_joint].orientation = angleAxis(20.f * (float32)sin(current_time / 20.f), vec3(1, 0, 0));

  const float32 arm_radius = 0.25f;
  scene.nodes[arm_test].scale_vertex = {arm_radius, arm_radius, 1.5f};
  scene.nodes[arm_test].position = {0.5f * arm_radius, 0.0f, -0.75f};

  renderer.set_camera(cam.pos, cam.dir);
  Particle_Emitter *pe = &scene.particle_emitters.back();
  // pe->descriptor.position = vec3(5.f * sin(current_time), 5.f * cos(current_time), 1.0f);

  glm::mat4 m = scene.build_transformation(arm_test);
  quat orientation;
  vec3 translation;
  decompose(m, vec3(), orientation, translation, vec3(), vec4());
  orientation = conjugate(orientation);
  // pe->descriptor.physics_descriptor.gravity = vec3(0);
  // pe->descriptor.orientation = orientation;
  //// pe->descriptor.position = translation;

  pe->descriptor.emission_descriptor.initial_velocity = vec3(0, 0, 2);
  pe->descriptor.emission_descriptor.initial_velocity_variance =
      vec3(5.5f * sin(5 * current_time), 5.5f * cos(4 * current_time), 2);
  pe->descriptor.emission_descriptor.initial_position_variance = vec3(3, 3, .1);
  pe->update(renderer.projection, renderer.camera, dt);
  pe->descriptor.physics_descriptor.intensity = random_between(11.f, 35.f);
  static vec3 dir;
  if (fract(sin(current_time)) > .5)
    dir = random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

  pe->descriptor.physics_descriptor.direction = dir;

  scene.lights.lights[0].brightness = random_between(47.f, 55.f);
  scene.draw_imgui();
}

void test_spheres(Flat_Scene_Graph &scene)
{
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
  uint32 count = 0;
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

        Node_Index index = scene.add_aiscene("Spherearray", "smoothsphere.fbx", &material);
        Flat_Scene_Graph_Node *node = &scene.nodes[index];
        node->scale = vec3(0.5f);
        node->position = (node->scale * 2.f * vec3(i, k, j)) + vec3(0, 6, 1);
        scene.set_parent(index, NODE_NULL);
        ++count;
      }
    }
  }
}
