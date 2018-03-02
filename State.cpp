#include "State.h"
#include "Globals.h"
#include "Render.h"
#include "Third_party/imgui/imgui.h"
#include <atomic>
#include <memory>
#include <sstream>
#include <thread>

using namespace glm;

State::State(std::string name, SDL_Window *window, ivec2 window_size)
    : state_name(name), window(window), renderer(window, window_size)
{
  reset_mouse_delta();
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

  // TODO: add simple bounding colliders assignable to each node_ptr
  //       add a node_ptr to render_entity to point back to the owner
  //       octree for triangle data to optimize collision
  auto render_entities = scene.visit_nodes_st_start();
  renderer.set_render_entities(&render_entities);
  renderer.set_lights(scene.lights);
  renderer.clear_color = clear_color;
}

void State::reset_mouse_delta()
{
  uint32 mouse_state = SDL_GetMouseState(
      &last_seen_mouse_position.x, &last_seen_mouse_position.y);
}

void State::render(float64 t)
{
  prepare_renderer(t);
  renderer.render(t);
}

void State::handle_input(State **current_state,
    std::vector<State *> *available_states,
    const std::vector<SDL_Event> &new_events, bool block_kb, bool block_mouse)
{
  ImGuiIO &io = ImGui::GetIO();

  static std::vector<SDL_Event> dispatching;
  dispatching.clear();

  for (auto &e : new_events)
  {
    bool dispatch = true;

    if (block_kb && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP))
      dispatch = false;

    if (block_mouse &&
        (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP ||
            e.type == SDL_MOUSEWHEEL))
      dispatch = false;

    if (e.type == SDL_QUIT)
    {
      running = false;
      return;
    }
    else if (e.type == SDL_WINDOWEVENT)
    {
      if (e.window.event == SDL_WINDOWEVENT_RESIZED)
      {
        // resize
      }
      else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
               e.window.event == SDL_WINDOWEVENT_ENTER)
      { // dumping mouse delta prevents camera teleport on focus gain

        //// required, else clicking the title bar itself to gain focus
        //// makes SDL ignore the mouse entirely for some reason...
        // SDL_SetRelativeMouseMode(SDL_bool(false));
        // SDL_SetRelativeMouseMode(SDL_bool(true));
        reset_mouse_delta();
      }
    }
    if (dispatch)
    {
      dispatching.push_back(e);
      if (e.type == SDL_KEYUP)
      {
        if (e.key.keysym.sym == SDLK_F1)
        {
          *current_state = &*(*available_states)[0];
          return;
        }
        if (e.key.keysym.sym == SDLK_F2)
        {
          *current_state = &*(*available_states)[1];
          return;
        }
      }
    }
    else
    {
      set_message("", s("dropping event: ", e.type), 1.0f);
    }
  }
  handle_input_events(dispatching, block_kb, block_mouse);
}

void State::performance_output()
{
  std::stringstream s;
  const float64 report_delay = .1;
  const uint64 frame_count = renderer.frame_count;
  const uint64 frames_since_last_tick = frame_count - frames_at_last_tick;

  if (last_output + report_delay < current_time)
  {
#ifdef __linux__
    system("clear");
#elif _WIN32
    system("cls");
#endif
    frames_at_last_tick = frame_count;
    last_output = current_time;
    Uint64 current_frame_rate = (1.0 / report_delay) * frames_since_last_tick;
    s << PERF_TIMER.string_report();
    s << "FPS: " << current_frame_rate;
    s << "\nTotal FPS:" << (float64)frame_count / current_time;
    s << "\nRender Scale: " << renderer.get_render_scale();
    s << "\nDraw calls: " << renderer.draw_calls_last_frame;
    set_message("Performance output: ", s.str(), report_delay / 2);
    std::cout << get_messages() << std::endl;
  }
}
