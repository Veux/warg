#include "stdafx.h"
#include "Render_Test_State.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "State.h"
#include "Animation_Utilities.h"
using namespace glm;
bool spawn_test_spheres(Flat_Scene_Graph &scene);

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

void spawn_water(Flat_Scene_Graph *scene, vec3 scale, vec3 pos)
{

  Mesh_Descriptor mesh;
  mesh.name = "generated water grid";
  mesh.mesh_data = generate_grid(ivec2(256));
  Material_Descriptor material;
  material.emissive.mod = vec4(0, 0, 0.005, 1);
  material.albedo.mod = vec4(.054, .135, .159, .998);
  //material.uses_transparency = true;
  material.uv_scale = vec2(1);
  material.roughness.mod = vec4(0.25);
  material.metalness.mod = vec4(0.84);
  //material.frag_shader = "water.frag";
  world_water_settings(&material.uniform_set);
  Node_Index memewater = scene->add_mesh("water",&mesh, &material);
  scene->nodes[memewater].scale = scale;
  scene->nodes[memewater].position = pos;
}

void spawn_ground(Flat_Scene_Graph *scene)
{
  Material_Descriptor material;
  material.albedo = "grass_albedo.png";
  material.normal = "ground1_normal.png";
  material.roughness.mod = vec4(.7);
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(32);
  material.casts_shadows = true;
  material.backface_culling = false;
  Node_Index ground = scene->add_mesh(cube, "world_cube", &material);
  scene->nodes[ground].position = {0.0f, 0.0f, -150.f};
  scene->nodes[ground].scale = {135.0f, 135.0f, 300.f};
  material.uv_scale = vec2(312);
  material.albedo.mod = vec4(2);
  Node_Index underwater = scene->add_mesh(cube, "underwater", &material);
  scene->nodes[underwater].position = {0.0f, 0.0f, -30.5f};
  scene->nodes[underwater].scale = {6500.0f, 6500.0f, 1.f};
  material.albedo.mod = vec4(1);
}

void spawn_gun(Flat_Scene_Graph *scene, vec3 position)
{
  Node_Index gun = scene->add_aiscene("Cerberus/cerberus-warg.FBX", "gun");
  scene->nodes[gun].position = position;
  scene->nodes[gun].scale = vec3(1);
}

void spawn_planets(Flat_Scene_Graph *scene, vec3 pos)
{
  Material_Descriptor material_star;

  material_star.emissive.source = "white";
  material_star.albedo.mod = vec4(0, 0, 0, 1);
  material_star.emissive.mod = vec4(2.f, 2.f, .3f, 0.f);
  material_star.frag_shader = "water.frag";
  small_object_water_settings(&material_star.uniform_set);

  Node_Index star = scene->add_aiscene("smoothsphere.fbx", "star");
  Node_Index spheremodel = scene->nodes[star].children[0];
  Material_Descriptor *mdp = scene->get_modifiable_material_pointer_for(spheremodel, 0);
  *mdp = material_star;

  Material_Descriptor planet;
  Node_Index cube_planet = scene->add_mesh(cube, "planet", &planet);
  Material_Descriptor moon;
  Node_Index cube_moon = scene->add_mesh(cube, "moon", &moon);
  scene->set_parent(cube_moon, cube_planet);
  scene->set_parent(cube_planet, star);
  scene->nodes[star].position = pos;
  scene->nodes[star].orientation = angleAxis(0.3f, vec3(1, 0, 0));

  const float32 star_scale = 1.0f;
  const float32 planet_scale = 0.35;
  const float32 moon_scale = 0.25;
  scene->nodes[star].scale = vec3(star_scale);
  scene->nodes[cube_planet].scale = vec3(planet_scale);
  scene->nodes[cube_moon].scale = vec3(moon_scale);
}

void spawn_grabbyarm(Flat_Scene_Graph *scene, vec3 position)
{
  Material_Descriptor material;
  Node_Index grabbycube = scene->add_mesh(cube, "grabbycube", &material);
  Node_Index arm_test = scene->add_mesh(cube, "grabbyarm", &material);
  Node_Index shoulder_joint = scene->new_node("grabbyarm shoulder");
  scene->set_parent(shoulder_joint, grabbycube);
  scene->set_parent(arm_test, shoulder_joint);

  Node_Index tiger = scene->add_aiscene("tiger/tiger.fbx", "grabbytiger");
}

void spawn_compass(Flat_Scene_Graph *scene)
{
  Node_Index root = scene->new_node("compass");
  Material_Descriptor material;

  material.emissive.source = "white";
  material.frag_shader = "emission.frag";
  material.emissive.mod = vec4(0);
  material.emissive.mod.r = 2.0f;
  Node_Index xaxis = scene->add_mesh(cube, "xaxis", &material);
  material.emissive.mod.r = .0f;
  material.emissive.mod.g = 2.0f;
  Node_Index yaxis = scene->add_mesh(cube, "yaxis", &material);
  material.emissive.mod.g = 0.0f;
  material.emissive.mod.b = 2.0f;
  Node_Index zaxis = scene->add_mesh(cube, "zaxis", &material);
  scene->set_parent(xaxis, root);
  scene->set_parent(yaxis, root);
  scene->set_parent(zaxis, root);

  scene->nodes[xaxis].scale = vec3(1, 0.025, 0.025);
  scene->nodes[xaxis].position = vec3(0.5, 0.0125, 0.0125);

  scene->nodes[yaxis].scale = vec3(0.025, 1, 0.025);
  scene->nodes[yaxis].position = vec3(0.0125, 0.5, 0.0125);

  scene->nodes[zaxis].scale = vec3(0.025, 0.025, 1);
  scene->nodes[zaxis].position = vec3(0.0125, 0.0125, 0.5);
}

void spawn_map(Flat_Scene_Graph *scene)
{
  Blades_Edge map(*scene);
}

void spawn_test_triangle(Flat_Scene_Graph *scene)
{

  Material_Descriptor material;

  material.emissive.source = "white";
  material.frag_shader = "emission.frag";
  material.emissive.mod = vec4(0);
  material.emissive.mod.r = 2.0f;

  Mesh_Descriptor md;
  Node_Index a = scene->add_mesh(cube, "a", &material);
  material.emissive.mod.r = .0f;
  material.emissive.mod.g = 2.0f;
  Node_Index b = scene->add_mesh(cube, "b", &material);
  material.emissive.mod.g = 0.0f;
  material.emissive.mod.b = 2.0f;
  Node_Index c = scene->add_mesh(cube, "c", &material);

  scene->nodes[a].scale = vec3(0.025);
  scene->nodes[b].scale = vec3(0.025);
  scene->nodes[c].scale = vec3(0.025);

  scene->nodes[a].position = random_3D_unit_vector();
  scene->nodes[b].position = random_3D_unit_vector();
  scene->nodes[c].position = random_3D_unit_vector();

  Material_Descriptor material2;

  material.albedo.source = "white";
  material2.albedo.mod = vec4(.2, .2, .2, .2);
  material.emissive.mod = vec4(0);
  material2.uses_transparency = true;
  material2.blending = true;
  material2.backface_culling = false;
  Node_Index triangle = scene->add_mesh("triangle", &md, &material2);

  material2.albedo.mod = vec4(.3, .3, .3, .3);
  material2.emissive.mod = vec4(0);

  Node_Index cb = scene->add_mesh(cube, "aabb", &material2);
}
Render_Test_State::Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{

  //scene.initialize_lighting(
  //    "Environment_Maps/Frozen_Waterfall/irradiance.hdr", "Environment_Maps/Frozen_Waterfall/irradiance.hdr");

    scene.initialize_lighting("Environment_Maps/GrandCanyon_C_YumaPoint/GCanyon_C_YumaPoint_8k.jpg",
      "Environment_Maps/GrandCanyon_C_YumaPoint/irradiance.hdr");

  // scene.initialize_lighting("Assets/Textures/black.png",
  //  "Assets/Textures/black.png");

  camera.phi = .25;
  camera.theta = -1.5f * half_pi<float32>();
  camera.pos = vec3(3.3, 2.3, 1.4);

  spawn_water(&scene, vec3(25, 25, 3), vec3(0, 0, -2));
  // spawn_ground(&scene);
  // spawn_gun(&scene, vec3(0));
  spawn_planets(&scene, vec3(12, 6, 3));
  // spawn_grabbyarm(&scene,vec3(0,0,1));
 // spawn_test_triangle(&scene);
 // spawn_compass(&scene);
  spawn_test_spheres(scene);

  // spawn_map(&scene);

  // scene.particle_emitters.push_back({});
  // scene.particle_emitters.push_back({});
  // scene.particle_emitters.push_back({});
  // scene.particle_emitters.push_back({});
  // Material_Descriptor material;
  // Particle_Emitter *pe = &scene.particle_emitters.back();
  // material.vertex_shader = "instance.vert";
  // material.frag_shader = "emission.frag";
  // material.emissive.mod = vec4(0.25f, .25f, .35f, 1.f);
  // small_object_water_settings(&material.uniform_set);
  // Node_Index particle_node = scene.add_mesh(cube, "snow particle", &material);
  // Mesh_Index mesh_index = scene.nodes[particle_node].model[0].first;
  // Material_Index material_index = scene.nodes[particle_node].model[0].second;
  // scene.nodes[particle_node].visible = false;
  // scene.particle_emitters[0].mesh_index = mesh_index;
  // scene.particle_emitters[0].material_index = material_index;
  // scene.particle_emitters[1].mesh_index = mesh_index;
  // scene.particle_emitters[1].material_index = material_index;
  // scene.particle_emitters[2].mesh_index = mesh_index;
  // scene.particle_emitters[2].material_index = material_index;
  // scene.particle_emitters[3].mesh_index = mesh_index;
  // scene.particle_emitters[3].material_index = material_index;

  auto &lights = scene.lights.lights;
  scene.lights.light_count = 2;

  Light *light0 = &scene.lights.lights[0];
  light0->position = vec3(804.00000, -414.00000, 401.00000);
  light0->direction = vec3(0.00000, 0.00000, 0.00000);
  light0->brightness = 500.00000;
  light0->color = vec3(1.00);
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

  Light *light1 = &scene.lights.lights[1];
  light1->position = vec3(-2.68201, 4.21981, 2.00000);
  light1->direction = vec3(0.00000, 0.00000, 0.00000);
  light1->brightness = 51.50000;
  light1->color = vec3(1.00000, 1.00000, 1.00000);
  light1->attenuation = vec3(1.00000, 0.22000, 0.00000);
  light1->ambient = 0.00000;
  light1->radius = 0.10000;
  light1->cone_angle = 0.15100;
  light1->type = Light_Type::spot;
  light1->casts_shadows = 1;
  light1->shadow_blur_iterations = 4;
  light1->shadow_blur_radius = 2.12000;
  light1->shadow_near_plane = 0.50000;
  light1->shadow_far_plane = 200.00000;
  light1->max_variance = 0.00000;
  light1->shadow_fov = 1.57080;
  light1->shadow_map_resolution = ivec2(1024, 1024);
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
    const float32 lower = -half_pi<float32>() + 100 * epsilon<float32>();
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

void update_grabbyarm(Flat_Scene_Graph *scene, float64 current_time)
{
  // build_transformation transfer child test
  static float32 last = (float32)current_time;
  static float32 time_of_reset = 0.f;
  static bool is_world = false;
  Node_Index grabbycube = scene->find_by_name(NODE_NULL, "grabbycube");
  Node_Index shoulder_joint = scene->find_by_name(grabbycube, "grabbyarm shoulder");
  Node_Index arm_test = scene->find_by_name(shoulder_joint, "grabbyarm");
  Node_Index grabbytiger = scene->find_by_name(NODE_NULL, "grabbytiger");
  if (grabbytiger == NODE_NULL)
  {
    grabbytiger = scene->find_by_name(arm_test, "grabbytiger");
  }

  if (last < current_time - 3)
  {
    Material_Descriptor *md = scene->get_modifiable_material_pointer_for(arm_test, 0);
    if (!is_world)
    {

      scene->drop(grabbytiger);
      md->albedo.mod = vec4(1, 0, 0, 1);

      last = (float32)current_time;
      is_world = true;
      if (time_of_reset < current_time - 15)
      {
        set_message(s("resetting tiger to origin"), "", 3.f);
        scene->nodes[grabbytiger].position = vec3(0);
        scene->nodes[grabbytiger].scale = vec3(1);
        scene->nodes[grabbytiger].orientation = {};
        // scene.nodes[tiger1].import_basis = mat4(1);
        time_of_reset = (float32)current_time;
      }
    }
    else
    {
      md->albedo.mod = vec4(0, 1, 0, 1);
      scene->grab(arm_test, grabbytiger);
      is_world = false;
      last = (float32)current_time;
    }
  }

  const float32 theta = (float32)sin(current_time / 12.f);
  float cube_diameter = 1;

  const float32 height = 1.25;
  scene->nodes[grabbycube].scale_vertex = vec3(1, 1, 2); // the size of the cube - affects *only* this object
  scene->nodes[grabbycube].scale = vec3(cube_diameter);  // the scale of the node - affects all children
  scene->nodes[grabbycube].position = vec3(0, 0, height + 3.f * (0.5 + 0.5 * (sin(current_time))));
  scene->nodes[grabbycube].orientation = angleAxis(theta, vec3(cos(.25f * current_time), sin(.25f * current_time), 0));

  scene->nodes[shoulder_joint].position = {0.50f * cube_diameter, 0.0f, cube_diameter};
  scene->nodes[shoulder_joint].orientation = angleAxis(20.f * (float32)sin(current_time / 20.f), vec3(1, 0, 0));

  const float32 arm_radius = 0.25f;
  scene->nodes[arm_test].scale_vertex = {arm_radius, arm_radius, 1.5f};
  scene->nodes[arm_test].position = {0.5f * arm_radius, 0.0f, -0.75f};
}

void update_planets(Flat_Scene_Graph *scene, float64 current_time)
{
  Node_Index cube_star = scene->find_by_name(NODE_NULL, "star");
  Node_Index cube_planet = scene->find_by_name(cube_star, "planet");
  Node_Index cube_moon = scene->find_by_name(cube_planet, "moon");

  const float32 planet_distance = 4;
  const float32 planet_year = 5;
  const float32 planet_day = 1;
  scene->nodes[cube_planet].position =
      planet_distance * vec3(cos(current_time / planet_year), sin(current_time / planet_year), 0);
  const float32 angle = wrap_to_range((float32)current_time, 0.0f, 2.0f * pi<float32>());
  scene->nodes[cube_planet].orientation = angleAxis((float32)current_time / planet_day, vec3(0, 0, 1));

  const float32 moon_distance = 1.5;
  const float32 moon_year = .75;
  const float32 moon_day = .1;
  scene->nodes[cube_moon].position =
      moon_distance * vec3(cos(current_time / moon_year), sin(current_time / moon_year), 0);
  scene->nodes[cube_moon].orientation = angleAxis((float32)current_time / moon_day, vec3(0, 0, 1));
}

void update_test_triangle(Flat_Scene_Graph *scene)
{
  Node_Index triangle = scene->find_by_name(NODE_NULL, "triangle");
  ASSERT(triangle != NODE_NULL);

  Mesh_Descriptor md;

  Node_Index na = scene->find_by_name(NODE_NULL, "a");
  Node_Index nb = scene->find_by_name(NODE_NULL, "b");
  Node_Index nc = scene->find_by_name(NODE_NULL, "c");

  vec3 a = scene->nodes[na].position;
  vec3 b = scene->nodes[nb].position;
  vec3 c = scene->nodes[nc].position;

  add_triangle(a, b, c, md.mesh_data);

  Mesh_Index meshi = scene->nodes[triangle].model[0].first;
  Material_Index mati = scene->nodes[triangle].model[0].second;

  scene->resource_manager->mesh_pool[meshi] = md;
  Material_Descriptor *material = scene->get_modifiable_material_pointer_for(triangle, 0);

  material->albedo.source = "white";
  material->emissive.source = "white";
  material->albedo.mod = vec4(.3, .3, .3, .3);
  material->uses_transparency = true;
  material->blending = true;
  material->backface_culling = false;

  scene->collision_octree.clear();
  scene->collision_octree.push(&md);

  Node_Index cuber = scene->find_by_name(NODE_NULL, "aabb");
  ASSERT(cuber != NODE_NULL);
  AABB aabb(scene->nodes[cuber].position);
  push_aabb(aabb, scene->nodes[cuber].position + (0.5f * scene->nodes[cuber].scale));
  push_aabb(aabb, scene->nodes[cuber].position - (0.5f * scene->nodes[cuber].scale));

  uint32 counter = 0;
  std::vector<Triangle_Normal> touching = scene->collision_octree.test_all(aabb, &counter);

  material->emissive.mod = vec4(0);
  if (touching.size())
  {
    material->emissive.mod = vec4(1.3, .3, .3, .3);
  }
}
void Render_Test_State::update()
{


  if(painter.textures.size() && painter.textures[0].texture != nullptr)
  {
    Node_Index water_node = scene.find_by_name(NODE_NULL,"water");
    Flat_Scene_Graph_Node* node = &scene.nodes[water_node];
    Material_Index mi = node->model[0].second;
    Material* mat = &scene.resource_manager->material_pool[mi];
    mat->displacement = painter.textures[painter.selected_texture];
  }


  // update_grabbyarm(&scene, current_time);
  //update_planets(&scene, current_time);
  scene.lights.lights[1].position = vec3(5 * cos(current_time * .0172), 5 * sin(current_time * .0172), 2.);
  renderer.set_camera(camera.pos, camera.dir);
  //update_test_triangle(&scene);

  // static vec3 wind_dir;
  // if (fract(sin(current_time)) > .5)
  //  wind_dir = vec3(.575, .575, .325) * random_3D_unit_vector(0, glm::two_pi<float32>(), 0.9f, 1.0f);

  // scene.particle_emitters[1].descriptor.position = vec3(0, 0, 25);
  // scene.particle_emitters[1].descriptor.emission_descriptor.initial_position_variance = vec3(70, 70, 0);
  // scene.particle_emitters[1].descriptor.emission_descriptor.particles_per_second = 255;
  // scene.particle_emitters[1].descriptor.emission_descriptor.minimum_time_to_live = 15;
  // scene.particle_emitters[1].descriptor.emission_descriptor.initial_scale = vec3(.15f, .15f, .14f);
  //// scene.particle_emitters[1].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  // scene.particle_emitters[1].descriptor.physics_descriptor.type = wind;
  // scene.particle_emitters[1].descriptor.physics_descriptor.direction = wind_dir;
  // scene.particle_emitters[1].descriptor.physics_descriptor.octree = &scene.collision_octree;
  // scene.particle_emitters[1].update(renderer.projection, renderer.camera, dt);
  // scene.particle_emitters[1].descriptor.physics_descriptor.intensity = random_between(21.f, 55.f);
  // scene.particle_emitters[1].descriptor.physics_descriptor.bounce_min = 0.02;
  // scene.particle_emitters[1].descriptor.physics_descriptor.bounce_max = 0.15;

  // scene.particle_emitters[2].descriptor.position = vec3(1, 0, 25);
  // scene.particle_emitters[2].descriptor.emission_descriptor.initial_position_variance = vec3(70, 70, 0);
  // scene.particle_emitters[2].descriptor.emission_descriptor.particles_per_second = 255;
  // scene.particle_emitters[2].descriptor.emission_descriptor.minimum_time_to_live = 15;
  // scene.particle_emitters[2].descriptor.emission_descriptor.initial_scale = vec3(.15f, .15f, .14f);
  //// scene.particle_emitters[2].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  // scene.particle_emitters[2].descriptor.physics_descriptor.type = wind;
  // scene.particle_emitters[2].descriptor.physics_descriptor.direction = wind_dir;
  // scene.particle_emitters[2].descriptor.physics_descriptor.octree = &scene.collision_octree;
  // scene.particle_emitters[2].update(renderer.projection, renderer.camera, dt);
  // scene.particle_emitters[2].descriptor.physics_descriptor.intensity = random_between(21.f, 55.f);
  // scene.particle_emitters[2].descriptor.physics_descriptor.bounce_min = 0.02;
  // scene.particle_emitters[2].descriptor.physics_descriptor.bounce_max = 0.15;

  // scene.particle_emitters[3].descriptor.position = vec3(0, 1, 25);
  // scene.particle_emitters[3].descriptor.emission_descriptor.initial_position_variance = vec3(70, 70, 0);
  // scene.particle_emitters[3].descriptor.emission_descriptor.particles_per_second = 255;
  // scene.particle_emitters[3].descriptor.emission_descriptor.minimum_time_to_live = 15;
  // scene.particle_emitters[3].descriptor.emission_descriptor.initial_scale = vec3(.15f, .15f, .14f);
  //// scene.particle_emitters[3].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  // scene.particle_emitters[3].descriptor.physics_descriptor.type = wind;
  // scene.particle_emitters[3].descriptor.physics_descriptor.direction = wind_dir;
  // scene.particle_emitters[3].descriptor.physics_descriptor.octree = &scene.collision_octree;
  // scene.particle_emitters[3].update(renderer.projection, renderer.camera, dt);
  // scene.particle_emitters[3].descriptor.physics_descriptor.intensity = random_between(21.f, 55.f);
  // scene.particle_emitters[3].descriptor.physics_descriptor.bounce_min = 0.02;
  // scene.particle_emitters[3].descriptor.physics_descriptor.bounce_max = 0.15;

  // scene.particle_emitters[0].descriptor.position = vec3(.5, .5, 25);
  // scene.particle_emitters[0].descriptor.emission_descriptor.initial_position_variance = vec3(70, 70, 0);
  // scene.particle_emitters[0].descriptor.emission_descriptor.particles_per_second = 255;
  // scene.particle_emitters[0].descriptor.emission_descriptor.minimum_time_to_live = 15;
  // scene.particle_emitters[0].descriptor.emission_descriptor.initial_scale = vec3(.15f, .15f, .14f);
  //// scene.particle_emitters[0].descriptor.emission_descriptor.initial_extra_scale_variance = vec3(1.5f,1.5f,.14f);
  // scene.particle_emitters[0].descriptor.physics_descriptor.type = wind;
  // scene.particle_emitters[0].descriptor.physics_descriptor.direction = wind_dir;
  // scene.particle_emitters[0].descriptor.physics_descriptor.octree = &scene.collision_octree;
  // scene.particle_emitters[0].update(renderer.projection, renderer.camera, dt);
  // scene.particle_emitters[0].descriptor.physics_descriptor.intensity = random_between(21.f, 55.f);
  // scene.particle_emitters[0].descriptor.physics_descriptor.bounce_min = 0.02;
  // scene.particle_emitters[0].descriptor.physics_descriptor.bounce_max = 0.15;
}

void Render_Test_State::draw_gui()
{
  IMGUI_LOCK lock(this);
  scene.draw_imgui(state_name);

  if (imgui_this_tick)
  {
    if (ImGui::Button("Texture Painter"))
    {
      painter.window_open = !painter.window_open;
    }
    painter.run(imgui_event_accumulator);

    //static std::vector<char> buf = {};
    //uint32 size = buf.size();
    //ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
    //ImGui::InputTextMultiline("blah",&buf[0],size,ImVec2(512,512),flags);
  }
}

bool spawn_test_spheres(Flat_Scene_Graph &scene)
{
  Node_Index test = scene.add_aiscene("smoothsphere.fbx", "spheretest", false);
  if (test == NODE_NULL)
  {
    return false;
  }
  scene.delete_node(test);

  Material_Descriptor material;
  // material.emissive = "";
  // material.normal = "test_normal.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.uv_scale = vec2(1);
  material.casts_shadows = true;
  material.backface_culling = true;
  material.roughness.source = "white";
  material.metalness.source = "white";
  uint32 kcount = 2;
  uint32 icount = 10;
  uint32 jcount = 4;

  for (uint32 i = 0; i < icount; ++i)
  {
    for (uint32 j = 0; j < jcount; ++j)
    {
      for (uint32 k = 0; k < kcount; ++k)
      {
        float roughness = mix(0.f, 1.f, float(i) / float(icount - 1));
        float metalness = mix(0.f, 1.f, float(j) / float(jcount - 1));
        float color = mix(0.f, 1.f, float(k) / float(kcount - 1));

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
          material.albedo.mod.a = mix(0.0f, 0.8f, float(k) / float(kcount - 1));
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

        Node_Index index = scene.add_aiscene("smoothsphere.fbx", "Spherearray");
        Material_Index mi = scene.resource_manager->push_custom_material(&material);
        scene.nodes[scene.nodes[index].children[0]].model[0].second = mi;
        scene.nodes[scene.nodes[index].children[0]].name = s("sphere ", i, " ", j, " ", k);
        material.uniform_set.clear();

        Flat_Scene_Graph_Node *node = &scene.nodes[index];
        node->scale = vec3(0.5f);
        node->position = (node->scale * 2.f * vec3(i, k, j)) + vec3(0, 6, 1);
        scene.set_parent(index, NODE_NULL);
      }
    }
  }
  return true;
}
