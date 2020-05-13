#include "stdafx.h"
#include "Globals.h"
#include "SDL_Imgui_State.h"
#include "State.h"
#include "Json.h"
#include "Render.h"

using namespace glm;

State::State(std::string name, SDL_Window *window, ivec2 window_size)
    : state_name(name), window(window), renderer(window, window_size, name), scene(&resource_manager), gui_state(&IMGUI)
{
  
  thread = std::thread(State::_update, this);
  thread_launched = true;
}



void State::_update(State *this_state)
{
  while (this_state->running)
  {

    if (this_state->tick_block)
    {
      SDL_Delay(1);
      continue;
    }
    ASSERT(this_state->gui_state == &IMGUI);
    ASSERT(this_state->thread_launched);
    this_state->current_time += dt;

    if (this_state->paused)
    {
      const float64 real_time = get_real_time();
      float64 past_accum = this_state->paused_time_accumulator;
      float64 real_time_of_last_update = this_state->current_time + past_accum;
      float64 real_time_since_last_update = real_time - real_time_of_last_update;
      this_state->paused_time_accumulator += real_time_since_last_update;
      continue;
    }
    set_message("Updating state:", s(this_state->state_name).c_str(), 1.0f);
    this_state->handle_input_events();
    this_state->events_this_tick.clear();
    this_state->update();
    this_state->tick_block = true;
  }
}

void State::prepare_renderer(double t)
{
  // todo: frustrum cull using primitive bounding volumes
  // todo: determine which lights affect which objects


  // camera must be set before entities, or they get a 1 frame lag
  renderer.set_camera(camera.pos, camera.dir);

  // Traverse graph nodes and submit to renderer for packing:

  // TODO: add simple bounding colliders assignable to each Node_Index
  //       add a Node_Index to render_entity to point back to the owner
  //       octree for triangle data to optimize collision
  std::vector<Render_Entity> render_entities = scene.visit_nodes_start();
  if (scene.draw_collision_octree)
  {
    std::vector<Render_Entity> octree = scene.collision_octree.get_render_entities(&scene);
    render_entities.insert(render_entities.end(), octree.begin(), octree.end());
  }
  renderer.set_render_entities(&render_entities);
  renderer.render_instances.clear();
  scene.push_particle_emitters_for_renderer(&renderer);
  scene.set_lights_for_renderer(&renderer);
}

void State::render(float64 t)
{
  if (!window)
    return;
  prepare_renderer(t);
  renderer.render(t);
}

void State::performance_output()
{
  const float report_frequency_in_seconds = 1.f;
  std::stringstream s;
  const uint64 frame_count = renderer.frame_count;

  static float current_frame_rate = 0;

  // s << PERF_TIMER.string_report();
  s << "Current FPS: " << current_frame_rate;
  s << "\nAverage FPS:" << (float64)frame_count / current_time;
  // s << "\nRender Scale: " << renderer.get_render_scale();
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

IMGUI_LOCK::IMGUI_LOCK(State *s) : lock(IMGUI_MUTEX) {}
