#include "State.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "Third_party/imgui/imgui.h"
#include <atomic>
#include <memory>
#include <sstream>
#include <thread>

using namespace glm;

State::State(std::string name, SDL_Window *window, ivec2 window_size)
    : state_name(name), window(window), renderer(window, window_size, name), scene(&GL_ENABLED_RESOURCE_MANAGER)
{
  reset_mouse_delta();
  save_graph_on_exit = true;
  scene_graph_json_filename = s(ROOT_PATH, name, ".json");
  std::string str = read_file(scene_graph_json_filename.c_str());
  try
  {
    json j = json::parse(str);
    Light_Array lights = j.at("Lights");
    scene.deserialize(j.at("Node Serialization"));
  }
  catch (std::exception &e)
  {
    set_message("Exception loading scene graph json:", e.what(), 55.0f);
    set_message("JSON:\n", str.c_str(), 55.0f);
  }
}

State::~State()
{
  if (save_graph_on_exit)
  {
    json j = scene;
    std::string str = pretty_dump(j);
    set_message("state destructor saved scene graph: ", str, 1.0f);
    std::fstream file(scene_graph_json_filename, std::ios::out | std::ios::trunc);
    file.write(str.c_str(), str.size());
  }
}

void State::prepare_renderer(double t)
{
  // todo: frustrum cull using primitive bounding volumes
  // todo: determine which lights affect which objects

  /*Light diameter guideline for deferred rendering (not yet used)
  Distance 	Constant 	Linear 	Quadratic
7 	1.0 	0.7 	1.8
13 	1.0 	0.35 	0.44
20 	1.0 	0.22 	0.20
32 	1.0 	0.14 	0.07
50 	1.0 	0.09 	0.032
65 	1.0 	0.07 	0.017
100 	1.0 	0.045 	0.0075
160 	1.0 	0.027 	0.0028
200 	1.0 	0.022 	0.0019
325 	1.0 	0.014 	0.0007
600 	1.0 	0.007 	0.0002
3250 	1.0 	0.0014 	0.000007

  */

  // camera must be set before entities, or they get a 1 frame lag
  renderer.set_camera(cam.pos, cam.dir);

  // Traverse graph nodes and submit to renderer for packing:

  // TODO: add simple bounding colliders assignable to each Node_Index
  //       add a Node_Index to render_entity to point back to the owner
  //       octree for triangle data to optimize collision
  auto render_entities = scene.visit_nodes_client_start();
  renderer.set_render_entities(&render_entities);
  renderer.set_lights(scene.lights);
  renderer.clear_color = clear_color;
}

void State::reset_mouse_delta()
{
  uint32 mouse_state = SDL_GetMouseState(&last_seen_mouse_position.x, &last_seen_mouse_position.y);
}

void State::render(float64 t)
{
  prepare_renderer(t);
  renderer.render(t);
}

void State::handle_input(State **current_state, std::vector<State *> *available_states, std::vector<SDL_Event> *input,
    bool block_kb, bool block_mouse)
{
  std::vector<SDL_Event> game_events;
  for (auto &e : *input)
  {
    if (e.type == SDL_QUIT)
    {
      running = false;
      return;
    }
    if (e.type == SDL_WINDOWEVENT)
    {
      if (e.window.event == SDL_WINDOWEVENT_RESIZED)
      {
        // resize
        continue;
      }
      else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || e.window.event == SDL_WINDOWEVENT_ENTER)
      { // dumping mouse delta prevents camera teleport on focus gain

        //// required, else clicking the title bar itself to gain focus
        //// makes SDL ignore the mouse entirely for some reason...
        // SDL_SetRelativeMouseMode(SDL_bool(false));
        // SDL_SetRelativeMouseMode(SDL_bool(true));
        reset_mouse_delta();
        continue;
      }
    }
    if (e.type == SDL_KEYUP)
    {
      if (e.key.keysym.sym == SDLK_F1)
      {
        *current_state = &*(*available_states)[0];
        if ((*current_state)->free_cam)
          SDL_SetRelativeMouseMode(SDL_bool(true));
        else
        {
          SDL_SetRelativeMouseMode(SDL_bool(false));
          SDL_WarpMouseInWindow(
              nullptr, (*current_state)->last_seen_mouse_position.x, (*current_state)->last_seen_mouse_position.y);
        }
        ivec2 trash;
        SDL_GetRelativeMouseState(&trash.x, &trash.y);
        //(*current_state)->reset_mouse_delta();
        (*current_state)->renderer.previous_color_target_missing = true;
        return;
      }
      if (e.key.keysym.sym == SDLK_F2)
      {
        *current_state = &*(*available_states)[1];
        if ((*current_state)->free_cam)
          SDL_SetRelativeMouseMode(SDL_bool(true));
        else
        {
          SDL_SetRelativeMouseMode(SDL_bool(false));
          SDL_WarpMouseInWindow(
              nullptr, (*current_state)->last_seen_mouse_position.x, (*current_state)->last_seen_mouse_position.y);
        }
        ivec2 trash;
        SDL_GetRelativeMouseState(&trash.x, &trash.y);
        //(*current_state)->reset_mouse_delta();
        (*current_state)->renderer.previous_color_target_missing = true;
        return;
      }
    }

    bool gui_owns_event = false;

    if (block_kb && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP))
      gui_owns_event = true;

    if (block_mouse && (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEWHEEL))
      gui_owns_event = true;

    if (free_cam)
    {
      gui_owns_event = false;
    }
    if (gui_owns_event)
    {
      continue;
    }
    game_events.push_back(e);
  }
  handle_input_events(game_events, block_kb, block_mouse);
}

void State::performance_output()
{
  const float report_frequency_in_seconds = 1.f;
  std::stringstream s;
  const uint64 frame_count = renderer.frame_count;

  static float current_frame_rate = 0;

  s << PERF_TIMER.string_report();
  s << "Current FPS: " << current_frame_rate;
  s << "\nAverage FPS:" << (float64)frame_count / current_time;
  s << "\nRender Scale: " << renderer.get_render_scale();
  set_message("Performance output: ", s.str(), report_frequency_in_seconds / 2);

  if (last_performance_output + report_frequency_in_seconds < current_time)
  {
#ifdef __linux__
    // system("clear");
#elif _WIN32
    // system("cls");
#endif
    const uint64 frames_since_last_report = frame_count - frames_at_last_report;

    current_frame_rate = (1.0f / report_frequency_in_seconds) * (float)frames_since_last_report;
    frames_at_last_report = frame_count;
    last_performance_output = current_time;
    // std::cout << get_messages() << std::endl;
  }
}
