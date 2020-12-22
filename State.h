#pragma once
#include "SDL_Imgui_State.h"
#include "Render.h"
#include "Scene_Graph.h"
#include <array>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>
struct Camera
{
  float32 theta = 0;
  float32 phi = half_pi<float32>() * 0.5f;
  float32 zoom = 4;
  vec3 pos;
  vec3 dir;
};

struct IMGUI_LOCK
{
  IMGUI_LOCK(State *s);
  std::lock_guard<std::mutex> lock;
};

struct State
{
  State() : scene(&resource_manager) {}
  State(std::string name, SDL_Window *window, ivec2 window_size);


  std::string state_name;

  // timestep
  static void _update(State *this_state);
  std::thread thread;
  bool paused = false;
  float64 paused_time_accumulator = 0; // how far the sim is behind 'real' process time
  float64 last_performance_output = 0;
  float64 current_time = 0;
  uint64 tick = 0;
  std::atomic<bool> tick_block = true;
  std::atomic<bool> running = true;
  bool thread_launched = false;
  bool imgui_this_tick = false;
  SDL_Imgui_State *gui_state = nullptr;

  // update
  virtual void update()
  {
    if (!imgui_this_tick)
      return;

    IMGUI_LOCK lock(this);
    ImGui::Begin(s("Hello from:update():", state_name).c_str(), &test_window);
    ImGui::Text(s("Hello from:update():", state_name).c_str());
    ImGui::End();
    // opengl calls no longer allowed in State::update
    // instead, put them all in State::prepare_renderer() or State::draw_gui
    // you can hold opengl state objects in State:: - all the constructors should
    // allow for no-op blank constructors
    // you just want to use them only inside prepare_renderer(), or State::draw_gui, or in a custom
    // function that you call inside prepare_renderer()
    // if you need the state to somehow specify rendering objects, for example:
    // you need game code to specify a Texture, have the state
    // create the Texture_Descriptor struct and fill that out, and then create the
    // Texture t = Texture_Descriptor td in one of the above functions
    // you may use Texture() (default constructor) in update()
  };

  bool test_window = true;
  virtual void draw_gui()
  {
    IMGUI_LOCK lock(this);
    ImGui::Begin(s("Hello from:draw_gui():", state_name).c_str(), &test_window);
    ImGui::Text(s("Hello from:draw_gui():", state_name).c_str());
    ImGui::End();
  }

  // data
  Resource_Manager resource_manager;
  Flat_Scene_Graph scene;

  // utility

  // perf
  void performance_output();
  uint64 frames_at_last_report = 0;

  // rendering
  SDL_Window *window = nullptr;
  virtual void render(float64 t) final;
  Renderer renderer;
  Camera camera;
  virtual void prepare_renderer(double remaining_dt);
  bool draws_imgui = true;

  // input:
  bool recieves_input = false;
  virtual void handle_input_events(){};
  std::vector<SDL_Event> events_this_tick;
  std::vector<uint8> key_state;
  std::vector<SDL_Event>* imgui_event_accumulator; //events only cleared after imgui pass
  ivec2 cursor_position;
  ivec2 mouse_delta;
  uint32 mouse_state;
  uint32 previous_mouse_state = 0;
  ivec2 request_cursor_warp_to = ivec2(-1);
  ivec2 last_grabbed_cursor_position = ivec2(0, 0);
  bool request_block_all_imgui = false;
  bool free_cam = false;
  bool mouse_grabbed = false;
  bool draw_cursor = true;
  bool mouse_relative_mode = false;
};
