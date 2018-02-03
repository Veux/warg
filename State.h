#pragma once
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
struct State
{
  State(std::string name, SDL_Window *window, ivec2 window_size);

  virtual void render(float64 t) final;
  virtual void update() = 0;
  virtual void handle_input(State **current_state,
                            std::vector<State *> available_states) = 0;
  float64 current_time = 0;
  bool paused = true;
  float64 paused_time_accumulator =
      0; // how far the sim is behind 'real' process time
  float64 last_output = 0;
  uint64 frames_at_last_tick = 0;
  void reset_mouse_delta();
  bool running = true;
  void performance_output();
  std::string state_name;
  Render renderer;
  Scene_Graph scene;
protected:
  void prepare_renderer(double t);
  ivec2 mouse_position = ivec2(0, 0);
  uint32 previous_mouse_state = 0;
  bool free_cam = false;
  Camera cam;
  vec3 clear_color = vec3(0);
private:
  SDL_Window *window = nullptr;
};
