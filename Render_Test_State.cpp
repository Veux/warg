#include "stdafx.h"
#include "Render_Test_State.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "State.h"
#include "Animation_Utilities.h"
using namespace glm;
bool spawn_test_spheres(Scene_Graph &scene);

void world_water_settings(Uniform_Set_Descriptor *dst)
{

  //dst->float32_uniforms["index_of_refraction"] = 1.15f;
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
}

void small_object_water_settings(Uniform_Set_Descriptor *dst)
{
  //dst->float32_uniforms["index_of_refraction"] = 1.15f;
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
  //dst->float32_uniforms["index_of_refraction"] = 1.15f;
  dst->float32_uniforms["refraction_offset_factor"] = 0.201;
}

void spawn_water(Scene_Graph *scene, vec3 scale, vec3 pos) {}

void spawn_ground(Scene_Graph *scene)
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

void spawn_gun(Scene_Graph *scene, vec3 position)
{
  Node_Index gun = scene->add_aiscene_new("Cerberus/cerberus-warg.FBX", "gun");
  scene->nodes[gun].position = position;
  scene->nodes[gun].scale = vec3(1);
}

void spawn_planets(Scene_Graph *scene, vec3 pos)
{
  Material_Descriptor material_star;

  material_star.emissive.source = "white";
  material_star.albedo.mod = vec4(0, 0, 0, 1);
  material_star.emissive.mod = vec4(2.f, 2.f, .3f, 0.f);
  material_star.frag_shader = "water.frag";
  small_object_water_settings(&material_star.uniform_set);

  Node_Index star = scene->add_aiscene_new("smoothsphere.fbx", "star");
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

void spawn_grabbyarm(Scene_Graph *scene, vec3 position)
{
  Material_Descriptor material;
  Node_Index grabbycube = scene->add_mesh(cube, "grabbycube", &material);
  Node_Index arm_test = scene->add_mesh(cube, "grabbyarm", &material);
  Node_Index shoulder_joint = scene->new_node("grabbyarm shoulder");
  scene->set_parent(shoulder_joint, grabbycube);
  scene->set_parent(arm_test, shoulder_joint);

  Node_Index tiger = scene->add_aiscene_new("tiger/tiger.fbx", "grabbytiger");
}

void spawn_compass(Scene_Graph *scene)
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

void spawn_map(Scene_Graph *scene)
{
  Blades_Edge map(*scene);
}

void spawn_test_triangle(Scene_Graph *scene)
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
  material2.translucent_pass = true;
  material2.backface_culling = false;
  Node_Index triangle = scene->add_mesh("triangle", &md, &material2);

  material2.albedo.mod = vec4(.3, .3, .3, .3);
  material2.emissive.mod = vec4(0);

  Node_Index cb = scene->add_mesh(cube, "aabb", &material2);
}
Render_Test_State::Render_Test_State(std::string name, SDL_Window *window, ivec2 window_size)
    : State(name, window, window_size)
{

  scene.initialize_lighting(
      "Environment_Maps/Shiodome_Stairs/irradiance.hdr", "Environment_Maps/Ice_Lake/irradiance.hdr");

  // scene.initialize_lighting("Environment_Maps/GrandCanyon_C_YumaPoint/GCanyon_C_YumaPoint_8k.jpg",
  //  "Environment_Maps/GrandCanyon_C_YumaPoint/irradiance.hdr");

  // scene.initialize_lighting("Assets/Textures/black.png",
  //  "Assets/Textures/black.png");

  camera.phi = .25;
  camera.theta = -1.5f * half_pi<float32>();
  camera.pos = vec3(3.3, 2.3, 1.4);

  //rach = scene.add_aiscene_new("racharound/rach5.fbx");
  //scene.set_wow_asset_import_transformation(rach);


  static Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";
  material.translucent_pass = true;
  material.albedo.mod = vec4(1, 1, 1, 0.5);


  vec3 radius = vec3(0.5f) * vec3(.39, 0.30, 1.61); // avg human in meters
  static Node_Index ghost_mesh = scene.add_mesh(cube, "human_size_mesh", &material);
  scene.nodes[ghost_mesh].position = vec3(0,0,0);
  scene.nodes[ghost_mesh].scale = radius * vec3(2);



  terrain.init(this, vec3(0, 0, -2.5), 55, ivec2(HEIGHTMAP_RESOLUTION));
  // spawn_ground(&scene);
  // spawn_gun(&scene, vec3(0));
  // spawn_planets(&scene, vec3(12, 6, 3));
  // spawn_grabbyarm(&scene,vec3(0,0,1));
  // spawn_test_triangle(&scene);
  // spawn_compass(&scene);
  //spawn_test_spheres(scene);

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
  light0->brightness = 2555.00000;
  light0->color = vec3(1.00);
  light0->attenuation = vec3(1.00000, 0.22000, 0.00000);
  light0->ambient = 0.00401;
  light0->radius = 22.00000;
  light0->cone_angle = 5.00000;
  light0->type = Light_Type::spot;
  light0->casts_shadows = 1;
  light0->shadow_blur_iterations = 3;
  light0->shadow_blur_radius = 0.30050;
  light0->shadow_near_plane = 963.00006;
  light0->shadow_far_plane = 1127.00000;
  light0->max_variance = 0.000003;
  light0->shadow_fov = 0.06230;
  light0->shadow_map_resolution = ivec2(2048, 2048);

  Light *light1 = &scene.lights.lights[1];
  light1->position = vec3(-3.12653, 3.90190, 2.00000);
  light1->direction = vec3(0.00000, 0.00000, 0.00000);
  light1->brightness = 51.50000;
  light1->color = vec3(1.00000, 0.99999, 0.99999);
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
  light1->max_variance = 0.000003;
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
      //  set_message("mouse grab event", "", 1.0f);

        mouse_grabbed = true;
        last_grabbed_cursor_position = cursor_position;
        mouse_relative_mode = true;
      }
      if (!IMGUI.context->IO.WantCaptureMouse)
      {
        IMGUI.ignore_all_input = true;
      }

      draw_cursor = false;/*
      set_message("mouse delta: ", s(mouse_delta.x, " ", mouse_delta.y), 1.0f);*/
      camera.theta += mouse_delta.x * MOUSE_X_SENS;
      camera.phi += mouse_delta.y * MOUSE_Y_SENS;/*
      set_message("mouse is grabbed", "", 1.0f);*/
    }
    else
    { // not holding button
      //set_message("mouse is free", "", 1.0f);
      if (mouse_grabbed)
      { // first unhold
        //set_message("mouse release event", "", 1.0f);
        mouse_grabbed = false;
        //set_message("mouse warp:",
        //    s("from:", cursor_position.x, " ", cursor_position.y, " to:", last_grabbed_cursor_position.x, " ",
        //        last_grabbed_cursor_position.y),
        //    1.0f);
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

void update_grabbyarm(Scene_Graph *scene, float64 current_time)
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

void update_planets(Scene_Graph *scene, float64 current_time)
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

void update_test_triangle(Scene_Graph *scene)
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
  material->translucent_pass = true;
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


  // update_planets(&scene, current_time);
  scene.lights.lights[1].position = vec3(5 * cos(current_time * .0172), 5 * sin(current_time * .0172), 2.);
  renderer.set_camera(camera.pos, camera.dir);
  terrain.apply_geometry_to_octree_if_necessary(&scene);

  if (rach != NODE_NULL)
  {
    Scene_Graph_Node *rach_ptr = &scene.nodes[rach];
    if (rach_ptr->animation_state_pool_index != NODE_NULL)
    {
      Skeletal_Animation_State *anim_ptr =
          &scene.resource_manager->animation_state_pool[rach_ptr->animation_state_pool_index];

      if (!anim_ptr->paused)
        anim_ptr->time += anim_ptr->time_scale * dt;
    }
  }
}

void anim_name_sort(std::vector<std::pair<std::string, std::string>> &result)
{
  std::sort(
      result.begin(), result.end(), [](std::pair<std::string, std::string> &l, std::pair<std::string, std::string> &r) {
        std::string &a = l.first;

        uint32 a_num = 0;
        for (size_t i = 0; i < a.size(); ++i)
        {
          const char *c = &a[i];
          if (!isdigit(*c))
          {
            continue;
          }

          a_num = atoi(c);
          break;
        }

        std::string &b = r.first;

        uint32 b_num = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
          const char *c = &b[i];
          if (!isdigit(*c))
          {
            continue;
          }
          b_num = atoi(c);
          break;
        }

        return a_num < b_num;
      });

  for (auto &name : result)
  {
    if (name.first.size() > 15)
    {
      name.second = name.first.substr(name.first.size() - 15);
    }
    else
    {
      name.second = name.first;
    }
  }
}

void Render_Test_State::draw_gui()
{
  IMGUI_LOCK lock(this);
  ImGui::Begin("test state");
  scene.draw_imgui(state_name);

  if (imgui_this_tick)
  {
    if (ImGui::Button("Liquid Surface Config"))
    {
      terrain.window_open = !terrain.window_open;
    }
    terrain.run(this);

    if (rach != NODE_NULL)
    {

      Scene_Graph_Node *rach_ptr = &scene.nodes[rach];
      if (rach_ptr->animation_state_pool_index != NODE_NULL)
      {
        Skeletal_Animation_State *anim_ptr =
            &scene.resource_manager->animation_state_pool[rach_ptr->animation_state_pool_index];

        Skeletal_Animation_Set *anim_set = &scene.resource_manager->animation_set_pool[anim_ptr->animation_set_index];

        ImGui::Checkbox("paused", &anim_ptr->paused);
        ImGui::SliderFloat("Speed", &anim_ptr->time_scale, 0.f, 2.f);
        ImGui::DragFloat("Time", &anim_ptr->time, 0.005f, 0.f, 0.f, "%.5f");
        ImGui::DragFloat("TimeScale", &anim_ptr->time_scale, 0.005f, 0.f, 0.f, "%.5f");

        std::string bone_shorthand = anim_ptr->name_of_manually_posed_bone;
        if (bone_shorthand.size() > 15)
        {
          bone_shorthand =
              anim_ptr->name_of_manually_posed_bone.substr(anim_ptr->name_of_manually_posed_bone.size() - 15);
        }

        const char* current_posed_bone = bone_shorthand.c_str();
        if (ImGui::BeginCombo("BonePose", current_posed_bone))
        {

          //.second is shorthand
          std::vector<std::pair<std::string, std::string>> vec;
          for (auto &bone : anim_ptr->final_bone_transforms)
          {
            vec.push_back({bone.first, ""});
          }
          anim_name_sort(vec);

          const size_t count = anim_ptr->final_bone_transforms.size();
          for (auto &bone : vec)
          {
            const char *this_bone_name = bone.second.c_str();
            if (ImGui::Selectable(this_bone_name, current_posed_bone))
            {
              anim_ptr->name_of_manually_posed_bone = bone.first;
              anim_ptr->bone_pose_overrides[bone.first] = anim_ptr->time;
            }
            if (anim_ptr->name_of_manually_posed_bone == bone.first)
              ImGui::SetItemDefaultFocus();
          }
          ImGui::EndCombo();
        }

        if (anim_ptr->name_of_manually_posed_bone != "")
        {
          ASSERT(anim_ptr->final_bone_transforms.contains(anim_ptr->name_of_manually_posed_bone));
          float32 *pose_time = &anim_ptr->bone_pose_overrides[anim_ptr->name_of_manually_posed_bone];
          ImGui::DragFloat("Bone_Pose", pose_time, 0.003f, 0.f, 0.0f, "%.5f");
        }
        if (ImGui::Button("Clear poses"))
        {
          anim_ptr->bone_pose_overrides.clear();
          anim_ptr->name_of_manually_posed_bone = "";
        }

        const char *currently_playing =
            anim_set->animation_set[anim_ptr->currently_playing_animation].animation_name.c_str();

        if (ImGui::BeginCombo("Anim", currently_playing))
        {
          const size_t count = anim_set->animation_set.size();
          for (size_t i = 0; i < count; i++)
          {
            const char *this_list_name = anim_set->animation_set[i].animation_name.c_str();
            bool is_selected = strcmp(currently_playing, this_list_name) == 0;

            if (ImGui::Selectable(this_list_name, is_selected))
            {
              anim_ptr->currently_playing_animation = i;
              anim_ptr->time = 0;
            }

            if (is_selected)
              ImGui::SetItemDefaultFocus();
          }
          ImGui::EndCombo();
        }
      }
    }
  }
  ImGui::End();
}

bool spawn_test_spheres(Scene_Graph &scene)
{
  //temp so we can wait for the sphere to load
  Node_Index test = scene.add_aiscene_new("smoothsphere.fbx", "spheretest", true);
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
  uint32 icount = 7;
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
        material.translucent_pass = false;
        material.casts_shadows = true;
        material.frag_shader = "fragment_shader.frag";
        material.require_self_depth = false;

        
        if (j == 1)
        { //refraction
          material.casts_shadows = false;
          material.translucent_pass = true;
          material.albedo.mod.a = mix(0.0f, 0.4f, float(k) / float(kcount - 1));
          material.roughness.mod = vec4(0);
          material.frag_shader = "refraction.frag";
          material.require_self_depth = false;
          small_object_refraction_settings(&material.uniform_set);
          material.ior = mix(1.001f, 1.30f, roughness);
          // material.uniform_set.float32_uniforms[""] = color;
        }
        if (j == 2)
        {//water
          material.casts_shadows = false;
          material.translucent_pass = true;
          material.albedo.mod.a = roughness;
          material.metalness.mod.a = 0.8f;
          material.require_self_depth = true;
          small_object_water_settings(&material.uniform_set);
          material.uniform_set.float32_uniforms["water_scale"] = mix(3.5f, 10.f, roughness);
          material.uniform_set.float32_uniforms["water_scale2"] = mix(0.5f, 3.f, roughness);
          // mix(1.5f, 10.f, roughness);
          material.roughness.mod = vec4(roughness);
          material.frag_shader = "water.frag";
        }

        Node_Index index = scene.add_aiscene_new("smoothsphere.fbx", "Spherearray");
        Material_Index mi = scene.resource_manager->push_material(&material);
        scene.nodes[scene.nodes[index].children[0]].model[0].second = mi;
        scene.nodes[scene.nodes[index].children[0]].name = s("sphere ", i, " ", j, " ", k);
        material.uniform_set.clear();

        Scene_Graph_Node *node = &scene.nodes[index];
        node->scale = vec3(0.5f);
        node->position = (node->scale * 2.f * vec3(i, k, j)) + vec3(0, 6, 1);
        scene.set_parent(index, NODE_NULL);
      }
    }
  }
  return true;
}

Frostbolt_Effect::Frostbolt_Effect(State *state, uint32 light_index)
{
  Material_Descriptor mat;
  mat.albedo.source = "white";
  mat.albedo.mod = vec4(0.0, 0., 0., 0.);
  mat.emissive.source = "frostbolteffect.png";
  mat.emissive.mod = vec4(.01, .25, .5, 0);
  mat.roughness.mod.x = .0f;
  mat.metalness.mod.x = 0.5f;
  mat.translucent_pass = true;
  mat.frag_shader = "emission.frag";
  billboard_spawn_source = state->scene.add_mesh(cube, "frostbolt billboard source", &mat);
  Scene_Graph_Node *node = &state->scene.nodes[billboard_spawn_source];
  node->scale = vec3(0.0001, 3, 3);

  crystal = state->scene.add_aiscene_new("sphere-1.fbx", "frostbolt crystal");
  node = &state->scene.nodes[crystal];
  node->scale = vec3(.5);
  Node_Index crystalchild = state->scene.nodes[crystal].children[0];
  Material_Descriptor *material = state->scene.get_modifiable_material_pointer_for(crystalchild, 0);
  material->albedo.source = "test_normal.png";
  material->albedo.mod = vec4(1, 1, 1, .315);
  material->emissive.source = "test_normal.png";
  material->emissive.mod = vec4(.15, 0.125, 0.25, 0);
  material->roughness.source = "white";
  material->roughness.mod.x = 0.15f;
  material->translucent_pass = true;

  node = &state->scene.nodes[crystalchild];
  // node->scale_vertex = vec3()

  // state->scene.set_parent(billboard,crystalchild);

  this->light_index = light_index;

  Light *light6 = &state->scene.lights.lights[light_index];
  light6->position = vec3(6.33112, 6.33112, 6.62005);
  light6->direction = vec3(0.00000, 0.00000, 0.00000);
  light6->brightness = 8111.00000;
  light6->color = vec3(.05, 0.67895, 0.94803);
  light6->attenuation = vec3(1.00000, 16.76000, 33.28000);
  light6->ambient = 0.00001;
  light6->radius = 0.40000;
  light6->cone_angle = 0.15000;
  light6->type = Light_Type::omnidirectional;
  light6->casts_shadows = 0;
  light6->shadow_blur_iterations = 2;
  light6->shadow_blur_radius = 0.50000;
  light6->shadow_near_plane = 0.10000;
  light6->shadow_far_plane = 100.00000;
  light6->max_variance = 0.00000;
  light6->shadow_fov = 1.57080;
  light6->shadow_map_resolution = ivec2(1024, 1024);
}

bool Frostbolt_Effect::update(State *owning_state, vec3 target)
{
  Scene_Graph_Node *node = &owning_state->scene.nodes[crystal];

  if (length(target - node->position) < 0.15f)
  {
    // node->position = vec3(35,35,5);//*random_3D_unit_vector(0,two_pi<float32>(),pi<float32>(),two_pi<float32>());
    // return false;
  }
  float32 sintime = 0.5 + 0.5 * sin(6 * owning_state->current_time);
  vec3 dir = normalize(target - node->position);
  vec3 pos = node->position + (dt * speed) * dir;
  // pos = vec3(1);
  node->position = pos;
  // node->orientation = glm::toQuat(lookAt(node->position,target,vec3(0,0,1)));
  Light *light = &owning_state->scene.lights.lights[light_index];

  light->position = node->position;
  light->brightness = random_between(4611., 6511.);
  light->radius = 0.f;
  // node->scale_vertex = vec3(0.4f) + 0.6f * vec3(sintime);
  // node->oriented_scale = vec3(.5) + 10.5f * dir;

  // for billboarding - might want to do this elsewhere since we have to undo the camera matrix here...
  vec3 v = normalize(owning_state->camera.dir);
  vec3 axis = normalize(cross(v, vec3(0, 0, 1)));
  float32 angle2 = atan2(v.y, v.x);
  quat o = angleAxis(-owning_state->camera.phi, axis) * angleAxis(angle2, vec3(0, 0, 1));

  // node->orientation = angleAxis(random_between(0.f, two_pi<float32>()), -v) * node->orientation;

  uint32 size = billboards.size();
  for (uint32 i = 0; i < size; ++i)
  {
    Scene_Graph_Node *node = &owning_state->scene.nodes[billboards[i].first];

    // node->velocity = node->velocity + dt * vec3(0, 0, -9.8);
    node->velocity = .99f * node->velocity;
    node->position = node->position + dt * node->velocity;
    float fade_t = random_between(0.97f, 0.99f);
    node->scale = fade_t * node->scale;
    node->scale.x = 0.0001;
    node->orientation = o;

    float32 invert_dir = -1;
    if (i % 2 == 0)
    {
      invert_dir = 1;
    }
    billboards[i].second = billboards[i].second + dt;
    float32 rotation_offset = billboards[i].second;
    float32 sintime2 = wrap_to_range(rotation_offset + 1.1f * owning_state->current_time, 0, two_pi<float32>());
    node->orientation = angleAxis(sintime2, -v) * node->orientation;

    if (length(node->scale) < 0.25f)
    {
      node->exists = false;
      billboards.erase(billboards.begin() + i);
      i = i - 1;
      size = size - 1;
    }
  }

  vec3 randdir =
      vec3(random_between(0.9f, 1.1f) * dir.x, random_between(0.9f, 1.1f) * dir.y, random_between(0.9f, 1.1f) * dir.z);

  Node_Index new_particle = owning_state->scene.new_node("frostbolt particle");
  owning_state->scene.nodes[new_particle] = owning_state->scene.nodes[billboard_spawn_source];

  rotation = rotation + 22 * dt;
  billboards.push_back({new_particle, rotation});
  node = &owning_state->scene.nodes[new_particle];
  node->orientation = o; // was not set before
  node->position = pos;  // - .5f*randdir;
  node->velocity = .5f * -randdir;
  float32 sintime2 = wrap_to_range(rotation + 3.1f * owning_state->current_time, 0, two_pi<float32>());
  node->orientation = angleAxis(sintime2, -v) * node->orientation;
  return true;
}

//////////////////////////////////////////////////////////////////////////

Frostbolt_Effect_2::Frostbolt_Effect_2(State *state, uint32 light_index)
{
  //////////////////////////////////////////////////
  //////////////////////////////////////////////////
  // crystal node
  crystal_node = state->scene.add_aiscene_new("sphere-1.fbx", "frostbolt crystal");
  Scene_Graph_Node *crystal_node_ptr = &state->scene.nodes[crystal_node];
  crystal_node_ptr->position = position;
  crystal_node_ptr->scale = vec3(.5);
  Node_Index crystalchild = state->scene.nodes[crystal_node].children[0];
  Material_Descriptor *crystal_material = state->scene.get_modifiable_material_pointer_for(crystalchild, 0);

  crystal_material->albedo.source = "Snow006_2K_Color.jpg";
  crystal_material->albedo.mod = vec4(.10, .10, 1.0, .40);
  crystal_material->roughness.source = "Snow006_2K_Roughness.jpg";
  crystal_material->roughness.mod = vec4(.10, 1.0, 1.0, 1.0);
  crystal_material->normal.source = "test_normal.png";
  crystal_material->normal.mod = vec4(1.0, 1.0, 1.0, 1.0);
  crystal_material->ambient_occlusion.source = "Snow006_2K_AmbientOcclusion.jpg.jpg";
  crystal_material->ambient_occlusion.mod = vec4(1.0, 1.0, 1.0, 1.0);
  crystal_material->translucent_pass = true;

  //////////////////////////////////////////////////
  //////////////////////////////////////////////////
  // light
  this->light_index = light_index;
  Light *light6 = &state->scene.lights.lights[6];
  light6->position = vec3(-4.89832, -18.04270, 2.22558);
  light6->direction = vec3(0.00000, 0.00000, 0.00000);
  light6->brightness = 4753.80762;
  light6->color = vec3(0.05000, 0.38085, 0.94803);
  light6->attenuation = vec3(1.00000, 16.76000, 33.28000);
  light6->ambient = 0.00001;
  light6->radius = 0.00000;
  light6->cone_angle = 0.15000;
  light6->type = Light_Type::omnidirectional;
  light6->casts_shadows = 0;
  light6->shadow_blur_iterations = 2;
  light6->shadow_blur_radius = 0.50000;
  light6->shadow_near_plane = 0.10000;
  light6->shadow_far_plane = 100.00000;
  light6->max_variance = 0.00000;
  light6->shadow_fov = 1.57080;
  light6->shadow_map_resolution = ivec2(1024, 1024);

  //////////////////////////////////////////////////
  //////////////////////////////////////////////////
  // trail emitter:
  Particle_Emission_Method_Descriptor pemd_trail;
  Particle_Physics_Method_Descriptor ppmd_trail;
  pemd_trail.type = stream;
  pemd_trail.particles_per_second = .9999000001f / dt;
  pemd_trail.minimum_time_to_live = 3;
  pemd_trail.initial_scale = vec3(3.f);
  pemd_trail.billboarding = true;
  pemd_trail.billboard_rotation_velocity = dt;
  // trail physics
  ppmd_trail.type = simple;
  ppmd_trail.gravity = vec3(0);
  ppmd_trail.friction = vec3(0.99f);
  ppmd_trail.size_multiply_uniform_min = 0.97f;
  ppmd_trail.size_multiply_uniform_max = 0.99f;
  // trail material
  Material_Descriptor trail_material;
  trail_material.albedo.source = "white";
  trail_material.albedo.mod = vec4(0.0, 0., 0., 0.);
  trail_material.emissive.source = "frostbolteffect.png";
  trail_material.emissive.mod = vec4(.3, .3, .3, 0);
  trail_material.roughness.mod.x = .0f;
  trail_material.metalness.mod.x = 0.5f;
  trail_material.translucent_pass = true;
  trail_material.backface_culling = true;
  trail_material.depth_mask = false;
  trail_material.vertex_shader = "instance.vert";
  trail_material.frag_shader = "emission.frag";
  trail_node = state->scene.add_mesh(cube, "frostbolt2 trail_node", &trail_material);
  Scene_Graph_Node *trail_node_ptr = &state->scene.nodes[trail_node];
  Mesh_Index trail_mesh_i = trail_node_ptr->model[0].first;
  Material_Index trail_mat_i = trail_node_ptr->model[0].second;
  // trail particle emitter
  Particle_Emitter_Descriptor ped_trail;
  ped_trail.emission_descriptor = pemd_trail;
  ped_trail.physics_descriptor = ppmd_trail;
  state->scene.particle_emitters.emplace_back(ped_trail, trail_mesh_i, trail_mat_i);
  stream_particle_emitter_index = state->scene.particle_emitters.size() - 1;

  //////////////////////////////////////////////////
  //////////////////////////////////////////////////
  // impact emitter:
  Particle_Emission_Method_Descriptor pemd_impact;
  pemd_impact.allow_colliding_spawns = false;
  pemd_impact.type = explosion;
  pemd_impact.explosion_particle_count = 155;
  pemd_impact.billboarding = false;
  pemd_impact.inherit_velocity = 0.5f;
  pemd_impact.hammersley_sphere = true;
  pemd_impact.low_discrepency_position_variance = true;
  pemd_impact.power = .50001f;
  pemd_impact.minimum_time_to_live = 5.05;
  pemd_impact.extra_time_to_live_variance = 1.4;
  pemd_impact.initial_position_variance = vec3(0.0f);
  pemd_impact.initial_velocity_variance = vec3(.20f);
  pemd_impact.billboard_rotation_velocity = 3.f * dt;
  pemd_impact.initial_billboard_rotation_velocity_variance = 3.f * dt;
  pemd_impact.initial_scale = vec3(.05f);
  pemd_impact.initial_extra_scale_variance = vec3(0.1f);
  pemd_impact.initial_extra_scale_uniform_variance = .1f;
  pemd_impact.impulse_center_offset_min = vec3(0.f);
  pemd_impact.impulse_center_offset_max = vec3(0.0f);
  pemd_impact.hammersley_radius = .1f;
  pemd_impact.initial_velocity = vec3(0);
  // impact physics
  Particle_Physics_Method_Descriptor ppmd_impact;
  ppmd_impact.type = wind;
  ppmd_impact.wind_intensity = 0.f;
  ppmd_impact.bounce_min = .5099528f;
  ppmd_impact.bounce_max = .8099935f;
  ppmd_impact.size_multiply_uniform_min = 1.0f;
  ppmd_impact.size_multiply_uniform_max = 1.0f;
  ppmd_impact.die_when_size_smaller_than = vec3(0.0f);
  ppmd_impact.friction = vec3(.9);
  ppmd_impact.drag = .25f;
  ppmd_impact.gravity = vec3(0, 0, -9.8);
  ppmd_impact.billboard_rotation_time_factor = 1.1f;
  // impact material
  explosion_node = state->scene.add_aiscene_new("sphere-1.fbx", "frostbolt2 explosion source");
  Node_Index explosionchild = state->scene.nodes[explosion_node].children[0];
  Material_Descriptor *explosion_material = state->scene.get_modifiable_material_pointer_for(explosionchild, 0);
  *explosion_material = Material_Descriptor();
  explosion_material->albedo.source = "Snow006_2K_Color.jpg";
  explosion_material->albedo.mod = vec4(.10, .10, 1.0, .40);
  explosion_material->roughness.source = "Snow006_2K_Roughness.jpg";
  explosion_material->roughness.mod = vec4(.10, 1.0, 1.0, 1.0);
  explosion_material->normal.source = "test_normal.png";
  explosion_material->normal.mod = vec4(1.0, 1.0, 1.0, 1.0);
  explosion_material->ambient_occlusion.source = "Snow006_2K_AmbientOcclusion.jpg.jpg";
  explosion_material->ambient_occlusion.mod = vec4(1.0, 1.0, 1.0, 1.0);

  // explosion_material->emissive.source = "frostbolt_explosion.png";
  // explosion_material->emissive.mod = vec4(.01, .25, .5, 0);
  explosion_material->roughness.mod.x = .1f;
  explosion_material->metalness.mod.x = 0.0f;
  explosion_material->emissive.mod = vec4(0);
  explosion_material->translucent_pass = true;
  explosion_material->backface_culling = false;
  explosion_material->discard_on_alpha = false;
  explosion_material->vertex_shader = "instance.vert";
  explosion_material->frag_shader = "fragment_shader.frag";
  Scene_Graph_Node *explosion_node_ptr = &state->scene.nodes[explosionchild];
  explosion_node_ptr->visible = false;
  Mesh_Index explosion_mesh_i = explosion_node_ptr->model[0].first;
  Material_Index explosion_mat_i = explosion_node_ptr->model[0].second;
  // impact particle emitter
  Particle_Emitter_Descriptor ped_impact;
  ped_impact.emission_descriptor = pemd_impact;
  ped_impact.physics_descriptor = ppmd_impact;
  ped_impact.static_geometry_collision = true;
  ped_impact.static_octree = &state->scene.collision_octree;
  state->scene.particle_emitters.emplace_back(ped_impact, explosion_mesh_i, explosion_mat_i);
  impact_particle_emitter_index = state->scene.particle_emitters.size() - 1;
}

bool Frostbolt_Effect_2::update(State *owning_state, vec3 target)
{
  Scene_Graph_Node *node = &owning_state->scene.nodes[crystal_node];
  Particle_Emitter *stream_emitter = nullptr;
  Particle_Emitter *impact_emitter = nullptr;
  if (owning_state->scene.particle_emitters.size() > stream_particle_emitter_index)
  {
    stream_emitter = &owning_state->scene.particle_emitters[stream_particle_emitter_index];
  }
  if (owning_state->scene.particle_emitters.size() > impact_particle_emitter_index)
  {
    impact_emitter = &owning_state->scene.particle_emitters[impact_particle_emitter_index];
  }
  if (!stream_emitter || !impact_emitter)
  {
    if (stream_emitter)
    {
      stream_emitter->clear();
    }
    if (impact_emitter)
    {
      impact_emitter->clear();
    }
    return true;
  }

  bool dst_reached = false;
  if (length(target - position) < (10.f * dt) * speed)
  {
    dst_reached = true;
  }
  float32 sintime = 0.5f + 0.5f * float32(sin(6 * owning_state->current_time));
  vec3 dir = normalize(target - position);
  vec3 pos = position + (dt * speed) * dir;
  node->velocity = speed * dir;
  node->position = pos;
  position = pos;
  Light *light = &owning_state->scene.lights.lights[light_index];

  light->position = node->position + 0.01f * (owning_state->camera.pos - pos);
  light->brightness = random_between(4611., 5611.);
  light->radius = 0.f;

  // rotation_inversion = -1.f*rotation_inversion;
  rotation = rotation + 22 * dt;

  vec3 randdir =
      vec3(random_between(0.9f, 1.1f) * dir.x, random_between(0.9f, 1.1f) * dir.y, random_between(0.9f, 1.1f) * dir.z);

  float32 sintime2 = wrap_to_range(rotation + 3.1f * owning_state->current_time, 0, two_pi<float32>());
  stream_emitter->descriptor.emission_descriptor.billboard_initial_angle = sintime2;
  stream_emitter->descriptor.emission_descriptor.initial_velocity = 0.5f * -randdir;
  stream_emitter->descriptor.position = pos;

  if (dst_reached)
  {
    impact_emitter->descriptor.position = pos; // target + (dt * dir);
    impact_emitter->descriptor.velocity = node->velocity;
    impact_emitter->descriptor.emission_descriptor.boom_t = 1 * dt;
    // impact_emitter->descriptor.emission_descriptor.generate_particles = true;
    stream_emitter->descriptor.emission_descriptor.generate_particles = false;
  }
  else
  {
    // impact_emitter->descriptor.emission_descriptor.generate_particles = false;
    stream_emitter->descriptor.emission_descriptor.generate_particles = true;
  }

  // could apply an impulse to all particles in the stream emitter based on distance from impact

  stream_emitter->update(owning_state->renderer.projection, owning_state->renderer.camera, dt);
  impact_emitter->update(owning_state->renderer.projection, owning_state->renderer.camera, dt);
  return !dst_reached;
}
