#include "stdafx.h"
#include "Render_Test_State.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "State.h"
#include "Animation_Utilities.h"
using namespace glm;
void test_spheres(Flat_Scene_Graph &scene);

void world_water_settings(Uniform_Set_Descriptor *dst)
{

  dst->float32_uniforms["index_of_refraction"] = 1.15f;
  dst->float32_uniforms["refraction_offset_factor"] = 0.01501;
  dst->float32_uniforms["water_eps"] = 0.001;
  dst->float32_uniforms["water_scale"] = .87;
  dst->float32_uniforms["water_speed"] = 1.401;
  dst->float32_uniforms["water_dist_scale"] = .81;
  dst->float32_uniforms["water_dist_exp"] = 1.67;
  dst->float32_uniforms["water_height_scale"] = 3.0;
  dst->float32_uniforms["water_dist_min"] = 5.;
  dst->float32_uniforms["water_scale2"] = 0.052;
  dst->float32_uniforms["height_scale2"] = 3.0;
  dst->float32_uniforms["water_time2"] = 0.192;
  dst->float32_uniforms["surfdotv_exp"] = 2.35f;
  dst->bool_uniforms["water_use_uv"] = false;

  // if (imgui_this_tick)
  //{
  //  ImGui::DragFloat("ior", &ior, 0.001f, -5.f, 5.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("refraction_offset_factor", &offset_factor, 0.0001f, .000001f, 2.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("water_eps", &water_eps, 0.00001f, .0000001f, 2.f, "%.8f", 2.0f);
  //  ImGui::DragFloat("water_scale", &water_scale, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("water_speed", &water_speed, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("water_dist_min", &water_dist_min, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("water_dist_scale", &water_dist_scale, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("water_dist_exp", &water_dist_exp, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("water_height_scale", &water_height_scale, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("water_scale2", &water_scale2, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("height_scale2", &height_scale2, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("water_time2", &water_time2, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::DragFloat("surfdotv_exp", &surfdotv_exp, 0.001f, -51.f, 51.f, "%.4f", 2.0f);
  //  ImGui::Checkbox("do_extra_water", &do_extra_water);
  //}
}

void small_object_water_settings(Uniform_Set_Descriptor *dst)
{
  dst->float32_uniforms["index_of_refraction"] = 1.15f;
  dst->float32_uniforms["refraction_offset_factor"] = 0.02501;
  dst->float32_uniforms["water_eps"] = 0.0001;
  dst->float32_uniforms["water_scale"] = 4.87;
  dst->float32_uniforms["water_speed"] = 0.201;
  dst->float32_uniforms["water_dist_scale"] = .81;
  dst->float32_uniforms["water_dist_exp"] = 1.67;
  dst->float32_uniforms["water_height_scale"] = 3.0;
  dst->float32_uniforms["water_dist_min"] = 5.;
  dst->float32_uniforms["water_scale2"] = 1.52;
  dst->float32_uniforms["height_scale2"] = 3.0;
  dst->float32_uniforms["water_time2"] = 0.0192;
  dst->float32_uniforms["surfdotv_exp"] = 2.35f;
  dst->bool_uniforms["water_use_uv"] = true;
}

void small_object_refraction_settings(Uniform_Set_Descriptor *dst)
{
  dst->float32_uniforms["index_of_refraction"] = 1.15f;
  dst->float32_uniforms["refraction_offset_factor"] = 0.201;
}

Render_Test_State::Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{
  free_cam = true;

  scene.initialize_lighting("Assets/Textures/Environment_Maps/GrandCanyon_C_YumaPoint/GCanyon_C_YumaPoint_8k.jpg",
      "Assets/Textures/Environment_Maps/GrandCanyon_C_YumaPoint/irradiance.hdr");

  Material_Descriptor material;
  test_spheres(scene);

  gun = scene.add_aiscene("Cerberus/cerberus-warg.FBX", "gun");
  scene.nodes[gun].position = vec3(4.0f, -3.0f, 2.0f);
  scene.nodes[gun].scale = vec3(1);

  // ground
  material.albedo = "grass_albedo.png";
  // material.emissive = "";
  material.normal = "ground1_normal.png";
  material.roughness.mod = vec4(.7);
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(12);
  material.casts_shadows = true;
  material.backface_culling = false;
  ground = scene.add_mesh(cube, "world_cube", &material);
  scene.nodes[ground].position = {0.0f, 0.0f, -150.f};
  scene.nodes[ground].scale = {35.0f, 35.0f, 300.f};
  material.uv_scale = vec2(312);
  material.albedo.mod = vec4(2);
  underwater = scene.add_mesh(cube, "underwater", &material);
  scene.nodes[underwater].position = {0.0f, 0.0f, -30.5f};
  scene.nodes[underwater].scale = {6500.0f, 6500.0f, 1.f};
  material.albedo.mod = vec4(1);

  // sphere
  material.casts_shadows = true;
  material.uv_scale = vec2(4);
  material.normal = "steel_normal.png";
  material.roughness = "steel_roughness.png";
  material.roughness.mod = vec4(.02);
  sphere = scene.add_aiscene("smoothsphere.fbx", "sphere");
  Material_Index mi = scene.resource_manager->push_custom_material(&material);
  scene.nodes[scene.nodes[sphere].children[0]].model[0].second = mi;
  scene.nodes[sphere].position = vec3(-4, -2, 3.5);
  scene.nodes[sphere].scale = vec3(1, 1, 1);

  // water
  material.emissive.mod = vec4(0, 0, 0.005, 1);
  material.albedo.mod = vec4(.036, .125, .3, .995);
  material.uses_transparency = true;
  material.uv_scale = vec2(1);
  material.roughness.mod = vec4(0.25);
  material.metalness.mod = vec4(0.84);
  material.frag_shader = "water.frag";
  world_water_settings(&material.uniform_set);
  memewater = scene.add_mesh(cube, "memewater", &material);
  scene.nodes[memewater].scale = vec3(6000, 6000, 3);
  scene.nodes[memewater].position = vec3(0, 0, -1.5);

  material.uses_transparency = false;
  material.roughness.mod = vec4(1);
  material.uniform_set.clear();
  material.frag_shader = "fragment_shader.frag";

  // sphere_raycast_test
  material.emissive.mod = vec4(10, .5, .5, 1);
  sphere_raycast_test = scene.add_aiscene("smoothsphere.fbx", "sphere_raycast_test");
  Material_Index mi2 = scene.resource_manager->push_custom_material(&material);
  scene.nodes[scene.nodes[sphere_raycast_test].children[0]].model[0].second = mi2;

  scene.nodes[sphere_raycast_test].scale = vec3(0.005);
  material.emissive.mod = MISSING_TEXTURE_MOD;

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
  camera.phi = .25;
  camera.theta = -1.5f * half_pi<float32>();
  camera.pos = vec3(3.3, 2.3, 1.4);

  // tigers
  Material_Descriptor tiger_mat;
  tiger_mat.backface_culling = true;
  tiger_mat.discard_on_alpha = true;
  tiger = scene.add_aiscene("tiger/tiger.fbx", "tiger");
  Material_Descriptor *tigermat = scene.get_modifiable_material_pointer_for(scene.nodes[tiger].children[0], 0);
  tigermat->normal_uv_scale = vec2(22);
  scene.nodes[tiger].position = vec3(-6, -3, 0.0);
  scene.nodes[tiger].scale = vec3(1.0);

  tiger1 = scene.add_aiscene("tiger/tiger.fbx", "tiger1");
  tigermat = scene.get_modifiable_material_pointer_for(scene.nodes[tiger1].children[0], 0);
  tigermat->normal_uv_scale = vec2(22);
  scene.set_parent(tiger1, grabbycube);

  tiger2 = scene.add_aiscene("tiger/tiger.fbx", "tiger2");
  tigermat = scene.get_modifiable_material_pointer_for(scene.nodes[tiger2].children[0], 0);
  tigermat->normal_uv_scale = vec2(22);
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

  scene.particle_emitters.push_back(Particle_Emitter());
  Particle_Emitter *pe = &scene.particle_emitters.back();
  material.vertex_shader = "instance.vert";
  material.frag_shader = "fireparticle.frag";
  material.backface_culling = false;
  // material.emissive = "color(1,1,1,1)";
  // material.emissive.mod = vec4(15.f, 3.3f, .7f, 1.f);
  fire_particle = scene.add_mesh(plane, "firep particle", &material);
  pe->mesh_index = scene.nodes[fire_particle].model[0].first;
  pe->material_index = scene.nodes[fire_particle].model[0].second;
  scene.nodes[fire_particle].visible = false;

  auto &lights = scene.lights.lights;

  scene.lights.light_count = 3;
  Light *light0 = &scene.lights.lights[0];
  light0->position = vec3(804.00000, -414.00000, 401.00000);
  light0->direction = vec3(0.00000, 0.00000, 0.00000);
  light0->brightness = 2500.00000;
  light0->color = vec3(1.00000, 0.95000, 1.00000);
  light0->attenuation = vec3(1.00000, 0.22000, 0.00000);
  light0->ambient = 0.00001;
  light0->radius = 22.00000;
  light0->cone_angle = 5.00000;
  light0->type = Light_Type::spot;
  light0->casts_shadows = 1;
  light0->shadow_blur_iterations = 3;
  light0->shadow_blur_radius = 0.30050;
  light0->shadow_near_plane = 963.00006;
  light0->shadow_far_plane = 1127.00000;
  light0->max_variance = 0.00000;
  light0->shadow_fov = 0.06230;
  light0->shadow_map_resolution = ivec2(2048, 2048);

  lights[1].type = Light_Type::spot;
  lights[1].direction = vec3(0);
  lights[1].color = vec3(1.0, 1.00, 1.0);
  lights[1].brightness = 150.f;
  lights[1].cone_angle = 0.151; //+ 0.14*sin(current_time);
  lights[1].ambient = 0.0000;
  lights[1].casts_shadows = true;
  lights[1].max_variance = 0.0000002;
  lights[1].shadow_near_plane = .5f;
  lights[1].shadow_far_plane = 200.f;
  lights[1].shadow_fov = radians(90.f);
  lights[1].shadow_blur_iterations = 4;
  lights[1].shadow_blur_radius = 2.12;

  lights[2].color = vec3(1, 0.05, 1.05);
  lights[2].brightness = 111.f;
  lights[2].type = Light_Type::omnidirectional;
  lights[2].attenuation = vec3(1.0, 1.7, 2.4);
  lights[2].ambient = 0.0000f;
}

void Render_Test_State::handle_input_events()
{
  if (!recieves_input)
    return;

  auto is_pressed = [this](int key) { return key_state[key]; };

  for (auto &_e : events_this_tick)
  {

    if (_e.type == SDL_KEYDOWN)
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
        mouse_relative_mode = free_cam;
      }
    }
    else if (_e.type == SDL_MOUSEWHEEL)
    {
      if (_e.wheel.y < 0)
        camera.zoom += ZOOM_STEP;
      else if (_e.wheel.y > 0)
        camera.zoom -= ZOOM_STEP;
    }
  }

  set_message("mouse position:", s(cursor_position.x, " ", cursor_position.y), 1.0f);
  set_message("mouse grab position:", s(last_grabbed_cursor_position.x, " ", last_grabbed_cursor_position.y), 1.0f);

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
  bool last_seen_lmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool last_seen_rmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

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
      camera.theta = camera.theta - two_pi<float32>();
    if (camera.theta < 0)
      camera.theta = two_pi<float32>() + camera.theta;
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (camera.phi > upper)
      camera.phi = upper;
    const float32 lower = -half_pi<float32>() + 100 * epsilon<float32>();
    if (camera.phi < lower)
      camera.phi = lower;

    mat4 rx = rotate(-camera.theta, vec3(0, 0, 1));
    vec4 vr = rx * vec4(0, 1, 0, 0);
    vec3 perp = vec3(-vr.y, vr.x, 0);
    mat4 ry = rotate(camera.phi, perp);
    camera.dir = normalize(vec3(ry * vr));

    if (is_pressed(SDL_SCANCODE_W))
      camera.pos += MOVE_SPEED * dt * camera.dir;
    if (is_pressed(SDL_SCANCODE_S))
      camera.pos -= MOVE_SPEED * dt * camera.dir;
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.x, vr.y, 0, 0);
      camera.pos += vec3(MOVE_SPEED * dt * r * v);
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.x, vr.y, 0, 0);
      camera.pos += vec3(MOVE_SPEED * dt * r * v);
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
      camera.theta = camera.theta - two_pi<float32>();
    if (camera.theta < 0)
      camera.theta = two_pi<float32>() + camera.theta;
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (camera.phi > upper)
      camera.phi = upper;
    const float32 lower = 100 * epsilon<float32>();
    if (camera.phi < lower)
      camera.phi = lower;

    //+x right, +y forward, +z up

    // construct a matrix that represents a rotation around the z axis by
    // theta(x) radians
    mat4 rx = rotate(-camera.theta, vec3(0, 0, 1));

    //'default' position of camera is behind the character
    // rotate that vector by our rx matrix
    cam_rel = rx * normalize(vec4(0, -1, 0.0, 0));

    // perp is the camera-relative 'x' axis that moves with the camera
    // as the camera rotates around the character-centered y axis
    // should always point towards the right of the screen
    vec3 perp = vec3(-cam_rel.y, cam_rel.x, 0);

    // construct a matrix that represents a rotation around perp by -phi
    mat4 ry = rotate(-camera.phi, perp);

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
    camera.pos = player_pos + vec3(cam_rel.x, cam_rel.y, cam_rel.z) * camera.zoom;
    camera.dir = -vec3(cam_rel);
  }
  previous_mouse_state = mouse_state;
}

void Render_Test_State::update()
{

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

  lights[1].position = vec3(5 * cos(current_time * .0172), 5 * sin(current_time * .0172), 2.);
  lights[2].position = vec3(3 * cos(current_time * .12), 3 * sin(.03 * current_time), 0.5);

  // scene.nodes[gun].orientation = angleAxis((float32)(.02f * current_time), vec3(0.f, 0.f, 1.f));
  scene.nodes[tiger].orientation = angleAxis((float32)(.03f * current_time), vec3(0.f, 0.f, 1.f));
  scene.nodes[tiger2].position = vec3(.35 * cos(3 * current_time), .35 * sin(3 * current_time), 1);
  scene.nodes[tiger2].scale = vec3(.25);

  // build_transformation transfer child test
  static float32 last = (float32)current_time;
  static float32 time_of_reset = 0.f;
  static bool is_world = false;
  static bool first1 = true;

  if (last < current_time - 3)
  {
    Material_Descriptor *md = scene.get_modifiable_material_pointer_for(arm_test, 0);
    if (!is_world)
    {
      scene.set_parent(tiger2, grabbycube);

      scene.drop(tiger1);
      md->albedo.mod = vec4(1, 0, 0, 1);

      last = (float32)current_time;
      is_world = true;
      if (time_of_reset < current_time - 15)
      {
        set_message(s("resetting tiger to origin"), "", 3.f);
        scene.nodes[tiger1].position = vec3(0);
        scene.nodes[tiger1].scale = vec3(1);
        scene.nodes[tiger1].orientation = {};
        //scene.nodes[tiger1].import_basis = mat4(1);
        time_of_reset = (float32)current_time;
      }
    }
    else
    {
      md->albedo.mod = vec4(0, 1, 0, 1);

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

  renderer.set_camera(camera.pos, camera.dir);

  vec3 real_camera_pos = camera.pos;
  vec3 world_ray_dir = renderer.ray_from_screen_pixel(cursor_position);

  vec3 world_pos;
  // Node_Index node_for_icosphere = 2;
  // Node_Index hit = scene.ray_intersects_node(real_camera_pos, world_ray_dir, node_for_icosphere, world_pos);

  scene.nodes[sphere_raycast_test].position = world_pos;
  scene.nodes[sphere_raycast_test].scale = vec3(0.123);

  scene.nodes[memewater].position = vec4(vec2(10. * sin(0.01 * current_time)), -2., 0);

  // Material_Descriptor *watermat = get_material_pointer_for(&scene, memewater, 0);
  // watermat->uv_scale = vec2(1000);

  // pe->descriptor.position = vec3(5.f * sin(current_time), 5.f * cos(current_time), 1.0f);

  glm::mat4 m = scene.build_transformation(arm_test);
  quat orientation;
  vec3 translation;
  decompose(m, vec3(), orientation, translation, vec3(), vec4());
  orientation = conjugate(orientation);
  // pe->descriptor.physics_descriptor.gravity = vec3(0);
  // pe->descriptor.orientation = orientation;
  //// pe->descriptor.position = translation;

  // fire_emitter2(&renderer,&scene, &scene.particle_emitters[0], &scene.lights.lights[4], vec3(0), vec2(2, 2));

  // IMGUI_LOCK lock(this);
}

void test_spheres(Flat_Scene_Graph &scene)
{
  Material_Descriptor material;
  material.albedo = "color(1,1,1,1)";
  // material.emissive = "";
  // material.normal = "test_normal.png";
  material.roughness = "color(1,1,1,1)";
  material.metalness = "color(1,1,1,1)";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(1);
  material.casts_shadows = true;
  material.backface_culling = true;
  uint32 count = 4;
  uint32 zcount = 4;

  for (uint32 i = 0; i < count; ++i)
  {
    for (uint32 j = 0; j < zcount; ++j)
    {
      for (uint32 k = 0; k < count; ++k)
      {
        float roughness = mix(0.f, 1.f, float(i) / float(count - 1));
        float metalness = mix(0.f, 1.f, float(j) / float(zcount - 1));
        float color = mix(0.f, 1.f, float(k) / float(count - 1));

        material.albedo.mod = vec4(color, 1, 1, 1.0);
        material.roughness.mod = vec4(roughness);
        material.metalness.mod = vec4(metalness);
        material.uses_transparency = false;
        material.casts_shadows = true;

        material.frag_shader = "fragment_shader.frag";
        if (j == 1)
        {
          material.casts_shadows = false;
          material.uses_transparency = true;
          material.albedo.mod.a = mix(0.0f, 0.8f, float(k) / float(count - 1));
          material.roughness.mod = vec4(0);
          material.frag_shader = "refraction.frag";
          small_object_refraction_settings(&material.uniform_set);
          material.uniform_set.float32_uniforms["index_of_refraction"] = mix(1.001f, 1.30f, roughness);
          // material.uniform_set.float32_uniforms[""] = color;
        }
        if (j == 2)
        {
          material.casts_shadows = false;
          material.uses_transparency = true;
          material.albedo.mod.a = roughness;
          small_object_water_settings(&material.uniform_set);
          material.uniform_set.float32_uniforms["water_scale"] = mix(3.5f, 10.f, roughness);
          material.uniform_set.float32_uniforms["water_scale2"] = mix(0.5f, 3.f, roughness);
          // mix(1.5f, 10.f, roughness);
          material.roughness.mod = vec4(0);
          material.frag_shader = "water.frag";
        }

        Node_Index index = scene.add_aiscene("sphere-2.fbx", "Spherearray");
        Material_Index mi = scene.resource_manager->push_custom_material(&material);
        scene.nodes[scene.nodes[index].children[0]].model[0].second = mi;

        material.uniform_set.clear();

        Flat_Scene_Graph_Node *node = &scene.nodes[index];
        node->scale = vec3(0.5f);
        node->position = (node->scale * 2.f * vec3(i, k, j)) + vec3(0, 6, 1);
        scene.set_parent(index, NODE_NULL);
      }
    }
  }
}
